#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_STARTER_H__
#define __MGN_TERRAIN_MERCATOR_TASK_STARTER_H__

#include "mgnTrMercatorTask.h"

namespace mgn {
    namespace terrain {

        //! Starter task class
        class StarterTask : public Task {
        public:
            StarterTask(MercatorNode * node);
            ~StarterTask();

            void Execute(); /* override */
            void Process(); /* override */
        };

    } // namespace terrain
} // namespace mgn

#endif