#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_MAP_TILE_H__
#define __MGN_TERRAIN_MERCATOR_MAP_TILE_H__

namespace mgn {
	namespace graphics {
		class Texture;
		class Image;
	} // namespace graphics
} // namespace mgn

namespace mgn {
    namespace terrain {

    	// Forward declarations
		class MercatorNode;

		//! Mercator map tile class
		class MercatorMapTile {
		public:
			MercatorMapTile(MercatorNode * node, graphics::Texture * albedo_texture);
			~MercatorMapTile();

			MercatorNode * GetNode();
			void BindTexture();

			void SetImage(const graphics::Image& image);

		private:
			MercatorNode * node_;
			graphics::Texture * albedo_texture_;
		};

    } // namespace terrain
} // namespace mgn

#endif