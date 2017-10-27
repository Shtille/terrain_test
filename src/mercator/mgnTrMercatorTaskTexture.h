#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_TEXTURE_H__
#define __MGN_TERRAIN_MERCATOR_TASK_TEXTURE_H__

#include "mgnTrMercatorTask.h"

#include "MapDrawing/Graphics/mgnImage.h"

namespace mgn {
    namespace terrain {

        //! Texture task class
        class TextureTask : public Task {
        public:
            TextureTask(MercatorNode * node);
            ~TextureTask();

            void Execute(); /* override */
            void Process(); /* override */

        private:
            graphics::Image image_;
        };

    } // namespace terrain
} // namespace mgn

#endif