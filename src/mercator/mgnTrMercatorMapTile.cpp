#include "mgnTrMercatorMapTile.h"

#include "mgnTrMercatorTree.h"

#include "MapDrawing/Graphics/Renderer.h"

namespace mgn {
    namespace terrain {

    	MercatorMapTile::MercatorMapTile()
		: node_(NULL)
		, albedo_texture_(NULL)
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
			if (albedo_texture_)
			{
				graphics::Renderer * renderer = node_->owner_->renderer_;
				renderer->DeleteTexture(albedo_texture_);
				albedo_texture_ = NULL;
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
		}
        bool MercatorMapTile::HasAlbedoTexture() const
        {
            return albedo_texture_ != NULL;
        }
		void MercatorMapTile::SetImage(const graphics::Image& image)
		{
			if (albedo_texture_)
				albedo_texture_->SetData(0, 0, image.width(), image.height(), image.pixels());
			else
			{
				graphics::Renderer * renderer = node_->owner_->renderer_;
				renderer->AddTextureFromImage(albedo_texture_, image, graphics::Texture::Wrap::kClampToEdge);
			}
		}

    } // namespace terrain
} // namespace mgn