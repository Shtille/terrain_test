#include "mgnTrMercatorTaskTexture.h"

#include "mgnTrMercatorNode.h"

namespace mgn {
    namespace terrain {

        TextureTask::TextureTask(MercatorNode * node)
        : Task(node, REQUEST_TEXTURE)
        {
        }
        TextureTask::~TextureTask()
        {
        }
        void TextureTask::Execute()
        {
            image_.Allocate(256, 256, graphics::Image::Format::kRGB8);
            // Render tile here
            image_.FillWithZeroes();
        }
        void TextureTask::Process()
        {
            node_->OnTextureTaskCompleted(image_);
        }

    } // namespace terrain
} // namespace mgn