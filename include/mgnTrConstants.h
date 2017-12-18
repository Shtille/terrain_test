#pragma once
#ifndef __MGN_TERRAIN_CONSTANTS_H__
#define __MGN_TERRAIN_CONSTANTS_H__

namespace mgn {
    namespace terrain {

    int GetTileResolution();
    int GetTileHeightSamples();

    const float GetHeightMin();
    const float GetHeightRange();

    } // namespace terrain
} // namespace mgn

#endif