#include "mgnTrMercatorMapTile.h"

#include "mgnTrMercatorTree.h"

#include "mgnTrConstants.h"

#include "MapDrawing/Graphics/Renderer.h"

namespace mgn {
    namespace terrain {

    	MercatorMapTile::MercatorMapTile()
		: node_(NULL)
		, albedo_texture_(NULL)
        , heightmap_texture_(NULL)
        , height_data_(NULL)
		{
		}
        MercatorMapTile::MercatorMapTile(MercatorNode * node)
        : node_(node)
        , albedo_texture_(NULL)
        , heightmap_texture_(NULL)
        , height_data_(NULL)
        {
        }
		MercatorMapTile::~MercatorMapTile()
		{
			Destroy();
		}
		void MercatorMapTile::Create(MercatorNode * node)
		{
			node_ = node;
		}
		void MercatorMapTile::Destroy()
		{
            graphics::Renderer * renderer = node_->owner_->renderer_;
			if (albedo_texture_)
			{
				renderer->DeleteTexture(albedo_texture_);
				albedo_texture_ = NULL;
			}
            if (heightmap_texture_)
            {
                renderer->DeleteTexture(heightmap_texture_);
                heightmap_texture_ = NULL;
            }
            if (height_data_)
            {
                delete[] height_data_;
                height_data_ = NULL;
            }
		}
		MercatorNode * MercatorMapTile::GetNode()
		{
			return node_;
		}
        const float * MercatorMapTile::GetHeightData() const
        {
            return height_data_;
        }
		void MercatorMapTile::BindTexture()
		{
			graphics::Renderer * renderer = node_->owner_->renderer_;
            // Set albedo texture
            if (albedo_texture_)
			    renderer->ChangeTexture(albedo_texture_, 0U);
            else
                renderer->ChangeTexture(node_->owner_->default_albedo_texture_, 0U);
            // Set height texture
            if (heightmap_texture_)
                renderer->ChangeTexture(heightmap_texture_, 1U);
            else
                renderer->ChangeTexture(node_->owner_->default_heightmap_texture_, 1U);
		}
        bool MercatorMapTile::HasAlbedoTexture() const
        {
            return albedo_texture_ != NULL;
        }
        bool MercatorMapTile::HasHeightmapTexture() const
        {
            return heightmap_texture_ != NULL;
        }
		void MercatorMapTile::SetAlbedoImage(const graphics::Image& image)
		{
			if (albedo_texture_)
				albedo_texture_->SetData(0, 0, image.width(), image.height(), image.pixels());
			else
			{
				graphics::Renderer * renderer = node_->owner_->renderer_;
				renderer->AddTextureFromImage(albedo_texture_, image, graphics::Texture::Wrap::kClampToEdge);
			}
		}
        void MercatorMapTile::SetHeightmapImage(const graphics::Image& image)
        {
            if (heightmap_texture_)
                heightmap_texture_->SetData(0, 0, image.width(), image.height(), image.pixels());
            else
            {
                graphics::Renderer * renderer = node_->owner_->renderer_;
                renderer->AddTextureFromImage(heightmap_texture_, image,
                    graphics::Texture::Wrap::kClampToEdge, graphics::Texture::Filter::kLinear, false);
            }
            FillHeightData(image);
        }
        void MercatorMapTile::FillHeightData(const graphics::Image& image)
        {
            const float kHeightMin = mgn::terrain::GetHeightMin();
            const float kHeightRange = mgn::terrain::GetHeightRange();

            if (height_data_)
                delete[] height_data_;
            height_data_ = new float[image.width() * image.height()];

            for (int j = 0; j < image.height(); ++j)
            {
                for (int i = 0; i < image.width(); ++i)
                {
                    // Unpack float value from two bytes
                    int index = j * image.width() + i;
                    const unsigned char * data = image.pixels() + (index * image.bpp());
                    unsigned int x = static_cast<unsigned int>(data[0]);
                    unsigned int y = static_cast<unsigned int>(data[1]);
                    float norm_height = static_cast<float>(255 * x + y) / 65535.0f;
                    float height = kHeightMin + kHeightRange * norm_height;
                    height_data_[index] = height;
                }
            }
        }

    } // namespace terrain
} // namespace mgn