#include "mgnTrMercatorTaskLabels.h"

#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorProvider.h"

namespace mgn {
    namespace terrain {

        LabelsTask::LabelsTask(MercatorNode * node, MercatorProvider * provider)
        : Task(node, REQUEST_LABELS)
        , provider_(provider)
        , has_errors_(false)
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
            labels_info.labels_data = &labels_data_;
            labels_info.errors_occured = false;

            provider_->GetLabels(labels_info);

            has_errors_ = labels_info.errors_occured;
        }
        void LabelsTask::Process()
        {
            node_->OnLabelsTaskCompleted(labels_data_, has_errors_);
        }

    } // namespace terrain
} // namespace mgn