#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_PROVIDER_H__
#define __MGN_TERRAIN_MERCATOR_PROVIDER_H__

#include "MapDrawing/Graphics/mgnImage.h"

#include <vector>
#include <string>

namespace mgn {
    namespace terrain {

        struct LabelInfo {
            double latitude;
            double longitude;
            double altitude;

            bool centered;
            unsigned int shield_hash;
            std::wstring text;

            // Shield information
            int bitmap_width;
            int bitmap_height;
            std::vector<unsigned char> bitmap_data;
        };

        //! Mercator provider class interface
        class MercatorProvider {
        public:
            virtual ~MercatorProvider() {}

            struct TextureInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                graphics::Image * image;
                bool errors_occured;
            };
            struct HeightmapInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                graphics::Image * image;
                bool errors_occured;
            };
            struct LabelsInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                std::vector<LabelInfo> infos;
                bool errors_occured;
            };
            struct IconsInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                graphics::Image * image;
                bool errors_occured;
            };

            virtual void GetTexture(TextureInfo & texture_info) = 0;
            virtual void GetHeightmap(HeightmapInfo & heightmap_info) = 0;
            virtual void GetLabels(LabelsInfo & labels_info) = 0;
            virtual void GetIcons(IconsInfo & icons_info) = 0;
        };

    } // namespace terrain
} // namespace mgn

#endif