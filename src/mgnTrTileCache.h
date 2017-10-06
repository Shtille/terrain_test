#pragma once

#include "mgnTrTileKey.h"

#include <cstddef>

namespace mgn {
    namespace terrain {

        class TerrainTile;
        class mgnTerrainFetcher;

        class TileCache
        {
            size_t mCapacity;

            TileMap mTiles;

        public:
            TileCache(size_t capacity) : mCapacity(capacity) {}
            ~TileCache() { clear(); }

            void clear();

            // get tile from cache
            TerrainTile * getTile(const mgnTileKey &key);

            // Store tile to cache
            void addTile(TerrainTile *tile);

            // check if the tile is not in cache -- delete it
            void recycleTile(TerrainTile *tile);

            // Check count of stored tiles and remove tiles which was not used 
            void flushTiles(int frameCount, mgnTerrainFetcher  *fetcher);

            size_t size() const { return mTiles.size(); }

            //! Set highlight message flag
            void setHighlightMessage(int message);

            //! Refresh all textures in cache
            void RequestTextureUpdate();

            //! Refresh all user data in cache
            void RequestUserDataUpdate();
        };

    } // namespace terrain
} // namespace mgn