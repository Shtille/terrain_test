#include "mgnTrMercatorTree.h"

#include "mgnTrConstants.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorNodePool.h"
#include "mgnTrMercatorTileMesh.h"
#include "mgnTrMercatorRenderable.h"
#include "mgnTrMercatorService.h"

#include "../mgnTrIcon.h"

#include "mgnTrMercatorTaskTexture.h"
#include "mgnTrMercatorTaskHeightmap.h"
#include "mgnTrMercatorTaskLabels.h"
#include "mgnTrMercatorTaskTextureLabels.h"
#include "mgnTrMercatorTaskIcons.h"

#include "mgnMdTerrainView.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <assert.h>

namespace {
    // The more detail coefficient is, the less detalization is required
    const float kGeoDetail = 6.0f;
    const float kTexDetail = 1.0f;
}

namespace mgn {
    namespace terrain {

        MercatorTree::MercatorTree(graphics::Renderer * renderer,
            graphics::Shader * shader, graphics::Shader * billboard_shader, const Font * font,
            math::Frustum * frustum, mgnMdTerrainView * terrain_view,
            MercatorProvider * provider, const mgnMdWorldPosition * gps_position)
        : renderer_(renderer)
        , shader_(shader)
        , billboard_shader_(billboard_shader)
        , font_(font)
        , frustum_(frustum)
        , terrain_view_(terrain_view)
        , provider_(provider)
        , gps_position_(gps_position)
        , grid_size_(17)
        , frame_counter_(0)
        , preprocess_(!IsCollection())
        , lod_freeze_(false)
        , tree_freeze_(false)
        {
            root_ = new MercatorNode(this);

            const int kPoolSize = 50;
            node_pool_ = new MercatorNodePool(kPoolSize);

            tile_ = new MercatorTileMesh(renderer, grid_size_);
            service_ = new MercatorService();

            const float kPlanetRadius = 6371000.0f;
            const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
            terrain_view->LocalToPixelDistance(kPlanetRadius, earth_radius_, kMSM);

#ifdef DEBUG
            debug_info_.num_nodes = 1;
            debug_info_.num_map_tiles = 0;
            debug_info_.maximum_achieved_lod = 0;
#endif
        }
        MercatorTree::~MercatorTree()
        {
            delete root_;
            root_ = NULL;

            delete node_pool_;
            node_pool_ = NULL;

            delete tile_;
            tile_ = NULL;

            // Clear texture caches
            for (IconTextureCache::iterator it = icon_texture_cache_.begin(); it != icon_texture_cache_.end(); ++it)
            {
                graphics::Texture * texture = it->second;
                if (texture)
                    renderer_->DeleteTexture(texture);
            }
            icon_texture_cache_.clear();

            for (ShieldTextureCache::iterator it = shield_texture_cache_.begin(); it != shield_texture_cache_.end(); ++it)
            {
                graphics::Texture * texture = it->second;
                if (texture)
                    renderer_->DeleteTexture(texture);
            }
            shield_texture_cache_.clear();

            // Delete default textures
            if (default_albedo_texture_)
            {
                renderer_->DeleteTexture(default_albedo_texture_);
                default_albedo_texture_ = NULL;
            }
            if (default_heightmap_texture_)
            {
                renderer_->DeleteTexture(default_heightmap_texture_);
                default_heightmap_texture_ = NULL;
            }

            service_->StopService();
            delete service_; // should be deleted after faces are done
            service_ = NULL;
        }
        bool MercatorTree::Initialize(float fovy_in_radians, int screen_height)
        {
            // Create tile mesh
            tile_->Create();
            if (!tile_->MakeRenderable())
                return false;
            // Create default textures
            {
                const int kAlbedoData = (0xff << 16) | (0xff << 8) | 0xff;
                renderer_->CreateTextureFromData(default_albedo_texture_, 1, 1,
                    graphics::Image::Format::kRGB8, graphics::Texture::Filter::kPoint,
                    (unsigned char*)&kAlbedoData);
                if (!default_albedo_texture_) return false;
            }
            {
                const float kHeightMin = mgn::terrain::GetHeightMin();
                const float kHeightRange = mgn::terrain::GetHeightRange();
                const float kDefaultHeight = 0.0f;
                // Pack float value into two bytes, order should be opposite to one in shader
                float norm_height = (kDefaultHeight - kHeightMin) / kHeightRange;
                unsigned short short_height = static_cast<unsigned short>(norm_height * 65535.0f);
                unsigned char r = static_cast<unsigned char>(short_height / 256);
                unsigned char g = static_cast<unsigned char>(short_height % 256);
                const int kHeightData = (g << 8) | r;
                renderer_->CreateTextureFromData(default_heightmap_texture_, 1, 1,
                    graphics::Image::Format::kRGB8, graphics::Texture::Filter::kPoint,
                    (unsigned char*)&kHeightData);
                if (!default_heightmap_texture_) return false;
            }
            // Run service
            service_->RunService();
            // Setup LOD parameters
            {
                float fov = 2.0f * tan(0.5f * fovy_in_radians);

                float geo_detail = std::max(1.0f, kGeoDetail);
                lod_params_.geo_factor = screen_height / (geo_detail * fov);

                float tex_detail = std::max(1.0f, kTexDetail);
                lod_params_.tex_factor = screen_height / (tex_detail * fov);
            }
            // Create font for labels rendering
            return true;
        }
        void MercatorTree::Update()
        {
            // Update LOD state.
            if (!lod_freeze_)
            {
                const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
                vec3 cam_position_pixel;
                terrain_view_->LocalToPixel(terrain_view_->getCamPosition(), cam_position_pixel, kMSM);
                lod_params_.camera_position = cam_position_pixel;
                lod_params_.camera_front = frustum_->getDir();
            }
            if (IsCollection())
            {
                HandleRenderRequests();
                HandleInlineRequests();

                FillRenderedKeys();
                PrepareNodes();
            }
            else
            {
                // Preprocess tree for the first time
                if (preprocess_)
                {
                    PreprocessTree();
                    preprocess_ = false;
                }

                // Handle delayed requests (for rendering new tiles).
                HandleRenderRequests();

                if (!tree_freeze_)
                {
                    // Prune the LOD tree
                    PruneTree();

                    // Update LOD requests.
                    HandleInlineRequests();
                }
            }
        }
        void MercatorTree::Render()
        {
            shader_->Bind();
            if (IsCollection())
            {
                for (std::vector<MercatorNode*>::const_iterator it = rendered_nodes_.begin();
                    it != rendered_nodes_.end(); ++it)
                {
                    MercatorNode * node = *it;
                    if (node->WillRender())
                        node->Render();
                }
            }
            else
            {
                rendered_nodes_.clear();
                if (root_->WillRender())
                    root_->Render();
            }
            shader_->Unbind();

            ++frame_counter_;
        }
        void MercatorTree::RenderLabels()
        {
            renderer_->DisableDepthTest();

            billboard_shader_->Bind();

            // Draw labels
            used_labels_.clear();
            label_bounding_boxes_.clear();
            for (std::vector<MercatorNode*>::const_iterator it = rendered_nodes_.begin();
                it != rendered_nodes_.end(); ++it)
            {
                MercatorNode * node = *it;
                if (node->lod_ == terrain_view_->GetLod())
                    node->RenderLabels();
            }
            // Draw point user meshes
            for (std::vector<Icon*>::iterator it = icons_list_.begin(); it != icons_list_.end(); ++it)
            {
                Icon* icon = *it;
                icon->render();
            }

            billboard_shader_->Unbind();

            renderer_->EnableDepthTest();
        }
        class IconDistanceCompareFunctor
        {
        public:
            IconDistanceCompareFunctor(const mgnMdTerrainView * terrain_view)
            {
                const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
                terrain_view->LocalToPixel(terrain_view->getCamPosition(), camera_position_, kMSM);
            }
            bool operator()(Icon * first, Icon * second) const
            {
                vec3 first_position = first->position();
                vec3 second_position = second->position();
                float d1_sqr = math::DistanceSqr(first_position, camera_position_);
                float d2_sqr = math::DistanceSqr(second_position, camera_position_);
                return d1_sqr > d2_sqr; // distant ones should be first
            }
        private:
            vec3 camera_position_; //!< camera position in local cs
        };
        void MercatorTree::UpdateIconList()
        {
            // TODO: lock needed for selection requests
            //mgnCriticalSectionScopedEntrance guard(mIconListCriticalSection);
            // Form list
            icons_list_.clear();
            for (std::vector<MercatorNode*>::const_iterator it = rendered_nodes_.begin();
                it != rendered_nodes_.end(); ++it)
            {
                MercatorNode * node = *it;
                if (node->lod_ != terrain_view_->GetLod()) continue;

                // if (! tile->mIsFetchedUserObjects)
                //     continue;

                for (std::vector<Icon*>::iterator iti = node->point_user_meshes_.begin();
                    iti != node->point_user_meshes_.end(); ++iti)
                {
                    Icon * icon = *iti;
                    icons_list_.push_back( icon );
                }
            }
            // Then sort it by distance
            // This is the weakest part, complexity is O(n*log(n))
            std::sort(icons_list_.begin(), icons_list_.end(), IconDistanceCompareFunctor(terrain_view_));
        }
        void MercatorTree::SplitQuadTreeNode(MercatorNode* node)
        {
            // Parent is no longer an open node, now has at least one child.
            if (node->parent_)
                open_nodes_.erase(node->parent_);
            // This node is now open.
            open_nodes_.insert(node);
            // Create children.
            for (int i = 0; i < 4; ++i)
            {
                node->AttachChild(i);
            }
#ifdef DEBUG
            if (debug_info_.maximum_achieved_lod < node->lod_ + 1)
                debug_info_.maximum_achieved_lod = node->lod_ + 1;
#endif
        }
        void MercatorTree::MergeQuadTreeNode(MercatorNode* node)
        {
            // Delete children.
            for (int i = 0; i < 4; ++i)
            {
                if (node->children_[i]->has_children_)
                {
                    printf("Lickety split. Faulty merge.\n");
                    assert(false);
                }
                node->DetachChild(i, IsUsingPool());
            }
            // This node is now closed.
            open_nodes_.erase(node);
            if (node->parent_)
            {
                // Check to see if any siblings are split.
                for (int i = 0; i < 4; ++i)
                    if (node->parent_->children_[i]->IsSplit())
                        return;
                // If not, the parent is now open.
                open_nodes_.insert(node->parent_);
            }
        }
        void MercatorTree::Request(MercatorNode* node, int type, bool priority)
        {
            RequestQueue& request_queue = (type == REQUEST_MAPTILE) ? render_requests_ : inline_requests_;
            if (priority)
                request_queue.push_front(RequestType(node, type));
            else
                request_queue.push_back(RequestType(node, type));
        }
        void MercatorTree::Unrequest(MercatorNode* node)
        {
            // Remove node from queues
            RequestQueue* request_queues[] = { &render_requests_, &inline_requests_ };
            for (int q = 0; q < 2; ++q)
            {
                RequestQueue* request_queue = request_queues[q];
                RequestQueue::iterator i = (*request_queue).begin(), j;
                while (i != (*request_queue).end())
                {
                    if ((*i).node == node)
                    {
                        // Remove item at front of queue.
                        if (i == (*request_queue).begin())
                        {
                            (*request_queue).erase(i);
                            i = (*request_queue).begin();
                            continue;
                        }
                        else // Remove item mid-queue.
                        {
                            j = i;
                            --i;
                            (*request_queue).erase(j);
                        }
                    }
                    ++i;
                }
            }
            // Remove node from service
            // Guess this is the best solution to make a cycle
            while (!service_->RemoveAllNodeTasks(node));
        }
        void MercatorTree::HandleRequests(RequestQueue& requests)
        {
            // Ensure we only use up x time per frame.
            int weights[] = { 10, 10, 1, 2 };
            bool(MercatorTree::*handlers[4])(MercatorNode*) =
            {
                &MercatorTree::HandleRenderable,
                &MercatorTree::HandleMapTile,
                &MercatorTree::HandleSplit,
                &MercatorTree::HandleMerge
            };
            int limit = 1000;
            bool sorted = false;

            // Added request count to not stuck into the cycle (some handlers may duplicate requests)
            RequestQueue::size_type num_requests = requests.size();
            while (!requests.empty() && num_requests != 0)
            {
                RequestType request = *requests.begin();
                MercatorNode* node = request.node;

                // If not a root level task.
                if (node->parent_)
                {
                    // Verify job limits.
                    if (limit <= 0) return;
                    limit -= weights[request.type];
                }

                requests.pop_front();
                --num_requests;

                // Call handler.
                if ((this->*handlers[request.type])(node))
                {
                    // Job was completed. We can re-sort the priority queue.
                    if (!sorted)
                    {
                        requests.sort(RequestComparePriority());
                        sorted = true;
                    }
                }
            }
        }
        void MercatorTree::HandleRenderRequests()
        {
            // Process task that have been done on a service thread
            if (!preprocess_)
                ProcessDoneTasks();

            // Then handle requests
            HandleRequests(render_requests_);
        }
        void MercatorTree::HandleInlineRequests()
        {
            HandleRequests(inline_requests_);
        }
        bool MercatorTree::HandleRenderable(MercatorNode* node)
        {
            // Determine max relative LOD depth between grid and tile
            int maxLODRatio = GetTileResolution() / (grid_size_ - 1);
            int maxLOD = 0;
            while (maxLODRatio > 1) {
                maxLODRatio >>= 1;
                maxLOD++;
            }

            if (preprocess_)
            {
                // We need renderable to be created at preprocess stage for further split
                node->RefreshRenderable(&node->map_tile_);
                node->request_renderable_ = false;
            }
            else
            {
                // See if we can find a map tile to derive from.
                MercatorNode * ancestor = node;
                while (!ancestor->has_map_tile_ && ancestor->parent_) { ancestor = ancestor->parent_; };

                // See if map tile found is in acceptable LOD range (ie. grid size <= texturesize).
                if (ancestor->has_map_tile_)
                {
                    int relativeLOD = node->lod_ - ancestor->lod_;
                    if (relativeLOD <= maxLOD)
                    {
                        // Replace existing renderable.
                        node->RefreshRenderable(&ancestor->map_tile_);
                        node->request_renderable_ = false;
                    }
                }
            }

            // If no renderable was created, try creating a map tile.
            if (node->request_renderable_ && !node->has_map_tile_ && !node->request_map_tile_)
            {
                // Request a map tile for this node's LOD level.
                node->request_map_tile_ = true;
                Request(node, REQUEST_MAPTILE, true);
            }
            node->request_renderable_ = false;
            return true;
        }
        bool MercatorTree::HandleMapTile(MercatorNode* node)
        {
            // Assemble a map tile object for this node.
            node->request_map_tile_ = false;

            bool has_albedo = node->map_tile_.HasAlbedoTexture();
            bool has_heightmap = node->map_tile_.HasHeightmapTexture();

            if (!has_albedo)
            {
                RequestTexture(node);
            }
            if (!has_heightmap)
            {
                RequestHeightmap(node);
            }
            if (preprocess_)
            {
                // At preprocess stage we just need to enqueue tasks
                return false;
            }
            bool tile_filled = has_albedo /*&& has_heightmap*/;
            if (tile_filled)
            {
                // Map tile is complete and can be used as ancestor
                node->CreateMapTile();

                // Request a new renderable to match.
                node->RefreshRenderable(&node->map_tile_);
                node->request_renderable_ = false;

                // See if any child renderables use the old map tile.
                if (!IsCollection() && node->has_renderable_)
                {
                    MercatorMapTile * old_tile = node->renderable_.GetMapTile();
                    RefreshMapTile(node, old_tile, &node->map_tile_);
                }

                return true;
            }
            else // ! tile_filled
            {
                // Repeat this request until is done
                node->request_map_tile_ = true;
                Request(node, REQUEST_MAPTILE, false);
                return false;
            }
        }
        bool MercatorTree::HandleSplit(MercatorNode* node)
        {
            if (node->lod_ < GetMaxLod())
            {
                SplitQuadTreeNode(node);
                node->request_split_ = false;
            }
            else
            {
                // request_split_ is stuck on, so will not be requested again.
            }
            return true;
        }
        bool MercatorTree::HandleMerge(MercatorNode* node)
        {
            MergeQuadTreeNode(node);
            node->request_merge_ = false;
            return true;
        }
        void MercatorTree::PruneTree()
        {
            NodeHeap heap;

            NodeSet::iterator open_node = open_nodes_.begin();
            while (open_node != open_nodes_.end())
            {
                heap.push(*open_node);
                ++open_node;
            };

            while (!heap.empty())
            {
                MercatorNode* old_node = heap.top();
                heap.pop();
                if (!old_node->page_out_ &&
                    !old_node->request_merge_ &&
                    (GetFrameCounter() - old_node->last_opened_ > 100))
                {
                    old_node->renderable_.SetFrameOfReference();
                    // Make sure node's children are too detailed rather than just invisible.
                    if (old_node->renderable_.IsFarAway() ||
                        (old_node->renderable_.IsInLODRange() && old_node->renderable_.IsInMIPRange())
                        )
                    {
                        old_node->request_merge_ = true;
                        Request(old_node, REQUEST_MERGE, true);
                        return;
                    }
                    else
                    {
                        old_node->last_opened_ = GetFrameCounter();
                    }
                }
            }
        }
        void MercatorTree::RefreshMapTile(MercatorNode* node, MercatorMapTile* old_tile, MercatorMapTile* new_tile)
        {
            for (int i = 0; i < 4; ++i)
            {
                MercatorNode* child = node->children_[i];
                if (child && child->has_renderable_ && child->renderable_.GetMapTile() == old_tile)
                {
                    // Refresh renderable relative to the map tile.
                    child->RefreshRenderable(new_tile);

                    // Recurse
                    RefreshMapTile(child, old_tile, new_tile);
                }
            }
        }
        void MercatorTree::FlushMapTileToRoot(MercatorNode* node)
        {
            for (int i = 0; i < 4; ++i)
            {
                MercatorNode* child = node->children_[i];
                if (child && child->has_renderable_)
                {
                    // Flush map tile to root one
                    child->RefreshRenderable(&root_->map_tile_);
                    // Recurse
                    FlushMapTileToRoot(child);
                }
            }
        }
        void MercatorTree::ProcessDoneTasks()
        {
            MercatorService::TaskList done_tasks;
            if (service_->GetDoneTasks(done_tasks))
            {
                // There are some tasks
                Task * task;
                while (!done_tasks.empty())
                {
                    task = done_tasks.front();
                    done_tasks.pop_front();
                    task->Process();
                    delete task;
                }
            }
        }
        void MercatorTree::PreprocessTree()
        {
            do
            {
                HandleInlineRequests();
                HandleRenderRequests();
                if (root_->WillRender())
                    root_->Render();
            }
            while (!render_requests_.empty() || !inline_requests_.empty());

            // After preprocess we should flush map tiles to root level
            FlushMapTileToRoot(root_);

            // And sort tasks queue in service
            service_->SortTasks();
        }
        void MercatorTree::FillRenderedKeys()
        {
            // Determine current node
            const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
            int lod = terrain_view_->GetLod();
            float tiles_per_side = static_cast<float>(1 << lod);
            int x = static_cast<int>((lod_params_.camera_position.x / kMSM) * tiles_per_side);
            int y = static_cast<int>((1.0f - lod_params_.camera_position.z / kMSM) * tiles_per_side);

            /*
            z
            |
            6 5 4
            7 0 3
            8 1 2 _ x
            */

            // Fill keys
            rendered_keys_.clear();
            rendered_keys_.push_back(MercatorNodeKey(lod, x  , y  ));
            rendered_keys_.push_back(MercatorNodeKey(lod, x  , y-1));
            rendered_keys_.push_back(MercatorNodeKey(lod, x+1, y-1));
            rendered_keys_.push_back(MercatorNodeKey(lod, x+1, y  ));
            rendered_keys_.push_back(MercatorNodeKey(lod, x+1, y+1));
            rendered_keys_.push_back(MercatorNodeKey(lod, x  , y+1));
            rendered_keys_.push_back(MercatorNodeKey(lod, x-1, y+1));
            rendered_keys_.push_back(MercatorNodeKey(lod, x-1, y  ));
            rendered_keys_.push_back(MercatorNodeKey(lod, x-1, y-1));
        }
        void MercatorTree::PrepareNodes()
        {
            rendered_nodes_.clear();
            for (std::vector<MercatorNodeKey>::const_iterator it = rendered_keys_.begin();
                it != rendered_keys_.end(); ++it)
            {
                const MercatorNodeKey& key = *it;
                MercatorNode * node;
                AllocatedNodes::iterator ait = allocated_nodes_.find(key);
                if (ait == allocated_nodes_.end()) // hasn't been allocated yet
                {
                    node = new MercatorNode(this);
                    node->lod_ = key.lod;
                    node->x_ = key.x;
                    node->y_ = key.y;
                    allocated_nodes_.insert(std::make_pair(key, node));
                }
                else
                {
                    node = ait->second;
                }
                rendered_nodes_.push_back(node);
            }
        }
        void MercatorTree::RequestTexture(MercatorNode* node)
        {
            if (!node->request_albedo_)
            {
                node->request_albedo_ = true;
                if (node->has_labels_)
                    service_->AddTask(new TextureTask(node, provider_));
                else
                    service_->AddTask(new TextureLabelsTask(node, provider_));
            }
        }
        void MercatorTree::RequestHeightmap(MercatorNode* node)
        {
            if (!node->request_heightmap_)
            {
                node->request_heightmap_ = true;
                service_->AddTask(new HeightmapTask(node, provider_));
            }
        }
        void MercatorTree::RequestLabels(MercatorNode* node)
        {
            if (!node->request_labels_)
            {
                node->request_labels_ = true;
                service_->AddTask(new LabelsTask(node, provider_));
            }
        }
        void MercatorTree::RequestIcons(MercatorNode* node)
        {
            if (!node->request_icons_)
            {
                node->request_icons_ = true;
                service_->AddTask(new IconsTask(node, provider_, gps_position_));
            }
        }
        const int MercatorTree::grid_size() const
        {
            return grid_size_;
        }
        int MercatorTree::GetFrameCounter() const
        {
            return frame_counter_;
        }
        const bool MercatorTree::IsUsingPool()
        {
            return true;
        }
        const bool MercatorTree::IsCollection()
        {
            return true;
        }
        bool MercatorTree::RequestComparePriority::operator()(const RequestType& a, const RequestType& b) const
        {
            return (a.node->GetPriority() > b.node->GetPriority());
        }
        bool TaskNodeCompareFunctor::operator()(Task * task1, Task * task2) const
        {
            // Textures have first priority for fetch
            if (task1->type() != task2->type())
                return task1->type() < task2->type(); // texture over other requests
            else if (task1->node()->lod() != task2->node()->lod())
                return task1->node()->lod() < task2->node()->lod(); // lower LOD priority
            else
                return task1->node()->GetPriority() > task2->node()->GetPriority();
        }

    } // namespace terrain
} // namespace mgn