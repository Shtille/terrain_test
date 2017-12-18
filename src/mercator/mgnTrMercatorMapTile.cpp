#include "mgnTrMercatorMapTile.h"

#include "mgnTrMercatorTree.h"

#include "MapDrawing/Graphics/Renderer.h"

namespace mgn {
    namespace terrain {

    	MercatorMapTile::MercatorMapTile()
		: node_(NULL)
		, albedo_texture_(NULL)
        , heightmap_texture_(NULL)
		{
		}
        MercatorMapTile::MercatorMapTile(MercatorNode * node)
        : node_(node)
        , albedo_texture_(NULL)
        , heightmap_texture_(NULL)
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
		}
		MercatorNode * MercatorMapTile::GetNode()
		{
			return node_;
		}
		void MercatorMapTile::BindTexture()
		{
			graphics::Renderer * renderer = node_->owner_->renderer_;
			renderer->ChangeTexture(albedo_texture_, 0U);
            renderer->ChangeTexture(heightmap_texture_, 1U);
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
                renderer->AddTextureFromImage(heightmap_texture_, image, graphics::Texture::Wrap::kClampToEdge);
            }
        }

    } // namespace terrain
} // namespace mgn