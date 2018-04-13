#include "mgnTrMercatorTree.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorTileMesh.h"
#include "mgnTrMercatorRenderable.h"
#include "mgnTrMercatorService.h"

#include "mgnTrMercatorTaskTexture.h"
#include "mgnTrMercatorTaskHeightmap.h"

#include "mgnMdTerrainView.h"

#include <cmath>
#include <cstddef>
#include <assert.h>

namespace {
    // The more detail coefficient is, the less detalization is required
    const float kGeoDetail = 6.0f;
    const float kTexDetail = 3.0f;
}

namespace mgn {
    namespace terrain {

        MercatorTree::MercatorTree(graphics::Renderer * renderer, graphics::Shader * shader,
                math::Frustum * frustum, mgnMdTerrainView * terrain_view,
                MercatorProvider * provider)
        : renderer_(renderer)
        , shader_(shader)
        , frustum_(frustum)
        , terrain_view_(terrain_view)
        , provider_(provider)
        , grid_size_(17)
        , frame_counter_(0)
        , preprocess_(true)
        , lod_freeze_(false)
        , tree_freeze_(false)
        {
            root_ = new MercatorNode(this);
            //tiles_pool_ = new MercatorNode*[tile_capacity];

            tile_ = new MercatorTileMesh(renderer, grid_size_);
            service_ = new MercatorService();

            const float kPlanetRadius = 6371000.0f;
            terrain_view->LocalToPixelDistance(kPlanetRadius, earth_radius_, GetMapSizeMax());

#ifdef DEBUG
            debug_info_.num_nodes = 1;
            debug_info_.num_map_tiles = 0;
            debug_info_.maximum_achieved_lod = 0;
#endif
        }
        MercatorTree::~MercatorTree()
        {
            //delete[] tiles_pool_;
            delete root_;

            delete tile_;
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
        }
        bool MercatorTree::Initialize()
        {
            // Create tile mesh
            tile_->Create();
            if (!tile_->MakeRenderable())
                return false;
            // Create default textures
            const int kAlbedoData = (0xff << 24) | (0xff << 16) | (0xff << 8) | 0xff;
            renderer_->CreateTextureFromData(default_albedo_texture_, 1, 1,
                graphics::Image::Format::kRGBA32, graphics::Texture::Filter::kLinear,
                (unsigned char*)&kAlbedoData);
            if (!default_albedo_texture_) return false;
            const int kHeightData = (0xff << 24);
            renderer_->CreateTextureFromData(default_heightmap_texture_, 1, 1,
                graphics::Image::Format::kRGBA32, graphics::Texture::Filter::kLinear,
                (unsigned char*)&kHeightData);
            if (!default_heightmap_texture_) return false;
            // Run service
            service_->RunService();
            return true;
        }
        void MercatorTree::SetParameters(float fovy_in_radians, int screen_height)
        {
            float fov = 2.0f * tan(0.5f * fovy_in_radians);

            float geo_detail = std::max(1.0f, kGeoDetail);
            lod_params_.geo_factor = screen_height / (geo_detail * fov);

            float tex_detail = std::max(1.0f, kTexDetail);
            lod_params_.tex_factor = screen_height / (tex_detail * fov);
        }
        void MercatorTree::Update()
        {
            // Update LOD state.
            if (!lod_freeze_)
            {
                vec3 cam_position_pixel;
                terrain_view_->LocalToPixel(terrain_view_->getCamPosition(), cam_position_pixel,
                    MercatorTree::GetMapSizeMax());
                lod_params_.camera_position = cam_position_pixel;
                lod_params_.camera_front = frustum_->getDir();
            }

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
        void MercatorTree::Render()
        {
            shader_->Bind();
            if (root_->WillRender())
                root_->Render();
            shader_->Unbind();

            ++frame_counter_;
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
                MercatorNode* child = new MercatorNode(node->owner_);
                node->AttachChild(child, i);
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
                if (node->children_[i]->children_[0] ||
                    node->children_[i]->children_[1] ||
                    node->children_[i]->children_[2] ||
                    node->children_[i]->children_[3])
                {
                    printf("Lickety split. Faulty merge.\n");
                    assert(false);
                }
                node->DetachChild(i);
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
            int maxLODRatio = (GetTextureSize() - 1) / (grid_size_ - 1);
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

            if (!has_albedo && !node->request_albedo_)
            {
                node->request_albedo_ = true;
                service_->AddTask(new TextureTask(node, provider_));
            }
            if (!has_heightmap && !node->request_heightmap_)
            {
                node->request_heightmap_ = true;
                service_->AddTask(new HeightmapTask(node, provider_));
            }
            if (preprocess_)
            {
                // At preprocess stage we just need to enqueue tasks
                return false;
            }
            bool tile_filled = has_albedo && has_heightmap;
            if (tile_filled)
            {
                // Map tile is complete and can be used as ancestor
                node->CreateMapTile();

                // Request a new renderable to match.
                node->request_renderable_ = true;
                Request(node, REQUEST_RENDERABLE, true);

                // See if any child renderables use the old map tile.
                if (node->has_renderable_)
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
            if (node->lod_ < GetLodLimit())
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
        const int MercatorTree::grid_size() const
        {
            return grid_size_;
        }
        int MercatorTree::GetFrameCounter() const
        {
            return frame_counter_;
        }
        const int MercatorTree::GetLodLimit()
        {
            return 22; // up to 22
        }
        const float MercatorTree::GetMapSizeMax()
        {
            return static_cast<float>(256 << GetLodLimit());
        }
        const int MercatorTree::GetTextureSize()
        {
            return 257;
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