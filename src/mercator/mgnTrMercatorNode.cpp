#include "mgnTrMercatorNode.h"

#include "mgnTrMercatorNodePool.h"
#include "mgnTrMercatorTileMesh.h"
#include "mgnTrMercatorTree.h"
#include "mgnTrMercatorMapTile.h"
#include "mgnTrMercatorRenderable.h"
#include "mgnTrMercatorService.h"

#include "../mgnTrAtlasLabel.h"
#include "../mgnTrLabel.h"
#include "../mgnTrIcon.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

#include <cstddef>
#include <assert.h>

static int InterpolateColor(int c1, int c2, float alpha)
{
    int a1 = (c1 & 0xff000000) >> 24;
    int b1 = (c1 & 0xff0000) >> 16;
    int g1 = (c1 & 0x00ff00) >> 8;
    int r1 = (c1 & 0x0000ff);
    int a2 = (c2 & 0xff000000) >> 24;
    int b2 = (c2 & 0xff0000) >> 16;
    int g2 = (c2 & 0x00ff00) >> 8;
    int r2 = (c2 & 0x0000ff);
    int r = r1 + (int)((r2-r1)*alpha);
    int g = g1 + (int)((g2-g1)*alpha);
    int b = b1 + (int)((b2-b1)*alpha);
    int a = a1 + (int)((a2-a1)*alpha);
    return (a << 24) | (b << 16) | (g << 8) | r;
}
static bool MakePotTextureFromNpot(int width, int height, const std::vector<unsigned char>& vdata,
    int& out_width, int& out_height, std::vector<unsigned char>& out_data)
{
    if (((width & (width-1)) != 0) || ((height & (height-1)) != 0))
    {
        // Need to make POT
        const unsigned int * data = reinterpret_cast<const unsigned int*>(&vdata[0]);
        int w2 = math::RoundToPowerOfTwo(width);
        int h2 = math::RoundToPowerOfTwo(height);
        out_width = w2;
        out_height = h2;

        // Rescale image to power of 2
        int new_size = w2 * h2;
        out_data.resize(new_size*4);
        unsigned int * new_data = reinterpret_cast<unsigned int*>(&out_data[0]);
        for (int dh2 = 0; dh2 < h2; ++dh2)
        {
            float rh = (float)dh2 / (float)h2;
            float y = rh * (float)height;
            int dh = (int)y;
            int dh1 = std::min<int>(dh+1, height-1);
            float ry = y - (float)dh; // fract part of y
            for (int dw2 = 0; dw2 < w2; ++dw2)
            {
                float rw = (float)dw2 / (float)w2;
                float x = rw * (float)width;
                int dw = (int)x;
                int dw1 = std::min<int>(dw+1, width-1);
                float rx = x - (float)dw; // fract part of x

                // We will use bilinear interpolation
                int sample1 = (int) data[dw +width*dh ];
                int sample2 = (int) data[dw1+width*dh ];
                int sample3 = (int) data[dw +width*dh1];
                int sample4 = (int) data[dw1+width*dh1];

                int color1 = InterpolateColor(sample1, sample2, rx);
                int color2 = InterpolateColor(sample3, sample4, rx);;
                int color3 = InterpolateColor(color1, color2, ry);
                new_data[dw2+w2*dh2] = (unsigned int)color3;
            }
        }
        return true;
    }
    else // already a pot texture
        return false;
}

namespace mgn {
    namespace terrain {

