#include "mgnTrMercatorTaskTexture.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        TextureTask::TextureTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_TEXTURE)
        , provider_(provider)
        {
        }
        TextureTask::~TextureTask()
        {
        }
        void TextureTask::Execute()
        {
            /*
            Note: Since we use LHS in 3D rendering, Z axis points North,
            an opposite direction to Mercator Y axis.
            Thus we should convert (x,y,z) of our quad tree to (x,y,z) of Mercator tile.
            */
            int tiles_per_side = 1 << node_->lod();
            MercatorProvider::TextureInfo texture_info;
            texture_info.key_x = node_->x();
            texture_info.key_y = node_->y();//tiles_per_side - node_->y();
            texture_info.key_z = node_->lod();
            texture_info.image = &image_;
            texture_info.errors_occured = false;

            provider_->GetTexture(texture_info);

            // image_.Allocate(256, 256, graphics::Image::Format::kRGB8);
            // image_.FillWithZeroes();
        }
        void TextureTask::Process()
        {
            node_->OnTextureTaskCompleted(image_);
        }

    } // namespace terrain
} // namespace mgn