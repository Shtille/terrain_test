#include "mgnTrMercatorTaskHeightmap.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        HeightmapTask::HeightmapTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_HEIGHTMAP)
        , provider_(provider)
        , has_errors_(false)
        {
        }
        HeightmapTask::~HeightmapTask()
        {
        }
        void HeightmapTask::Execute()
        {
            MercatorProvider::HeightmapInfo heightmap_info;
            heightmap_info.key_x = node_->x();
            heightmap_info.key_y = node_->y();
            heightmap_info.key_z = node_->lod();
            heightmap_info.image = &image_;
            heightmap_info.errors_occured = false;

            provider_->GetHeightmap(heightmap_info);

            has_errors_ = heightmap_info.errors_occured;
        }
        void HeightmapTask::Process()
        {
            node_->OnHeightmapTaskCompleted(image_, has_errors_);
        }

    } // namespace terrain
} // namespace mgn