        MercatorNode::MercatorNode(MercatorTree * tree)
        : owner_(tree)
        , map_tile_(this)
        , renderable_()
        , lod_(0)
        , x_(0)
        , y_(0)
        , parent_slot_(-1)
        , parent_(NULL)
        , has_children_(false)
        , page_out_(false)
        , has_map_tile_(false)
        , has_renderable_(false)
        , request_page_out_(false)
        , request_map_tile_(false)
        , request_renderable_(false)
        , request_split_(false)
        , request_merge_(false)
        , request_albedo_(false)
        , request_heightmap_(false)
        , request_labels_(false)
        , request_icons_(false)
        , has_labels_(false)
        , has_icons_(false)
        {
            last_opened_ = last_rendered_ = owner_->GetFrameCounter();
            for (int i = 0; i < 4; ++i)
                children_[i] = NULL;
        }
        MercatorNode::~MercatorNode()
        {
            owner_->Unrequest(this);
            if (parent_)
                parent_->children_[parent_slot_] = NULL;
            DestroyMapTile();
            DestroyRenderable();
            UnloadData();
            for (int i = 0; i < 4; ++i)
                DetachChild(i, false);
        }
        int MercatorNode::lod() const
        {
            return lod_;
        }
        int MercatorNode::x() const
        {
            return x_;
        }
        int MercatorNode::y() const
        {
            return y_;
        }
        const float MercatorNode::GetPriority() const
        {
            if (!has_renderable_)
            {
                if (parent_)
                    return parent_->GetPriority();
                return 0.0f;
            }
            return renderable_.GetLodPriority();
        }
        const mgnMdTerrainView * MercatorNode::terrain_view() const
        {
            return owner_->terrain_view_;
        }
        bool MercatorNode::IsSplit()
        {
            return children_[0] || children_[1] || children_[2] || children_[3];
        }
        void MercatorNode::AttachChild(int position)
        {
            if (children_[position])
                throw "Attaching child where one already exists.";

            MercatorNodeKey key(
                lod_ + 1,
                x_ * 2 + (position % 2),
                y_ * 2 + (position / 2));

            MercatorNode * child = NULL;
            if (owner_->IsUsingPool())
            {
                // Get node from pool
                child = owner_->node_pool_->Pull(key);
                if (!child) // hasn't been allocated yet
                    child = new MercatorNode(owner_);
                // Some loading stuff
                child->OnAttach();
            }
            else
            {
                child = new MercatorNode(owner_);
            }

            children_[position] = child;
            child->parent_ = this;
            child->parent_slot_ = position;

            child->lod_ = key.lod;
            child->x_ = key.x;
            child->y_ = key.y;

            has_children_ = true;

#ifdef DEBUG
            ++owner_->debug_info_.num_nodes;
#endif
        }
        void MercatorNode::DetachChild(int position, bool use_pool)
        {
            MercatorNode * child = children_[position];
            if (child)
            {
                if (use_pool)
                {
                    MercatorNode * dropped_node = owner_->node_pool_->Push(child);
                    if (dropped_node != NULL) // pool is full
                    {
                        // We should cleanup dropped nodes
                        delete dropped_node;
                    }
                    // Some unloading stuff
                    child->OnDetach();
                }
                else
                {
                    delete child;
                }
                children_[position] = NULL;

                has_children_ = children_[0] || children_[1] || children_[2] || children_[3];

#ifdef DEBUG
                --owner_->debug_info_.num_nodes;
#endif
            }
        }
        void MercatorNode::PropagateLodDistances()
        {
            if (has_renderable_)
            {
                float max_child_distance = 0.0f;
                // Get maximum LOD distance of all children.
                for (int i = 0; i < 4; ++i)
                {
                    if (children_[i] && children_[i]->has_renderable_)
                    {
                        // Increase LOD distance w/ centroid distances, to ensure proper LOD nesting.
                        float child_distance = children_[i]->renderable_.GetLodDistance();
                        if (max_child_distance < child_distance)
                            max_child_distance = child_distance;
                    }
                }
                // Store in renderable.
                renderable_.SetChildLodDistance(max_child_distance);
            }
            // Propagate changes to parent.
            if (parent_)
                parent_->PropagateLodDistances();
        }
        void MercatorNode::CreateMapTile()
        {
            assert(!has_map_tile_);
            has_map_tile_ = true;
            map_tile_.Create(this);
#ifdef DEBUG
            ++owner_->debug_info_.num_map_tiles;
#endif
        }
        void MercatorNode::DestroyMapTile()
        {
            map_tile_.Destroy();
            has_map_tile_ = false;
#ifdef DEBUG
            --owner_->debug_info_.num_map_tiles;
#endif
        }
        void MercatorNode::CreateRenderable(MercatorMapTile * map_tile)
        {
            assert(!has_renderable_);
            if (page_out_)
                page_out_ = false;
            has_renderable_ = true;
            renderable_.Create(this, map_tile);
            PropagateLodDistances();
        }
        void MercatorNode::DestroyRenderable()
        {
            renderable_.Destroy();
            has_renderable_ = false;
            PropagateLodDistances();
        }
        void MercatorNode::RefreshRenderable(MercatorMapTile * map_tile)
        {
            renderable_.Update(this, map_tile);
            page_out_ = false;
            has_renderable_ = true;
            PropagateLodDistances();
        }
        bool MercatorNode::WillRender()
        {
            // Being asked to render ourselves.
            if (!has_renderable_)
            {
                last_opened_ = last_rendered_ = owner_->GetFrameCounter();

                if (page_out_ && has_children_)
                    return true;

                if (!request_renderable_)
                {
                    request_renderable_ = true;
                    owner_->Request(this, MercatorTree::REQUEST_RENDERABLE);
                }
                return false;
            }
            return true;
        }
        int MercatorNode::Render()
        {
            // In case of collection grid rendering is much easier
            if (owner_->IsCollection())
            {
                if (has_renderable_)
                {
                    // TODO: add frustum clipping
                    if (!has_map_tile_ && !request_map_tile_)
                    {
                        // Request a native res map tile.
                        request_map_tile_ = true;
                        owner_->Request(this, MercatorTree::REQUEST_MAPTILE);
                    }
                    RenderSelf();
                    return 1;
                }
                return 0;
            }

            // Determine if this node's children are render-ready.
            bool will_render_children = true;
            for (int i = 0; i < 4; ++i)
            {
                // Note: intentionally call WillRender on /all/ children, not just until one fails,
                // to ensure all 4 children are queued in immediately.
                if (!children_[i] || !children_[i]->WillRender())
                    will_render_children = false;
            }

            // If node is paged out, always recurse.
            if (page_out_)
            {
                // Recurse down, calculating min recursion level of all children.
                int min_level = children_[0]->Render();
                for (int i = 1; i < 4; ++i)
                {
                    int level = children_[i]->Render();
                    if (level < min_level)
                        min_level = level;
                }
                // If we are a shallow node.
                if (!request_renderable_ && min_level <= 1)
                {
                    request_renderable_ = true;
                    owner_->Request(this, MercatorTree::REQUEST_RENDERABLE);
                }
                return min_level + 1;
            }

            // If we are renderable, check LOD/visibility.
            if (has_renderable_)
            {
                renderable_.SetFrameOfReference();

                // If invisible, return immediately.
                if (renderable_.IsClipped())
                    return 1;

                // Whether to recurse down.
                bool recurse = false;

                // If the texture is not fine enough...
                if (!renderable_.IsInMIPRange())
                {
                    const int kStartLod = 5;
                    if (owner_->preprocess_)
                    {
                        // We need to do a split at preprocess stage
                        recurse = true;
                        // Also make a map tile request just to enqueue tasks
                        if (!request_albedo_ && lod_ >= kStartLod)
                        {
                            request_map_tile_ = true;
                            owner_->Request(this, MercatorTree::REQUEST_MAPTILE);
                        }
                    }
                    else if (lod_ < kStartLod)
                    {
                        recurse = true;
                    }
                    // If there is already a native res map-tile...
                    else if (has_map_tile_)
                    {
                        // Make sure the renderable is up-to-date.
                        if (renderable_.GetMapTile() == &map_tile_)
                        {
                            // Split so we can try this again on the child tiles.
                            recurse = true;
                        }
                    }
                    // Otherwise try to get native res tile data.
                    else
                    {
                        // Make sure no parents are waiting for tile data update.
                        MercatorNode *ancestor = this;
                        bool parent_request = false;
                        while (ancestor && !ancestor->has_map_tile_ && !ancestor->page_out_)
                        {
                            if (ancestor->request_map_tile_ || ancestor->request_renderable_)
                            {
                                parent_request = true;
                                break;
                            }
                            ancestor = ancestor->parent_;
                        }

                        if (!parent_request)
                        {
                            // Request a native res map tile.
                            request_map_tile_ = true;
                            owner_->Request(this, MercatorTree::REQUEST_MAPTILE);
                        }
                    }
                }

                // If the geometry is not fine enough...
                if ((has_children_ || !request_map_tile_) && !renderable_.IsInLODRange())
                {
                    // Go down an LOD level.
                    recurse = true;
                }

                // If a recursion was requested...
                if (recurse)
                {
                    // Update recursion counter, used to find least recently used nodes to page out.
                    last_opened_ = owner_->GetFrameCounter();

                    // And children are available and renderable...
                    if (has_children_)
                    {
                        if (will_render_children)
                        {
                            // Recurse down, calculating min recursion level of all children.
                            int min_level = children_[0]->Render();
                            for (int i = 1; i < 4; ++i)
                            {
                                int level = children_[i]->Render();
                                if (level < min_level)
                                    min_level = level;
                            }
                            // If we are a shallow node with a tile that is not being rendered or close to being rendered.
                            if (min_level > 1 && has_map_tile_ && false)
                            {
                                page_out_ = true;
                                DestroyRenderable();
                                DestroyMapTile();
                            }
                            return min_level + 1;
                        }
                    }
                    // If no children exist yet, request them.
                    else if (!request_split_)
                    {
                        request_split_ = true;
                        owner_->Request(this, MercatorTree::REQUEST_SPLIT);
                    }
                }

                // Last rendered flag, used to find ancestor patches that can be paged out.
                last_rendered_ = owner_->GetFrameCounter();

                // Otherwise, render ourselves.
                RenderSelf();

                return 1;
            }
            return 0;
        }
        void MercatorNode::RenderSelf()
        {
            if (!owner_->IsCollection())
            {
                if (owner_->preprocess_)
                    return;

                owner_->rendered_nodes_.push_back(this);
            }

            graphics::Shader * shader = owner_->shader_;

            // Vertex shader
            shader->Uniform4fv("u_stuv_scale", renderable_.stuv_scale_);
            shader->Uniform4fv("u_stuv_position", renderable_.stuv_position_);
            shader->Uniform1f("u_skirt_height", renderable_.distance_);
            //shader->Uniform4fv("u_color", renderable_.color_);

            renderable_.GetMapTile()->BindTexture();
            owner_->tile_->Render();
            owner_->renderer_->ChangeTexture(NULL, 1U);
        }
        void MercatorNode::RenderLabels()
        {
            MercatorTree::UsedLabelsSet & used_labels = owner_->used_labels_;
            std::vector<math::Rect> & bboxes = owner_->label_bounding_boxes_;

            for (std::vector<Label*>::iterator it = label_meshes_.begin();
                it != label_meshes_.end(); ++it)
            {
                Label * label = *it;
                if(used_labels.find(label->text()) == used_labels.end())
                {
                    label->render();
                    used_labels.insert(label->text());
                }
            }

            const math::Matrix4& view = owner_->renderer_->view_matrix();
            for (std::vector<AtlasLabel*>::iterator it = atlas_label_meshes_.begin();
                it != atlas_label_meshes_.end(); ++it)
            {
                AtlasLabel * label = *it;
                math::Rect bbox;
                label->GetBoundingBox(view, bbox);
                bool intersection_found = false;
                for (std::vector<math::Rect>::const_iterator itb = bboxes.begin();
                    itb != bboxes.end(); ++itb)
                {
                    const math::Rect& checked_bbox = *itb;
                    if (math::RectRectIntersection2D(bbox, checked_bbox))
                    {
                        intersection_found = true;
                        break;
                    }
                }
                if(!intersection_found && used_labels.find(label->text()) == used_labels.end())
                {
                    label->render();
                    used_labels.insert(label->text());
                    bboxes.push_back(bbox);
                }
            }
        }
        void MercatorNode::OnTextureTaskCompleted(const graphics::Image& image, bool has_errors)
        {
            request_albedo_ = false;
            map_tile_.SetAlbedoImage(image);

            // Remember old map tile before update
            MercatorMapTile * old_tile = renderable_.GetMapTile();

            // Refresh renderable relative to the map tile.
            RefreshRenderable(&map_tile_);
            request_renderable_ = false;

            // See if any child renderables use the old map tile.
            owner_->RefreshMapTile(this, old_tile, &map_tile_);

            if (has_errors) // repeat request
                owner_->RequestTexture(this);
        }
        void MercatorNode::OnHeightmapTaskCompleted(const graphics::Image& image, bool has_errors)
        {
            request_heightmap_ = false;
            map_tile_.SetHeightmapImage(image);

            if (has_errors)
                owner_->RequestHeightmap(this);

            LoadData();
        }
        void MercatorNode::OnLabelsTaskCompleted(const std::vector<LabelData>& labels_data, bool has_errors)
        {
            // Build label meshes
            for (std::vector<LabelData>::const_iterator it = labels_data.begin();
                it != labels_data.end(); ++it)
            {
                // Recognize type of billboard (label or shield)
                const LabelData& data = *it;
                if (data.centered) // shield
                {
                    graphics::Texture * texture;
                    MercatorTree::ShieldTextureCache::iterator it_shield =
                        owner_->shield_texture_cache_.find(data.shield_hash);
                    if (it_shield == owner_->shield_texture_cache_.end())
                    {
                        int new_width, new_height;
                        std::vector<unsigned char> new_data;
                        // This shield doesn't exist in texture cache, make a new texture from bitmap
                        if (MakePotTextureFromNpot(data.width, data.height, data.bitmap_data,
                                                   new_width, new_height, new_data))
                        {
                            // Texture data has been converted to power of two
                            owner_->renderer_->CreateTextureFromData(texture, new_width, new_height,
                                graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear,
                                &new_data[0]);
                        }
                        else
                        {
                            // Texture already has power of two sizes
                            owner_->renderer_->CreateTextureFromData(texture, data.width, data.height,
                                graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear,
                                &data.bitmap_data[0]);
                        }
                        // Also insert texture into cache
                        owner_->shield_texture_cache_.insert(std::make_pair(data.shield_hash, texture));
                    }
                    else
                    {
                        // This shield exists in texture cache, so obtain it texture from cache
                        texture = it_shield->second;
                    }

                    Label *label = new Label(owner_->renderer_, owner_->terrain_view_,
                        owner_->billboard_shader_, data, lod_);
                    label->setTexture(texture, false); // shield texture cache owns the texture
                    label_meshes_.push_back(label);
                }
                else // atlas-based label
                {
                    AtlasLabel *label = new AtlasLabel(owner_->renderer_, owner_->terrain_view_,
                        owner_->billboard_shader_, owner_->font_, data, lod_);
                    atlas_label_meshes_.push_back(label);
                }
            }
            has_labels_ = true;
        }
        void MercatorNode::OnTextureLabelsTaskCompleted(const graphics::Image& image,
            const std::vector<LabelData>& labels_data, bool has_errors)
        {
            OnTextureTaskCompleted(image, has_errors);
            OnLabelsTaskCompleted(labels_data, has_errors);
        }
        void MercatorNode::OnIconsTaskCompleted(const std::vector<IconData>& icons_data, bool has_errors)
        {
            // Precision is equal to 2 pixels in chosen LOD (in degrees)
            const double kPrecision = 2.0 * 360.0 / static_cast<double>(256 << lod_);
            for (std::vector<IconData>::const_iterator it = icons_data.begin(); it != icons_data.end(); ++it)
            {
                const IconData & data = *it;

                // Filter out duplicates (POIs that stand in the same position)
                bool has_duplicate = false;
                std::vector<IconData>::const_iterator dit = it; ++dit;
                for (; dit != icons_data.end(); ++dit)
                {
                    const IconData & other_data = *dit;
                    if (fabs(data.latitude - other_data.latitude) < kPrecision &&
                        fabs(data.longitude - other_data.longitude) < kPrecision) // the same position
                    {
                        has_duplicate = true;
                        break;
                    }
                }
                if (has_duplicate)
                    continue;

                size_t bitmap_hash = data.hash;

                MercatorTree::IconTextureCache::iterator it_cache = owner_->icon_texture_cache_.find(bitmap_hash);
                if (it_cache != owner_->icon_texture_cache_.end()) // texture with this ID exists in cache
                {
                    graphics::Texture * texture = it_cache->second;
                    Icon *icon = new Icon(owner_->renderer_, owner_->terrain_view_,
                        owner_->billboard_shader_, data, lod_);
                    icon->setTexture(texture, false);
                    point_user_meshes_.push_back(icon);
                    continue;
                }

                // Rescale image if it's not power of two 
                // (thus generateMipmaps isn't working on iOS devices with NPOT textures)
                graphics::Texture * texture = NULL;
                int new_width, new_height;
                std::vector<unsigned char> new_data;
                if (MakePotTextureFromNpot(data.width, data.height, data.bitmap_data,
                                           new_width, new_height, new_data))
                {
                    const unsigned char * udata = & new_data[0];
                    owner_->renderer_->CreateTextureFromData(texture, new_width, new_height,
                        graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, udata);
                }
                else
                {
                    const unsigned char * udata = & data.bitmap_data[0];
                    owner_->renderer_->CreateTextureFromData(texture, data.width, data.height,
                        graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, udata);
                }
                
                owner_->icon_texture_cache_.insert(std::make_pair<size_t, graphics::Texture*>(bitmap_hash, texture));
                
                Icon *icon = new Icon(owner_->renderer_, owner_->terrain_view_,
                    owner_->billboard_shader_, data, lod_);
                icon->setTexture(texture, false); // cache owns texture
                point_user_meshes_.push_back(icon);
            }
            has_icons_ = true;

            // Finally update icons list
            owner_->UpdateIconList();
        }
        void MercatorNode::OnAttach()
        {
            // Load data on attach
            //LoadData();
        }
        void MercatorNode::OnDetach()
        {
            // Unload data on detach
            //UnloadData();
        }
        void MercatorNode::LoadData()
        {
            // Request labels load
            if (!has_labels_)
                owner_->RequestLabels(this);

            // Request icons load
            if (!has_icons_)
                owner_->RequestIcons(this);
        }
        void MercatorNode::UnloadData()
        {
            if (has_labels_)
            {
                for (std::vector<Label*>::iterator it = label_meshes_.begin();
                    it != label_meshes_.end(); ++it)
                    delete *it;
                label_meshes_.clear();
                for (std::vector<AtlasLabel*>::iterator it = atlas_label_meshes_.begin();
                    it != atlas_label_meshes_.end(); ++it)
                    delete *it;
                atlas_label_meshes_.clear();
                has_labels_ = false;
            }
            if (has_icons_)
            {
                for (std::vector<Icon*>::iterator it = point_user_meshes_.begin();
                    it != point_user_meshes_.end(); ++it)
                    delete *it;
                point_user_meshes_.clear();
                has_icons_ = false;
                // Need to flush icons list
                owner_->UpdateIconList();
            }
        }

    } // namespace terrain
} // namespace mgn