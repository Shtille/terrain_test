#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_ICONS_H__
#define __MGN_TERRAIN_MERCATOR_TASK_ICONS_H__

#include "mgnTrMercatorTask.h"

#include "mgnTrMercatorDataInfo.h"

class mgnMdWorldPosition;

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        //! Icons task class
        class IconsTask : public Task {
        public:
            IconsTask(MercatorNode * node, MercatorProvider * provider,
                const mgnMdWorldPosition * gps_position);
            ~IconsTask();

            void Execute(); /* override */
            void Process(); /* override */

        private:
            MercatorProvider * provider_;
            const mgnMdWorldPosition * gps_position_;
            std::vector<IconData> icons_data_;
            bool has_errors_;
        };

    } // namespace terrain
} // namespace mgn

#endif