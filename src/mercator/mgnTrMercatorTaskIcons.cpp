#include "mgnTrMercatorTaskIcons.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"
#include "mgnTrMercatorTileContext.h"

namespace mgn {
    namespace terrain {

        IconsTask::IconsTask(MercatorNode * node, MercatorProvider * provider,
            const mgnMdWorldPosition * gps_position)
        : Task(node, REQUEST_ICONS)
        , provider_(provider)
        , gps_position_(gps_position)
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

            const mgnMdTerrainView * terrain_view = node_->terrain_view();
            MercatorTileContext context(terrain_view, gps_position_,
                node_->x(), node_->y(), node_->lod());

            provider_->GetIcons(icons_info, context);

            has_errors_ = icons_info.errors_occured;
        }
        void IconsTask::Process()
        {
            node_->OnIconsTaskCompleted(icons_data_, has_errors_);
        }

    } // namespace terrain
} // namespace mgn