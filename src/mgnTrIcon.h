#pragma once
#ifndef __MGN_TERRAIN_ICON_H__
#define __MGN_TERRAIN_ICON_H__

#include "mgnTrBillboard.h"

struct PointUserObjectInfo;

namespace mgn {
    namespace terrain {

        //! class for rendering icons
        class Icon : public Billboard {
        public:
            explicit Icon(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const vec3* tile_position, const PointUserObjectInfo &pui, unsigned short magIndex);
            virtual ~Icon();

            void render();

            void GetIconSize(vec2& size);
            int getID() const;

        private:
            void Create(const PointUserObjectInfo &pui, unsigned short magIndex);

            int mID; //!< icon ID for selection
        };

    } // namespace terrain
} // namespace mgn

#endif