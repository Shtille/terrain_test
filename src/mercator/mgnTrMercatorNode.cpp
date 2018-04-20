#include "mgnTrMercatorNode.h"

#include "mgnTrMercatorNodePool.h"
#include "mgnTrMercatorTileMesh.h"
#include "mgnTrMercatorTree.h"
#include "mgnTrMercatorMapTile.h"
#include "mgnTrMercatorRenderable.h"
#include "mgnTrMercatorService.h"

#include <cstddef>
#include <assert.h>

namespace mgn {
    namespace terrain {

        MercatorNode::MercatorNode(MercatorTree * tree)
        : owner_(tree)
        , map_tile_(this)
        , renderable_()
        , lod_(0)
        , x_(0)
        , y_(0)
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
        , parent_slot_(-1)
        , parent_(NULL)
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
                    owner_->node_pool_->Push(child);
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
            if (owner_->preprocess_)
                return;

            graphics::Shader * shader = owner_->shader_;

            // Vertex shader
            shader->Uniform4fv("u_stuv_scale", renderable_.stuv_scale_);
            shader->Uniform4fv("u_stuv_position", renderable_.stuv_position_);
            shader->Uniform1f("u_skirt_height", renderable_.distance_);
            //shader->Uniform4fv("u_color", renderable_.color_);

            renderable_.GetMapTile()->BindTexture();

            owner_->tile_->Render();
        }
        void MercatorNode::OnTextureTaskCompleted(const graphics::Image& image)
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
        }
        void MercatorNode::OnHeightmapTaskCompleted(const graphics::Image& image)
        {
            request_heightmap_ = false;
            map_tile_.SetHeightmapImage(image);
        }
        void MercatorNode::OnAttach()
        {
            // Load data on attach
            LoadData();
        }
        void MercatorNode::OnDetach()
        {
            // Unload data on detach
            UnloadData();
        }
        void MercatorNode::LoadData()
        {
            //
        }
        void MercatorNode::UnloadData()
        {
            //
        }

    } // namespace terrain
} // namespace mgn