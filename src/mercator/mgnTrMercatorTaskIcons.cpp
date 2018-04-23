#include "mgnTrMercatorTaskIcons.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        IconsTask::IconsTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_ICONS)
        , provider_(provider)
        {
        }
        IconsTask::~IconsTask()
        {
        }
        void IconsTask::Execute()
        {
            MercatorProvider::TextureInfo texture_info;
            texture_info.key_x = node_->x();
            texture_info.key_y = node_->y();
            texture_info.key_z = node_->lod();
            texture_info.image = &image_;
            texture_info.errors_occured = false;

            provider_->GetTexture(texture_info);
        }
        void IconsTask::Process()
        {
            node_->OnTextureTaskCompleted(image_, false);
        }

    } // namespace terrain
} // namespace mgn