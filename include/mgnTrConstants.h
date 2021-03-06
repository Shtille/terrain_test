#pragma once
#ifndef __MGN_TERRAIN_CONSTANTS_H__
#define __MGN_TERRAIN_CONSTANTS_H__

namespace mgn {
    namespace terrain {

    int GetTileResolution();
    int GetTileHeightSamples();
    int GetMaxLod();
    int GetMapSizeMax();

    // Height map parameters
    const float GetHeightMin();
    const float GetHeightRange();
    const int GetHeightmapWidth();
    const int GetHeightmapHeight();

    } // namespace terrain
} // namespace mgn

#endif