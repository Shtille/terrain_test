#include "mgnTrTileCache.h"
#include "mgnTrTerrainTile.h"
#include "mgnTrTerrainFetcher.h"

namespace mgn {
    namespace terrain {

        void TileCache::clear()
        {
            while (!mTiles.empty())
            {
                std::map<mgnTileKey, TerrainTile*>::iterator it = mTiles.begin();
                delete it->second;
                mTiles.erase(it);
            }
        }

        TerrainTile * TileCache::getTile(const mgnTileKey &key)
        {
            std::map<mgnTileKey, TerrainTile*>::iterator tile_it = mTiles.find(key);
            if (tile_it != mTiles.end())
                return tile_it->second;
            else
                return 0;
        }

        void TileCache::addTile(TerrainTile *tile)
        {
            if (tile) mTiles[tile->mKey] = tile;
        }

        void TileCache::recycleTile(TerrainTile *tile)
        {
            if (!tile) return;

            if (!getTile(tile->mKey))
                delete tile;
        }

        void TileCache::flushTiles(int frameCount, mgnTerrainFetcher  *fetcher)
        {
            if (mTiles.size() <= mCapacity*1.2)
                return;

            static std::vector<std::pair<int, mgnTileKey> > keys;
            keys.clear();
            // fill array "keys"
            {
                std::map<mgnTileKey, TerrainTile*>::iterator it;
                for (it = mTiles.begin(); it != mTiles.end(); ++it)
                {
                    TerrainTile *tile = it->second;
                    keys.push_back(std::make_pair(frameCount - tile->mDrawFrame, it->first));
                }
            }

            // partially sort array
            std::nth_element(keys.begin(), keys.begin() + mCapacity, keys.end());

            // remove array tail
            {
                std::vector<std::pair<int, mgnTileKey> >::iterator it;
                for (it = keys.begin() + mCapacity; it != keys.end(); ++it)
                {
                    if (it->first == 1 || it->first==0)
                    {
                        continue;
                    }
                    mgnTileKey key = it->second;
                    std::map<mgnTileKey, TerrainTile*>::iterator tile_it = mTiles.find(key);
                    if(fetcher->removeTile(tile_it->second))
                    {
                        delete tile_it->second;
                        mTiles.erase(tile_it);
                    }
                }
            }
            keys.clear();
        }

        void TileCache::setHighlightMessage(int message)
        {
            for (TileMap::iterator it = mTiles.begin(); it != mTiles.end(); ++it)
            {
                TerrainTile *tile = it->second;
                tile->mHighlightMessage = message;
            }
        }

        void TileCache::RequestTextureUpdate()
        {
            for (TileMap::iterator it = mTiles.begin(); it != mTiles.end(); ++it)
            {
                TerrainTile *tile = it->second;
                if (tile && tile->mIsFetchedTexture)
                    tile->mUpdateTexture = true;
            }
        }
        void TileCache::RequestUserDataUpdate()
        {
            for (TileMap::iterator it = mTiles.begin(); it != mTiles.end(); ++it)
            {
                TerrainTile *tile = it->second;
                if (tile && tile->mIsFetchedUserObjects)
                    tile->mUpdateUserData = true;
            }
        }

    } // namespace terrain
} // namespace mgn