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
			MercatorMapTile();
            MercatorMapTile(MercatorNode * node);
			~MercatorMapTile();

			void Create(MercatorNode * node);
			void Destroy();

			MercatorNode * GetNode();
            const float * GetHeightData() const;
			void BindTexture();

            bool HasAlbedoTexture() const;
            bool HasHeightmapTexture() const;

			void SetAlbedoImage(const graphics::Image& image);
            void SetHeightmapImage(const graphics::Image& image);

		private:
            void FillHeightData(const graphics::Image& image);

			MercatorNode * node_; //!< pointer to owner node
			graphics::Texture * albedo_texture_;
            graphics::Texture * heightmap_texture_;
            float * height_data_;
		};

    } // namespace terrain
} // namespace mgn

#endif