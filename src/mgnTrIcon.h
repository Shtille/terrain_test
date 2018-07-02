#pragma once
#ifndef __MGN_TERRAIN_ICON_H__
#define __MGN_TERRAIN_ICON_H__

#include "mgnTrBillboard.h"
#include "mgnMdMapObjectInfo.h"

struct PointUserObjectInfo;

namespace mgn {
    namespace terrain {

        struct IconData;

        //! class for rendering icons
        class Icon : public Billboard {
        public:
            explicit Icon(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const IconData& data, int lod);
            virtual ~Icon();

            void render();

            void GetIconSize(vec2& size);
            int getID() const;
            const mgnMdMapObjectInfo& getMapObjectInfo() const;
            bool isPOI() const;

        private:
            void Create(const IconData& data, int lod);

            int mID; //!< icon ID for selection
            mgnMdMapObjectInfo mMapObjectInfo; //!< map object info for POI
            bool mIsPOI;
        };

    } // namespace terrain
} // namespace mgn

#endif