#include "mgnTrMercatorUtils.h"

#include <cmath>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

namespace
{
    const double kEarthRadius = 6378137.0;
    const double kMinLatitude = -85.05112878;
    const double kMaxLatitude = 85.05112878;
    const double kMinLongitude = -180;
    const double kMaxLongitude = 180;

    const double kPixelsPerCm = 37.79;
    double kZoomTable[] =
    {
        78271.5170*2,
        78271.5170,
        39135.7585,
        19567.8792,
        9783.9396,
        4891.9698,
        2445.9849,
        1222.9925,
        611.4962,
        305.7481,
        152.8741,
        76.4370,
        38.2185,
        19.1093,
        9.5546,
        4.7773,
        2.3887,
        1.1943,
        0.5972,
        0.2986,
        0.1493,
        0.0746,
        0.0373,
        0.0187
    };

    template <typename T>
    inline T Clip(T x, T min_x, T max_x)
    {
        return std::min<T>(std::max<T>(x, min_x), max_x);
    }
    inline int MapSize(int level_of_detail)
    {
        return 256 << level_of_detail;
    }
    void PixelXYToTileXY(int& tile_x, int& tile_y)
    {
        tile_x /= 256;
        tile_y /= 256;
    }
    double ClippedLongitude(double longitude)
    {
        double clipped_longitude = longitude;
        while (clipped_longitude < -180.0)
            clipped_longitude += 360.0;
        while (clipped_longitude >= 180.0)
            clipped_longitude -= 360.0;
        return clipped_longitude;
    }
}

namespace Mercator
{
    void LatLonToPixelXY(double latitude, double longitude, int level_of_detail, int& pixel_x, int& pixel_y)
    {
        latitude = Clip(latitude, kMinLatitude, kMaxLatitude);
        longitude = Clip(longitude, kMinLongitude, kMaxLongitude);

        double x = (longitude + 180.0) / 360.0;
        double sin_latitude = sin(latitude * M_PI / 180.0);
        double y = 0.5 - log((1.0 + sin_latitude) / (1.0 - sin_latitude)) / (4.0 * M_PI);

        int map_size = MapSize(level_of_detail);
        pixel_x = Clip((int)(x * (double)map_size + 0.5), 0, map_size - 1);
        pixel_y = Clip((int)(y * (double)map_size + 0.5), 0, map_size - 1);
    }
    void PixelXYToLatLon(int pixel_x, int pixel_y, int level_of_detail, double& latitude, double& longitude)
    {
        int map_size = MapSize(level_of_detail);
        double x = ((double)Clip(pixel_x, 0, map_size - 1) / (double)map_size) - 0.5;
        double y = 0.5 - ((double)Clip(pixel_y, 0, map_size - 1) / (double)map_size);

        latitude = 90.0 - 360.0 * atan(exp(-y * 2.0 * M_PI)) / M_PI;
        longitude = 360.0 * x;
    }
    void WorldPointToTileXY(double latitude, double longitude, int level_of_detail, int& key_x, int& key_y)
    {
        LatLonToPixelXY(latitude, ClippedLongitude(longitude), level_of_detail, key_x, key_y);
        PixelXYToTileXY(key_x, key_y);
    }
    void TileXYToQuadkey(int tile_x, int tile_y, int level_of_detail, std::string& quadkey)
    {
        for (int i = level_of_detail; i > 0; --i)
        {
            char digit = '0';
            int mask = 1 << (i - 1);
            if ((tile_x & mask) != 0)
            {
                digit++;
            }
            if ((tile_y & mask) != 0)
            {
                digit++;
                digit++;
            }
            quadkey.push_back(digit);
        }
    }
    void QuadkeyToTileXY(const std::string& quadkey, int& tile_x, int& tile_y, int& level_of_detail)
    {
        tile_x = tile_y = 0;
        level_of_detail = (int)quadkey.length();
        for (int i = level_of_detail; i > 0; i--)
        {
            int mask = 1 << (i - 1);
            switch (quadkey[level_of_detail - i])
            {
            case '0':
                break;

            case '1':
                tile_x |= mask;
                break;

            case '2':
                tile_y |= mask;
                break;

            case '3':
                tile_x |= mask;
                tile_y |= mask;
                break;

            default:
                assert(false);
            }
        }
    }
    int GetOptimalLevelOfDetail(double screen_pixel_size_x, int min_lod, int max_lod)
    {
        const double kInvLog2 = 1.442695040888963; // 1/ln(2)
        double ex = 1.40625/screen_pixel_size_x; // 360/256/pixel_size
        double log_value = log(ex) * kInvLog2;
        // To not do ceil we just use +1
        return Clip(static_cast<int>(log_value)+1, min_lod, max_lod);
    }
    double GetNativeScale(int level_of_detail)
    {
        return kZoomTable[level_of_detail] * kPixelsPerCm;
    }
}