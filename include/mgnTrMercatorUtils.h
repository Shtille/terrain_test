#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_UTILS_H__
#define __MGN_TERRAIN_MERCATOR_UTILS_H__

#include <string>

// Mercator tile system functions
namespace Mercator
{
    void LatLonToPixelXY(double latitude, double longitude, int level_of_detail, int& pixel_x, int& pixel_y);
    void PixelXYToLatLon(int pixel_x, int pixel_y, int level_of_detail, double& latitude, double& longitude);
    void WorldPointToTileXY(double latitude, double longitude, int level_of_detail, int& key_x, int& key_y);

    void TileXYToQuadkey(int tile_x, int tile_y, int level_of_detail, std::string& quadkey);
    void QuadkeyToTileXY(const std::string& quadkey, int& tile_x, int& tile_y, int& level_of_detail);

    int GetOptimalLevelOfDetail(double screen_pixel_size_x, int min_lod, int max_lod);

    double GetNativeScale(int level_of_detail);
}

#endif