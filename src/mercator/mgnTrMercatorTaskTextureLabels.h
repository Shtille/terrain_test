#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_TEXTURE_LABELS_H__
#define __MGN_TERRAIN_MERCATOR_TASK_TEXTURE_LABELS_H__

#include "mgnTrMercatorTask.h"

#include "mgnTrMercatorDataInfo.h"

#include "MapDrawing/Graphics/mgnImage.h"

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        //! Texture and labels task class
        class TextureLabelsTask : public Task {
        public:
            TextureLabelsTask(MercatorNode * node, MercatorProvider * provider);
            ~TextureLabelsTask();

            void Execute(); /* override */
            void Process(); /* override */

        private:
            MercatorProvider * provider_;
            graphics::Image image_;
            std::vector<LabelData> labels_data_;
            bool has_errors_;
        };

    } // namespace terrain
} // namespace mgn

#endif