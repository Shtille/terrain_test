#include "mgnTrMercatorNodeKey.h"

namespace mgn {
    namespace terrain {

        MercatorNodeKey::MercatorNodeKey(int lod, int x, int y)
        : lod(lod)
        , x(x)
        , y(y)
        {

        }
        bool MercatorNodeKey::operator < (const MercatorNodeKey& other)
        {
            if (lod != other.lod)
                return lod < other.lod;
            else if (x != other.x)
                return x < other.x;
            else
                return y < other.y;
        }
        bool MercatorNodeKey::operator ==(const MercatorNodeKey& other)
        {
            return lod == other.lod && x == other.x && y == other.y;
        }

    } // namespace terrain
} // namespace mgn