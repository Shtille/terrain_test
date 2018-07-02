#include "mgnTrMercatorTileContext.h"

#include "mgnTrMercatorUtils.h"
#include "mgnMdTerrainView.h"

namespace mgn {
    namespace terrain {

        MercatorTileContext::MercatorTileContext(const mgnMdTerrainView * terrain_view, const mgnMdWorldPosition * gps_position,
            int key_x, int key_y, int key_z)
        : terrain_view_(terrain_view)
        , gps_position_(gps_position)
        , key_z_(key_z)
        {
            Create(key_x, key_y, key_z);
        }
        mgnMdWorldRect MercatorTileContext::GetWorldRect() const
        {
            return world_rect_;
        }
        mgnMdWorldPosition MercatorTileContext::GetGPSPosition() const
        {
            return *gps_position_;
        }
        int MercatorTileContext::GetMapScaleIndex() const
        {
            return terrain_view_->GetMapScaleIndex(key_z_);
        }
        float MercatorTileContext::GetMapScaleFactor() const
        {
            return terrain_view_->GetMapScaleFactor(key_z_);
        }
        bool MercatorTileContext::ShouldDrawPOIs() const
        {
            return false;
        }
        bool MercatorTileContext::ShouldDrawLabels() const
        {
            return false;
        }
        void MercatorTileContext::Create(int key_x, int key_y, int key_z)
        {
            double latitude, longitude;
            Mercator::PixelXYToLatLon(256 * key_x, 256 * key_y, key_z, latitude, longitude);
            mgnMdWorldPoint upper_left(latitude, longitude);
            Mercator::PixelXYToLatLon(256 * (key_x + 1), 256 * (key_y + 1), key_z, latitude, longitude);
            mgnMdWorldPoint lower_right(latitude, longitude);
            world_rect_ = mgnMdWorldRect(upper_left, lower_right);
        }

    } // namespace terrain
} // namespace mgn