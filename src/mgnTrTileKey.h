#pragma once

#include <map>

namespace mgn {
    namespace terrain {

        struct mgnTileKey
        {
            mgnTileKey (unsigned short magIndex_, int x_, int y_) : magIndex(magIndex_), x(x_), y(y_) {}
            unsigned short magIndex;
            int x, y;
        };

        inline bool operator<(const mgnTileKey& a, const mgnTileKey &b)
        {
            if (a.magIndex != b.magIndex) return a.magIndex < b.magIndex;
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;

            return false; // these keys are equal
        }

        class TerrainTile;
        typedef std::map<mgnTileKey, TerrainTile*> TileMap;

    } // namespace terrain
} // namespace mgn