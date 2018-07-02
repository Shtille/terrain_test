#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_COLLECTION_H__
#define __MGN_TERRAIN_MERCATOR_COLLECTION_H__

#include "mgnTrMercatorNode.h"

#include <boost/unordered_set.hpp>

#include <list>
#include <set>
#include <map>
#include <queue>

class mgnMdTerrainView;
class mgnMdWorldPosition;
namespace math {
    class Frustum;
    struct Rect;
}

namespace mgn {
    namespace terrain {

        // Forward declarations
        class MercatorTileMesh;
        class MercatorMapTile;
        class MercatorService;
        class MercatorProvider;
        class MercatorNodePool;
        class Font;

#ifdef DEBUG
        struct MercatorDebugInfo {
            int num_nodes;
            int num_map_tiles;
            int maximum_achieved_lod;
        };
#endif

        struct MercatorLodParams {
            int limit;

            math::Vector3 camera_position;  //!< position of camera in geocentric coordinate system
            math::Vector3 camera_front;     //!< forward direction vector of camera

            float geo_factor;
            float tex_factor;
        };

        //! Mercator tree rendering class
        class MercatorCollection {
            friend class MercatorNode;
            friend class MercatorRenderable;
            friend class MercatorMapTile;

            enum {
                REQUEST_RENDERABLE,
                REQUEST_MAPTILE,
                REQUEST_SPLIT,
                REQUEST_MERGE,
            };

            struct RequestType {
                MercatorNode* node;
                int type;

                RequestType(MercatorNode* node, int type) : node(node), type(type) {};
            };

            class RequestComparePriority {
            public:
                bool operator()(const RequestType& a, const RequestType& b) const;
            };

            typedef std::list<RequestType> RequestQueue;
            typedef std::set<MercatorNode*> NodeSet;
            typedef std::priority_queue<MercatorNode*, std::vector<MercatorNode*>, MercatorNodeCompareLastOpened> NodeHeap;

        public:
            MercatorCollection(graphics::Renderer * renderer,
                graphics::Shader * shader, graphics::Shader * billboard_shader, const Font * font,
                math::Frustum * frustum, mgnMdTerrainView * terrain_view,
                MercatorProvider * provider, const mgnMdWorldPosition * gps_position);
            ~MercatorCollection();

            //! Data video memory objects creation and other things that may fail
            bool Initialize(float fovy_in_radians, int screen_height);

            void Update();
            void Render();
            void RenderLabels();

            void UpdateIconList();

            const int grid_size() const;

            static const bool IsUsingPool();

            int GetFrameCounter() const;

        protected:
            void FillRenderedKeys();
            void PrepareNodes();
            void SplitQuadTreeNode(MercatorNode* node);
            void MergeQuadTreeNode(MercatorNode* node);
            void Request(MercatorNode* node, int type, bool priority = false);
            void Unrequest(MercatorNode* node);
            void HandleRequests(RequestQueue& requests);
            void HandleRenderRequests();
            void HandleInlineRequests();

            bool HandleRenderable(MercatorNode* node);
            bool HandleMapTile(MercatorNode* node);
            bool HandleSplit(MercatorNode* node);
            bool HandleMerge(MercatorNode* node);

            void PruneTree();
            void RefreshMapTile(MercatorNode* node, MercatorMapTile* old_tile, MercatorMapTile* new_tile);
            void FlushMapTileToRoot(MercatorNode* node);
            void ProcessDoneTasks();
            void PreprocessTree();

            void RequestTexture(MercatorNode* node);
            void RequestHeightmap(MercatorNode* node);
            void RequestLabels(MercatorNode* node);
            void RequestIcons(MercatorNode* node);

        private:
            graphics::Renderer * renderer_;     //!< pointer to renderer object
            graphics::Shader * shader_;         //!< pointer to shader object
            graphics::Shader * billboard_shader_;//!< pointer to shader object
            const Font * font_;
            math::Frustum * frustum_;           //!< pointer to frustum object
            mgnMdTerrainView * terrain_view_;   //!< pointer to terrain view object
            MercatorProvider * provider_;       //!< pointer to provider object
            const mgnMdWorldPosition * gps_position_;

            MercatorNode * root_;
            MercatorNodePool * node_pool_;

            const int grid_size_;
            MercatorTileMesh * tile_;
            MercatorService * service_;         //!< service for filling data

            graphics::Texture * default_albedo_texture_;
            graphics::Texture * default_heightmap_texture_;

            RequestQueue inline_requests_;
            RequestQueue render_requests_;
            NodeSet open_nodes_;

            float earth_radius_;
            MercatorLodParams lod_params_;
#ifdef DEBUG
            MercatorDebugInfo debug_info_;
#endif

            int frame_counter_;
            bool preprocess_;
            bool lod_freeze_;
            bool tree_freeze_;

            // Misc
            std::vector<MercatorNodeKey> rendered_keys_;
            std::vector<MercatorNode*> rendered_nodes_; //!< for optimized rendering of labels and other data
            typedef std::map<MercatorNodeKey, MercatorNode*> AllocatedNodes;
            std::map<MercatorNodeKey, MercatorNode*> allocated_nodes_;

            typedef std::map<size_t, graphics::Texture*> IconTextureCache;
            IconTextureCache icon_texture_cache_;

            typedef std::map<unsigned int, graphics::Texture*> ShieldTextureCache;
            ShieldTextureCache shield_texture_cache_;

            std::vector<math::Rect> label_bounding_boxes_; //!< to not render overlapping labels
            typedef boost::unordered_set<std::wstring> UsedLabelsSet;
            UsedLabelsSet used_labels_; //!< to not render duplicated labels
            std::vector<Icon*> icons_list_; //!< list of icons (any billboard objects) for rendering
        };

    } // namespace terrain
} // namespace mgn

#endif