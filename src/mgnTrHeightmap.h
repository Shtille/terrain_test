#pragma once
#ifndef __MGN_TERRAIN_HEIGHTMAP_H__
#define __MGN_TERRAIN_HEIGHTMAP_H__

#include "mgnTrMesh.h"

namespace mgn {
    namespace terrain {

        class TerrainTile;

        //! Heightmap mesh
        class Heightmap : protected Mesh {
        public:
            /*! Constructor
            @param heights is filled by rows
            @param size_x width of heightmap
            @param size_y height of heightmap
            @param position position of anchor point, middle of heightmap by default
            */
            explicit Heightmap(mgn::graphics::Renderer * renderer,
                int width, int height, float *heights, float size_x, float size_y);
            virtual ~Heightmap();

            int width() const;
            int height() const;
            float minHeight() const;
            float maxHeight() const;

            using Mesh::Render;
            using Mesh::MakeRenderable;

        private:
            void FillAttributes();
            void create(float *heights);

            int mWidth;             //!< width of the input samples regular grid
            int mHeight;            //!< height of the input samples regular grid
            float mSizeX;           //!< width of this heightmap in units
            float mSizeY;           //!< height of this heightmap in units
            float mTileSizeX;       //!< cell width
            float mTileSizeY;       //!< cell height
            float mMinHeight;       //!< minimum height
            float mMaxHeight;       //!< maximum height
        };

    } // namespace terrain
} // namespace mgn

#endif