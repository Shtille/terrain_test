#include "mgnTrMercatorTaskLabels.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        LabelsTask::LabelsTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_LABELS)
        , provider_(provider)
        {
        }
        LabelsTask::~LabelsTask()
        {
        }
        void LabelsTask::Execute()
        {
            MercatorProvider::LabelsInfo labels_info;
            labels_info.key_x = node_->x();
            labels_info.key_y = node_->y();
            labels_info.key_z = node_->lod();
            // TODO
            labels_info.errors_occured = false;

            provider_->GetLabels(labels_info);
        }
        void LabelsTask::Process()
        {
            node_->OnTextureTaskCompleted(image_, false);
        }

    } // namespace terrain
} // namespace mgn