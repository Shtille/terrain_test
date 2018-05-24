#include "mgnTrMercatorTaskIcons.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        IconsTask::IconsTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_ICONS)
        , provider_(provider)
        , has_errors_(false)
        {
        }
        IconsTask::~IconsTask()
        {
        }
        void IconsTask::Execute()
        {
            MercatorProvider::IconsInfo icons_info;
            icons_info.key_x = node_->x();
            icons_info.key_y = node_->y();
            icons_info.key_z = node_->lod();
            icons_info.icons_data = &icons_data_;
            icons_info.errors_occured = false;

            provider_->GetIcons(icons_info);

            has_errors_ = icons_info.errors_occured;
        }
        void IconsTask::Process()
        {
            node_->OnIconsTaskCompleted(icons_data_, has_errors_);
        }

    } // namespace terrain
} // namespace mgn