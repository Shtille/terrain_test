#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_HEIGHTMAP_H__
#define __MGN_TERRAIN_MERCATOR_TASK_HEIGHTMAP_H__

#include "mgnTrMercatorTask.h"

#include "MapDrawing/Graphics/mgnImage.h"

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        //! Heightmap task class
        class HeightmapTask : public Task {
        public:
            HeightmapTask(MercatorNode * node, MercatorProvider * provider);
            ~HeightmapTask();

            void Execute(); /* override */
            void Process(); /* override */

        private:
            MercatorProvider * provider_;
            graphics::Image image_;
        };

    } // namespace terrain
} // namespace mgn

#endif