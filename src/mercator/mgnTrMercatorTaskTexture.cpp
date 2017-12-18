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
            MercatorProvider::TextureInfo texture_info;
            texture_info.key_x = node_->x();
            texture_info.key_y = node_->y();
            texture_info.key_z = node_->lod();
            texture_info.image = &image_;
            texture_info.errors_occured = false;

            provider_->GetTexture(texture_info);
        }
        void TextureTask::Process()
        {
            node_->OnTextureTaskCompleted(image_);
        }

    } // namespace terrain
} // namespace mgn