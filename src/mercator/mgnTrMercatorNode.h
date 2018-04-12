#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_NODE_H__
#define __MGN_TERRAIN_MERCATOR_NODE_H__

#include "mgnTrMercatorMapTile.h"
#include "mgnTrMercatorRenderable.h"

#include "../mgnTrMesh.h"

class mgnMdTerrainView;

namespace mgn {
    namespace terrain {

        // Forward declarations
        class MercatorTree;

        //! Mercator tree node class
        class MercatorNode {
            friend class MercatorTree;
            friend class MercatorRenderable;
            friend class MercatorMapTile;
            friend class MercatorNodeCompareLastOpened;
        public:
            enum Slot { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

            explicit MercatorNode(MercatorTree * tree);
            virtual ~MercatorNode();

            int lod() const;
            int x() const;
            int y() const;

            const float GetPriority() const;

            bool IsSplit();

            void AttachChild(MercatorNode * child, int position);
            void DetachChild(int position);

            void PropagateLodDistances();

            void CreateMapTile();
            void DestroyMapTile();

            void CreateRenderable(MercatorMapTile * map_tile);
            void DestroyRenderable();
            void RefreshRenderable(MercatorMapTile * map_tile);

            bool WillRender();
            int Render();
            void RenderSelf();

            // Task functions
            void OnTextureTaskCompleted(const graphics::Image& image);
            void OnHeightmapTaskCompleted(const graphics::Image& image);

        private:
            MercatorTree * owner_; //!< owner tree

            MercatorMapTile map_tile_;
            MercatorRenderable renderable_;

            int lod_; //!< level of detail
            int x_;
            int y_;

            int last_rendered_;
            int last_opened_;

            bool has_children_;
            bool page_out_;
            bool has_map_tile_;
            bool has_renderable_;

            bool request_page_out_;
            bool request_map_tile_;
            bool request_renderable_;
            bool request_split_;
            bool request_merge_;

            bool request_albedo_;
            bool request_heightmap_;

            int parent_slot_;
            MercatorNode * parent_;
            MercatorNode * children_[4];
        };

        // Quad tree node comparison for LOD age.
        class MercatorNodeCompareLastOpened {
        public:
            bool operator()(const MercatorNode* a, const MercatorNode* b) const {
                return (a->last_opened_ > b->last_opened_);
            }
        };

    } // namespace terrain
} // namespace mgn

#endif