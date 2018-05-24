#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_LABELS_H__
#define __MGN_TERRAIN_MERCATOR_TASK_LABELS_H__

#include "mgnTrMercatorTask.h"

#include "mgnTrMercatorDataInfo.h"

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        //! Labels task class
        class LabelsTask : public Task {
        public:
            LabelsTask(MercatorNode * node, MercatorProvider * provider);
            ~LabelsTask();

            void Execute(); /* override */
            void Process(); /* override */

        private:
            MercatorProvider * provider_;
            std::vector<LabelData> labels_data_;
            bool has_errors_;
        };

    } // namespace terrain
} // namespace mgn

#endif