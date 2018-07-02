#include "mgnTrMercatorTaskTextureLabels.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        TextureLabelsTask::TextureLabelsTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_TEXTURE)
        , provider_(provider)
        , has_errors_(false)
        {
        }
        TextureLabelsTask::~TextureLabelsTask()
        {
        }
        void TextureLabelsTask::Execute()
        {
            MercatorProvider::TextureLabelsInfo tl_info;
            tl_info.key_x = node_->x();
            tl_info.key_y = node_->y();
            tl_info.key_z = node_->lod();
            tl_info.image = &image_;
            tl_info.labels_data = &labels_data_;
            tl_info.need_image = true;
            tl_info.need_labels = true;
            tl_info.errors_occured = false;

            provider_->GetTextureAndLabels(tl_info);

            has_errors_ = tl_info.errors_occured;
        }
        void TextureLabelsTask::Process()
        {
            node_->OnTextureLabelsTaskCompleted(image_, labels_data_, has_errors_);
        }

    } // namespace terrain
} // namespace mgn