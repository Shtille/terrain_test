#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_PROVIDER_H__
#define __MGN_TERRAIN_MERCATOR_PROVIDER_H__

#include "MapDrawing/Graphics/mgnImage.h"

namespace mgn {
    namespace terrain {

        //! Mercator provider class interface
        class MercatorProvider {
        public:
            virtual ~MercatorProvider() {}

            struct TextureInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                graphics::Image * image;
                bool errors_occured;
            };

            virtual void GetTexture(TextureInfo & texture_info) = 0;
        };

    } // namespace terrain
} // namespace mgn

#endif