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

    } // namespace terrain
} // namespace mgn