#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TREE_H__
#define __MGN_TERRAIN_MERCATOR_TREE_H__

#include "mgnTrMercatorNode.h"

#include <list>
#include <set>
#include <queue>

class mgnMdTerrainView;
namespace math {
    class Frustum;
}

namespace mgn {
    namespace terrain {

        // Forward declarations
        class MercatorTileMesh;
        class MercatorMapTile;
        class MercatorService;
        class MercatorProvider;

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
        class MercatorTree {
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
            MercatorTree(graphics::Renderer * renderer, graphics::Shader * shader,
                math::Frustum * frustum, mgnMdTerrainView * terrain_view,
                MercatorProvider * provider);
            ~MercatorTree();

            //! Data video memory objects creation and other things that may fail
            bool Initialize();

            void SetParameters(float fovy_in_radians, int screen_height);

            void Update();
            void Render();

            const int grid_size() const;

            static const int GetLodLimit();
            static const float GetMapSizeMax();
            static const int GetTextureSize();

            int GetFrameCounter() const;

        protected:
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
            void RefreshMapTile(MercatorNode* node, MercatorMapTile* tile);
            void ProcessDoneTasks();
            void PreprocessTree();

        private:
            graphics::Renderer * renderer_;     //!< pointer to renderer object
            graphics::Shader * shader_;         //!< pointer to shader object
            math::Frustum * frustum_;           //!< pointer to frustum object
            mgnMdTerrainView * terrain_view_;   //!< pointer to terrain view object
            MercatorProvider * provider_;       //!< pointer to provider object

            MercatorNode * root_;
            unsigned int tile_capacity_;
            unsigned int num_tiles_;
            MercatorNode ** tiles_pool_;

            const int grid_size_;
            MercatorTileMesh * tile_;
            MercatorService * service_;         //!< service for filling data

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
        };

    } // namespace terrain
} // namespace mgn

#endif