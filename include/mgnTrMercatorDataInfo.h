#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_DATA_INFO_H__
#define __MGN_TERRAIN_MERCATOR_DATA_INFO_H__

#include <vector>
#include <string>

namespace mgn {
    namespace terrain {

        //! Label data structure
        struct LabelData {
            double latitude;
            double longitude;
            double altitude;

            int width;
            int height;
            bool centered;
            unsigned int shield_hash;
            std::wstring text;
            std::vector<unsigned char> bitmap_data; // for shield
        };

        //! Icon data structure
        struct IconData {
            double latitude;
            double longitude;
            double altitude;
        };

    } // namespace terrain
} // namespace mgn

#endif