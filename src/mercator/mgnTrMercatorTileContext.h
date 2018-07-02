#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TILE_CONTEXT_H__
#define __MGN_TERRAIN_MERCATOR_TILE_CONTEXT_H__

#include "mgnMdIUserDataDrawContext.h"

class mgnMdTerrainView;

namespace mgn {
    namespace terrain {

        /*! Mercator tile context class
        */
        class MercatorTileContext : public mgnMdIUserDataDrawContext {
        public:
            MercatorTileContext(const mgnMdTerrainView * terrain_view, const mgnMdWorldPosition * gps_position,
                int key_x, int key_y, int key_z);

            mgnMdWorldRect GetWorldRect() const;
            mgnMdWorldPosition GetGPSPosition() const;
            int GetMapScaleIndex() const;
            float GetMapScaleFactor() const;
            bool ShouldDrawPOIs() const;
            bool ShouldDrawLabels() const;

        private:
            void Create(int key_x, int key_y, int key_z);

            const mgnMdTerrainView * terrain_view_;
            const mgnMdWorldPosition * gps_position_;
            int key_z_;
            mgnMdWorldRect world_rect_;
        };

    } // namespace terrain
} // namespace mgn

#endif