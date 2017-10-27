#include "mgnTrMercatorMapTile.h"

#include "mgnTrMercatorTree.h"

#include "MapDrawing/Graphics/Renderer.h"

namespace mgn {
    namespace terrain {

    	MercatorMapTile::MercatorMapTile(MercatorNode * node, graphics::Texture * albedo_texture)
		: node_(node)
		, albedo_texture_(albedo_texture)
		{
		}
		MercatorMapTile::~MercatorMapTile()
		{
			graphics::Renderer * renderer = node_->owner_->renderer_;
			renderer->DeleteTexture(albedo_texture_);
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