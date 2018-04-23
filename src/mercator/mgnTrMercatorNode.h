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

            void AttachChild(int position);
            void DetachChild(int position, bool use_pool);

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
            void OnTextureTaskCompleted(const graphics::Image& image, bool has_errors);
            void OnHeightmapTaskCompleted(const graphics::Image& image, bool has_errors);

        protected:
            // Node data functions
            void OnAttach();
            void OnDetach();

            void LoadData();
            void UnloadData();

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

            // Node data requests
            bool request_albedo_;
            bool request_heightmap_;
            bool request_labels_;
            bool request_icons_;

            // Node data load flags
            bool has_labels_;
            bool has_icons_;

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