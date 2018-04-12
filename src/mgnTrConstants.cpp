#include "mgnTrConstants.h"

namespace mgn {
    namespace terrain {

        int GetTileResolution()
        {
            return 256;
        }
        int GetTileHeightSamples()
        {
            return 31;
        }
        const float GetHeightMin()
        {
            return -1000.0f;
        }
        const float GetHeightRange()
        {
            return 10000.0f;
        }
        const int GetHeightmapWidth()
        {
            return 65;
        }
        const int GetHeightmapHeight()
        {
            return 65;
        }

    } // namespace terrain
} // namespace mgn