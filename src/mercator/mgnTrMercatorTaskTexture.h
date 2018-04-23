#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_TEXTURE_H__
#define __MGN_TERRAIN_MERCATOR_TASK_TEXTURE_H__

#include "mgnTrMercatorTask.h"

#include "MapDrawing/Graphics/mgnImage.h"

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        //! Texture task class
        class TextureTask : public Task {
        public:
            TextureTask(MercatorNode * node, MercatorProvider * provider);
            ~TextureTask();

            void Execute(); /* override */
            void Process(); /* override */

        private:
            MercatorProvider * provider_;
            graphics::Image image_;
            bool has_errors_;
        };

    } // namespace terrain
} // namespace mgn

#endif