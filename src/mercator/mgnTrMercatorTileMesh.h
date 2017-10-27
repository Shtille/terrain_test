#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TILE_MESH_H__
#define __MGN_TERRAIN_MERCATOR_TILE_MESH_H__

#include "../mgnTrMesh.h"

namespace mgn {
    namespace terrain {

    	//! Mercator tile mesh class
		class MercatorTileMesh : public Mesh {
		public:
			explicit MercatorTileMesh(graphics::Renderer * renderer, int grid_size);
			virtual ~MercatorTileMesh();

			void Create();

		private:
            void FillAttributes();

			const int grid_size_;
		};

    } // namespace terrain
} // namespace mgn

#endif