#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_DATA_INFO_H__
#define __MGN_TERRAIN_MERCATOR_DATA_INFO_H__

#include "mgnMdMapObjectInfo.h"

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

            int width; // texture width
            int height; // texture height
            int id; //!< ID of user data object (need for selection)
            mgnMdMapObjectInfo poi_info;
            std::vector<unsigned char> bitmap_data;
            size_t hash;
            bool is_poi;
            bool centered;
        };

        struct HighlightData {
            // Messages for notification
            enum {
                kNoMessages = 0,
                kAddRoute = 1,
                kRemoveRoute = 2
            };
        };

    } // namespace terrain
} // namespace mgn

#endif