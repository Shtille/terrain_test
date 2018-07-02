#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_PROVIDER_H__
#define __MGN_TERRAIN_MERCATOR_PROVIDER_H__

#include "mgnTrMercatorDataInfo.h"

#include "MapDrawing/Graphics/mgnImage.h"

#include "mgnMdWorldPoint.h"

#include <vector>

class tnCDbTopo;
class mgnMdIUserDataDrawContext;

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
            struct HeightmapInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                graphics::Image * image;
                bool errors_occured;
            };
            struct LabelsInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                std::vector<LabelData> * labels_data;
                bool errors_occured;
            };
            struct TextureLabelsInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                graphics::Image * image;
                std::vector<LabelData> * labels_data;
                bool need_image;
                bool need_labels;
                bool errors_occured;
            };
            struct IconsInfo {
                int key_x, key_y, key_z; //!< tile coordinates in Mercator projection
                std::vector<IconData> * icons_data;
                bool errors_occured;
            };
            /*! Service struct for obtaining maneuver data */
            struct GuidanceArrow {
                float distance;                     //!< [in] distance, meters
                mgnMdWorldPoint point;              //!< [out] point of guidance arrow
                float heading;                      //!< [out] heading, radians
            };

            virtual double GetAltitude(double lat, double lng, const tnCDbTopo * topo = 0, float dxm = -1.f) = 0;

            virtual void GetTexture(TextureInfo & texture_info) = 0;
            virtual void GetHeightmap(HeightmapInfo & heightmap_info) = 0;
            virtual void GetLabels(LabelsInfo & labels_info) = 0;
            virtual void GetTextureAndLabels(TextureLabelsInfo & tl_info) = 0;
            virtual void GetIcons(IconsInfo & icons_info, const mgnMdIUserDataDrawContext& context) = 0;

            virtual bool FetchGuidanceArrow(const mgnMdIUserDataDrawContext& context, GuidanceArrow& arrow) = 0;
            virtual bool FetchRouteBegin(std::vector<mgnMdWorldPoint>& route_points) = 0;
            virtual bool FetchRouteEnd(std::vector<mgnMdWorldPoint>& route_points) = 0;
            virtual bool FetchGpsMmPoint(mgnMdWorldPoint * gps_point, mgnMdWorldPoint * mm_point) = 0;
        };

    } // namespace terrain
} // namespace mgn

#endif