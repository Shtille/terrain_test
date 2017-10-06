#include "mgnTrTerrainMap.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

#include "mgnTrTerrainFetcher.h"
#include "mgnTrTileCache.h"
#include "mgnTrIcon.h"
#include "mgnTrHighlightTrackRenderer.h"
#include "mgnTrFontAtlas.h"

#include "mgnOmManager.h"

#include "mgnMdIUserDataDrawContext.h"

#include "mgnLog.h"

//#define LOG_STAT_3D
#ifdef LOG_STAT_3D
#ifdef ANDROID
#include <android/log.h>
#define LOG_ME(s, d) __android_log_print(ANDROID_LOG_DEBUG,"TILECACHE","%s:%d",s,d)
#else
#include "mgnLog.h"
#define LOG_ME(s,d) LOG_INFO(0, ("TILECACHE %s:%d", s, d));
#endif
#else
#define LOG_ME(s, d)
#endif

#if defined(ANDROID) || defined(LINUX)
#include <sys/sysinfo.h>
#elif defined(MAC)
// TODO: include for MAC
#endif
static size_t getTileCacheCapacity()
{
    static size_t capacity = 0;
    if (!capacity)
    {
#if defined(ANDROID) || defined(LINUX)
        struct sysinfo info;
        sysinfo (&info);
        // LOG_INFO( 0, ( "MK_DEBUG: total ram:    %d", info.totalram  ) );
        // LOG_INFO( 0, ( "MK_DEBUG: free ram:     %d", info.freeram   ) );
        // LOG_INFO( 0, ( "MK_DEBUG: shared ram:   %d", info.sharedram ) );
        // LOG_INFO( 0, ( "MK_DEBUG: buffer ram:   %d", info.bufferram ) );
        // LOG_INFO( 0, ( "MK_DEBUG: total swap:   %d", info.totalswap ) );
        // LOG_INFO( 0, ( "MK_DEBUG: free swap:    %d", info.freeswap  ) );
        // LOG_INFO( 0, ( "MK_DEBUG: total height: %d", info.totalhigh ) );
        // LOG_INFO( 0, ( "MK_DEBUG: free height:  %d", info.freehigh  ) );
        // LOG_INFO( 0, ( "MK_DEBUG: mem_unit:     %d", info.mem_unit  ) );

        // TODO: check values
        if      (info.totalram > 500000000)
            capacity = 180;
        else if (info.totalram > 300000000)
            capacity = 120;
        else
            capacity = 90;
#elif defined(MAC)
        // TODO: need algorithm for MAC
        capacity = 120;
#else
        capacity = 180;
#endif
        LOG_INFO( 0, ( "MK_DEBUG: Tile cache capacity: %d", capacity) );
    }

    return capacity;
}

namespace mgn {
    namespace terrain {

        TerrainMap::TerrainMap(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * terrain_provider,
            graphics::Shader * terrain_shader, graphics::Shader * billboard_shader,
            const mgnMdWorldPosition * gps_pos,
            const char * font_path, const float pixel_scale)
          : renderer_(renderer)
          , mTerrainView(terrain_view)
          , mTerrainProvider(*terrain_provider)
          , mTerrainShader(terrain_shader)
          , mBillboardShader(billboard_shader)
          , pGpsPosition(gps_pos)
          , mFrameCount(0)
          , pHighlightTrackRenderer(NULL)
        {
            mActiveTSParams = &mTileSetParams[0];

            mTileCache = new TileCache(getTileCacheCapacity());

            mFetcher = new mgnTerrainFetcher();

            // Sizes taken from RenderToolManagerGL.cpp
            const int size3 = static_cast<int>(18.0f /** pixel_scale*/ + 0.5f);
            const int border = static_cast<int>(3.0f /*+ pixel_scale*/ + 0.5f);
            mOutlinedFont = Font::CreateWithBorder(renderer, font_path, size3, border,
                Color(0.0f, 0.0f, 0.0f), Color(1.0f, 1.0f, 1.0f), pixel_scale);

            mOnlineRasterMapsEnabled = mgn::online_map::Manager::GetInstance()->IsEnabled();

            mgnCriticalSectionInitialize(&mIconListCriticalSection);
        }

        TerrainMap::~TerrainMap()
        {
            mgnCriticalSectionDelete(&mIconListCriticalSection);

            for (IconTextureCache::iterator it = mIconTextureCache.begin(); it != mIconTextureCache.end(); ++it)
            {
                graphics::Texture * texture = it->second;
                if (texture)
                    renderer_->DeleteTexture(texture);
            }
            mIconTextureCache.clear();

            for (ShieldTextureCache::iterator it = mShieldTextureCache.begin(); it != mShieldTextureCache.end(); ++it)
            {
                graphics::Texture * texture = it->second;
                if (texture)
                    renderer_->DeleteTexture(texture);
            }
            mShieldTextureCache.clear();

            delete mOutlinedFont;

            delete mFetcher; mFetcher = NULL;
            clearTiles();
            delete mTileCache; mTileCache = NULL;
        }

        void TerrainMap::Update()
        {
            mgnTerrainFetcher::CommandList fetchedTiles;
            mFetcher->getResults(fetchedTiles);

            if (fetchedTiles.empty())
                return;

            while (!fetchedTiles.empty())
            {
                mgnTerrainFetcher::CommandData cmdData = fetchedTiles.back();
                fetchedTiles.pop_back();

                TerrainTile *tile = cmdData.tile;
                mgnTerrainFetcher::CommandType cmd = cmdData.cmd;

                //tile->mDrawFrame = mFrameCount;

                switch (cmd)
                {
                case mgnTerrainFetcher::FETCH_TERRAIN:
                    tile->generateMesh();
                    tile->mIsFetchedTerrain = true;
                    if (!tile->mIsFetchedTexture || tile->mRefetchTexture)
                    {
                        mFetcher->addCommand(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::FETCH_TEXTURE));
                        break;
                    }
                case mgnTerrainFetcher::FETCH_TEXTURE:
                    tile->generateTextures();
                    // TODO: We may simply exchange this shit on additional fetcher command (FETCH_LABELS)
                    if (tile->mNeedToGenerateLabels && tile->isFetchedLabels())
                        tile->generateLabels();
                    tile->mIsFetchedTexture = true;
                    if (tile->mRefetchTerrain)
                        mFetcher->addCommand(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::FETCH_TERRAIN));
                    else
                    {
                        mFetcher->addCommand(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::FETCH_USER_DATA));
                        if (tile->mRefetchTexture)
                            mFetcher->addCommandLow(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::REFETCH_TEXTURE));
                    }
                    break;
                case mgnTerrainFetcher::FETCH_USER_DATA:
                    tile->mIsFetchedUserObjects = true;
                    tile->mIsFetched = true;
                    tile->generateUserObjects();
                    updateIconList(); // manual updating of icons when some new data appears
                    if(!tile->mRefetchTexture)
                        mTileCache->addTile(tile);
                    break;
                case mgnTerrainFetcher::UPDATE_TRACKS:
                    tile->mHighlightTrackChunk->mIsFetched = true;
                    break;
                case mgnTerrainFetcher::REFETCH_TEXTURE:
                    tile->generateTextures();
                    if (tile->mNeedToGenerateLabels && tile->isFetchedLabels())
                        tile->generateLabels();
                    tile->mIsFetchedTexture = true;
                    tile->mUpdateTextureRequested = false;
                    if (tile->mRefetchTexture)
                        mFetcher->addCommandLow(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::REFETCH_TEXTURE));
                    else
                        mTileCache->addTile(tile);
                    break;
                case mgnTerrainFetcher::REFETCH_USER_DATA:
                    tile->freeUserObjects(); // free any allocated user objects to renew with new ones
                    tile->generateUserObjects();
                    updateIconList(); // manual updating of icons when some new data appears
                    tile->mUpdateUserDataRequested = false;
                    break;
                case mgnTerrainFetcher::UNSPECIFIED:
                default:
                    assert(0 && "ERROR: Unexpected command");
                }
            }

            mTileCache->flushTiles(mFrameCount, mFetcher);
        }

        void TerrainMap::IntersectionWithRay(const vec3& ray, vec3& intersection) const
        {
            if (mTerrainView->getCamTiltRad() > 1.0 && // High altitudes, use approximate approach
                ray.y < 0.0f)
            {
                vec3 origin = mTerrainView->getCamPosition();
                float d = -(float)mTerrainView->getCenterHeight();
                float t = -(origin.y + d)/ray.y;
                intersection = origin + t * ray;
                return;
            }
            mgnMdWorldPoint world_point;
            float distance = mTerrainView->getLargestCamDistance();
            vec3 left_point = mTerrainView->getCamPosition();
            vec3 right_point = left_point + distance * ray;
            mTerrainView->LocalToWorld(left_point.x, left_point.z, world_point);
            float left_height = (float)mTerrainProvider.getAltitude(world_point.mLatitude, world_point.mLongitude);
            mTerrainView->LocalToWorld(right_point.x, right_point.z, world_point);
            float right_height = (float)mTerrainProvider.getAltitude(world_point.mLatitude, world_point.mLongitude);
            if ((left_point.y - left_height)*(right_point.y - right_height) < 0.0f)
            {
                do
                {
                    vec3 center_point = 0.5f*(left_point + right_point);
                    mTerrainView->LocalToWorld(center_point.x, center_point.z, world_point);
                    float center_height = (float)mTerrainProvider.getAltitude(world_point.mLatitude, world_point.mLongitude);
                    if ((left_point.y - left_height)*(center_point.y - center_height) < 0.0f)
                    {
                        right_point = center_point;
                        right_height = center_height;
                    }
                    else
                    {
                        left_point = center_point;
                        left_height = center_height;
                    }
                    distance *= 0.5f; // equal to / 2
                }
                while (distance > 1.0f);

                // We don't need an accurate value, so just take middle
                intersection = 0.5f*(left_point + right_point);
            }
            else // no possible intersection
            {
                distance = (float)mTerrainView->getCamDistance();
                intersection = left_point + distance * ray;
            }
        }

        void TerrainMap::render(const math::Frustum& frustum)
        {
            mFrameCount++;

            mgnMdWorldPoint location = mTerrainView->getViewPoint();
            renderTiles(location, *mActiveTSParams, frustum);

            flushTiles();
        }

        void TerrainMap::flushTiles()
        {
            LOG_ME("+flushTiles",mActiveTSParams->tileMap.size());
            // Icons are linked to current rendering tiles.
            // If camera position doesn't change there's a chance to get non valid tiles on icons
            bool need_resort_icons = false;
            LOG_ME("mFrameCount",mFrameCount);
            for (TileMap::iterator it=mActiveTSParams->tileMap.begin(); it!=mActiveTSParams->tileMap.end();)
            {
                TerrainTile *tile=it->second;

                bool remove=( mFrameCount-tile->mDrawFrame>0);
                LOG_ME("mDrawFrame",tile->mDrawFrame);
                if (remove)
                {
                    need_resort_icons = true;
                    if (mFetcher->removeTile(tile))
                    {
                        LOG_ME("remove success",1);
                        recycleTile(it, mActiveTSParams);
                    }
                    else
                    {
                        LOG_ME("remove failed",0);
                        tile->mDrawFrame = mFrameCount+1;
                    }
                }
                else
                {
                    LOG_ME("do not remove",0);
                    it++;
                }
            }
            if (need_resort_icons)
                updateIconList();
            LOG_ME("-flushTiles",mActiveTSParams->tileMap.size());
        }

        void TerrainMap::clearTiles()
        {
            mActiveTSParams->clear(this);
        }

        void TerrainMap::recycleTile(TileMap::iterator &it, TileSetParams *ts)
        {
            TerrainTile *tile = it->second;
            mTileCache->recycleTile(tile);
            TileMap::iterator it_deletable = it; ++it;
            ts->tileMap.erase(it_deletable);
        }

        TerrainTile * TerrainMap::findTile( mgnTileKey tilekey)
        {
            return mTileCache->getTile(tilekey);
        }

        TerrainTile * TerrainMap::createNewTile( TileMap &tileMap, mgnTileKey tilekey, bool &created )
        {
            created = false;

            TileMap::iterator it = tileMap.find(tilekey);
            if (it != tileMap.end()) return it->second;

            TerrainTile * tile = findTile(tilekey);
            if (!tile)
            {
                mgnMdWorldRect rc = mTerrainView->getTileRect(tilekey.magIndex, tilekey.x, tilekey.y);
                tile = new TerrainTile(this, tilekey, GeoSquare(rc), 0, 0);
                created = true;

                // start tile initialization from terrain fetching
                mFetcher->addCommand(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::FETCH_TERRAIN));
            }

            tileMap[tilekey]=tile;

            return tile;
        }

        void TerrainMap::updateTiles(mgnMdWorldPoint &location, TileSetParams &ts)
        {
            const mgnMdTerrainView * terrainView = getTerrainView();

            unsigned short magIndex = terrainView->getMagIndex();
            TPointInt centerCell = terrainView->getCenterCell(magIndex);
            int x = centerCell.x;
            int y = centerCell.y;

            double m2deg_lat = 1/terrainView->getMetersPerLatitude();
            double m2deg_lon = 1/terrainView->getMetersPerLongitude();

            int heading_int = static_cast<int>(terrainView->getCamHeadingRad()*2);

            // Check if parameters have been changed (if not -- skip)
            if (    ts.magIndex == magIndex
                 && ts.tx == x
                 && ts.ty == y
                 && ts.heading == heading_int ) return;

            ts.magIndex = magIndex;
            ts.tx = x;
            ts.ty = y;
            ts.heading = heading_int;

            ts.tileKeys.clear();

            mgnMdWorldPoint lbCorner;

            // heading correction general params
            static volatile double tmp__scale = 1;
            double headingCorrectionScale = cos(terrainView->getCamTiltRad())*tmp__scale;
            double headingStep_lat = cos(terrainView->getCamHeadingRad()) * headingCorrectionScale;
            double headingStep_lon = sin(terrainView->getCamHeadingRad()) * headingCorrectionScale;

            magIndex += 5;
            for (int i=0; i<5; ++i, --magIndex)
            {
                lbCorner = location;
                double cellsz_lat = terrainView->getCellSizeLat(magIndex);
                double cellsz_lon = terrainView->getCellSizeLat(magIndex);

                if (i != 0)
                {
                    // mandatory correction
                    lbCorner.mLatitude  -= cellsz_lat*m2deg_lat/2;
                    lbCorner.mLongitude -= cellsz_lon*m2deg_lon/2;

                    // heading correction
                    if (magIndex != ts.magIndex)
                    {
                        lbCorner.mLatitude  += cellsz_lat*m2deg_lat*headingStep_lat;
                        lbCorner.mLongitude += cellsz_lon*m2deg_lon*headingStep_lon;
                    }
                }

                centerCell = terrainView->getCell(magIndex, lbCorner.mLatitude, lbCorner.mLongitude);
                x = centerCell.x;
                y = centerCell.y;

                if (i==0)
                {
                    // single very low-detailed tile
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y+0));
                    --magIndex;
                    continue;
                }

                ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y+0));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y+1));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+1, y+1));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+1, y+0));

                ts.tileKeys.push_back(mgnTileKey(magIndex, x-1, y+2));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y+2));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+1, y+2));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+2, y+2));

                ts.tileKeys.push_back(mgnTileKey(magIndex, x+2, y+1));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+2, y+0));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+2, y-1));

                ts.tileKeys.push_back(mgnTileKey(magIndex, x+1, y-1));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y-1));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x-1, y-1));

                ts.tileKeys.push_back(mgnTileKey(magIndex, x-1, y+0));
                ts.tileKeys.push_back(mgnTileKey(magIndex, x-1, y+1));

                if (magIndex == ts.magIndex)
                {
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-2, y+3));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-1, y+3));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y+3));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+1, y+3));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+2, y+3));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+3, y+3));

                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+3, y+2));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+3, y+1));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+3, y+0));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+3, y-1));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+3, y-2));

                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+2, y-2));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+1, y-2));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x+0, y-2));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-1, y-2));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-2, y-2));

                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-2, y-1));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-2, y+0));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-2, y+1));
                    ts.tileKeys.push_back(mgnTileKey(magIndex, x-2, y+2));
                }
            }

            bool has_created = false, created = false;
            // Create tiles
            size_t sz = ts.tileKeys.size();
            for (size_t keyind=0; keyind<sz; ++keyind)
            {
                const mgnTileKey &tilekey = ts.tileKeys[keyind];

                TerrainTile *tile = createNewTile(ts.tileMap, tilekey, created);
                has_created = has_created || created;
                tile->priority = sz-keyind;
            }

            // Update tile sorting in queue to fetch
            if (has_created) mFetcher->resort();
        }

        void TerrainMap::renderTiles(mgnMdWorldPoint &location, TileSetParams &ts,
            const math::Frustum& frustum)
        {
            renderedTiles.clear();

            float heading = (float)mTerrainView->getCamHeadingRad();

            float tLatZ = cos(heading);
            float tLonZ = -sin(heading);

            double lat2Local = getTerrainView()->getMetersPerLatitude();
            double lon2Local = getTerrainView()->getMetersPerLongitude();
            LOG_ME("+Tilemap",ts.tileMap.size());
            updateTiles(location, ts);
            LOG_ME("updated tilemap",ts.tileMap.size());
            // Push highlight messages
            // Why do we do this shit? Some tiles may be in cache during route recalculation.
            int message = mTerrainProvider.dequeueHighlightMessage();
            if (message != GeoHighlight::kNoMessages)
            {
                mTileCache->setHighlightMessage(message);
                for (TileMap::const_iterator it = ts.tileMap.begin(); it != ts.tileMap.end(); ++it)
                    it->second->mHighlightMessage = message;
                mTerrainProvider.clearHighlightMessages();
            }

            // Whether we need to refetch textures
            bool need_refetch_textures = false;
            bool orm_enabled = mgn::online_map::Manager::GetInstance()->IsEnabled();
            if (orm_enabled != mOnlineRasterMapsEnabled)
            {
                mOnlineRasterMapsEnabled = orm_enabled;
                need_refetch_textures = true;
            }
            bool is_update_requested = mTerrainProvider.IsTextureUpdateRequested();
            if (is_update_requested)
            {
                mTerrainProvider.ClearTextureUpdateRequests();
                need_refetch_textures = true;
            }
            if (need_refetch_textures)
            {
                mTileCache->RequestTextureUpdate();
                for (TileMap::const_iterator it = ts.tileMap.begin(); it != ts.tileMap.end(); ++it)
                {
                    TerrainTile *tile = it->second;
                    if (tile && tile->mIsFetchedTexture)
                        tile->mUpdateTexture = true;
                }
            }
            bool is_user_data_update_requested = mTerrainProvider.IsUserDataUpdateRequested();
            if (is_user_data_update_requested)
            {
                mTerrainProvider.ClearUserDataUpdateRequests();
                mTileCache->RequestUserDataUpdate();
                for (TileMap::const_iterator it = ts.tileMap.begin(); it != ts.tileMap.end(); ++it)
                {
                    TerrainTile *tile = it->second;
                    if (tile && tile->mIsFetchedUserObjects)
                        tile->mUpdateUserData = true;
                }
            }

            for (size_t keyind=0; keyind<ts.tileKeys.size(); ++keyind)
            {
                if (keyind==0 || ts.tileKeys[keyind].magIndex != ts.tileKeys[keyind-1].magIndex)
                    renderer_->ClearDepthBuffer();
                
                const mgnTileKey &tilekey = ts.tileKeys[keyind];
                unsigned short magIndex = tilekey.magIndex;
                int x = tilekey.x;
                int y = tilekey.y;

                float tilez=tLonZ*x+tLatZ*y;

                TerrainTile *tile;
                TileMap::const_iterator tit = ts.tileMap.find(tilekey);
                if (tit == ts.tileMap.end()) continue;
                else tile = tit->second;

                if (tile)
                {
                    if(tile->mTerrainMesh)
                    {
                        mgnMdWorldPoint centerPt = getTerrainView()->getCenter(magIndex);
                        double xm = (location.mLongitude - centerPt.mLongitude) * lon2Local;
                        double ym = (location.mLatitude  - centerPt.mLatitude ) * lat2Local;
                        TPointInt centerCell = getTerrainView()->getCenterCell(magIndex);
                        int tx = centerCell.x;
                        int ty = centerCell.y;

                        // gx and gy are changing over time and should be updated
                        // float gx = float(x-tx)*tile->sizeMetersLon*1.01 - float(xm);
                        // float gy = float(y-ty)*tile->sizeMetersLat*1.01 - float(ym);
                        float gx = float(x-tx)*tile->sizeMetersLon - float(xm);
                        float gy = float(y-ty)*tile->sizeMetersLat - float(ym);
                        tile->mPosition.x = gx;
                        tile->mPosition.y = 0.0f;
                        tile->mPosition.z = gy;

                        tile->updateTracks();

                        if (tile->mUpdateTexture && !tile->mUpdateTextureRequested)
                        {
                            tile->mUpdateTextureRequested = true;
                            tile->mUpdateTexture = false; // turn off flag
                            mFetcher->addCommandLow(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::REFETCH_TEXTURE));
                        }
                        if (tile->mUpdateUserData && !tile->mUpdateUserDataRequested)
                        {
                            tile->mUpdateUserDataRequested = true;
                            tile->mUpdateUserData = false; // turn off flag
                            mFetcher->addCommandLow(mgnTerrainFetcher::CommandData(tile, mgnTerrainFetcher::REFETCH_USER_DATA));
                        }

                        renderer_->PushMatrix();
                        renderer_->Translate(tile->mPosition);

                        // const bool use_fog = mTerrainView->useFog();
                        // if (use_fog)
                        //     context.enableFog( mTerrainView->getCamHeight(), mTerrainView->getZFar() );
                        tile->drawTileMesh(frustum, mTerrainShader);
                        tile->drawSegments(frustum);
                        // if (use_fog)
                        //     context.disableFog();

                        renderer_->PopMatrix();

                        renderedTiles.push_back(tile);
                    }
                    tile->mDrawFrame = mFrameCount;
                }

            }
            bool need_resort_requests = ((message & (int)GeoHighlight::kAddRoute) != 0);
            if (need_resort_requests)
            {
                // TODO: remove this when a new fetcher class will be in use
                mFetcher->resort();
            }
        }

        void TerrainMap::renderLabels()
        {
            renderer_->DisableDepthTest();
    
            mBillboardShader->Bind();

            // Draw labels
            mUsedLabels.clear();
            mLabelBoundingBoxes.clear();
            for (size_t i=0; i<renderedTiles.size(); ++i)
            {
                TerrainTile * tile = renderedTiles[i];
                if (tile->mKey.magIndex != getTerrainView()->getMagIndex()) continue;

                renderer_->PushMatrix();
                renderer_->Translate(tile->position());
                tile->drawLabelsMesh(mUsedLabels, mLabelBoundingBoxes);
                renderer_->PopMatrix();
            }
            // Draw point user meshes
            for (std::list<Icon*>::iterator it = mIconList.begin(); it != mIconList.end(); ++it)
            {
                Icon* icon = *it;

                renderer_->PushMatrix();
                renderer_->Translate(icon->tile_position());
                icon->render();
                renderer_->PopMatrix();
            }
            renderer_->EnableDepthTest();
        }
        const Font * TerrainMap::chooseCompatibleFont() const
        {
            // At the beginning I planned to use outlined and non-outlined fonts and here determine which to use.
            // But now we always use outlined font.
            return mOutlinedFont;
        }
        bool TerrainMap::shouldRenderFakeTerrain() const
        {
            return (renderedTiles.empty()) || (!renderedTiles.front()->isFetched());
        }

        class IconDistanceCompareFunctor
        {
        public:
            IconDistanceCompareFunctor(const vec3& cam_position)
            : mCamPosition(cam_position)
            {
            }
            bool operator()(Icon* first, Icon* second) const
            {
                vec3 first_position = first->position() + first->tile_position();
                vec3 second_position = second->position() + second->tile_position();
                float d1_sqr = math::DistanceSqr(first_position, mCamPosition);
                float d2_sqr = math::DistanceSqr(second_position, mCamPosition);
                return d1_sqr > d2_sqr; // distant ones should be first
            }
        private:
            vec3 mCamPosition; //!< camera position in local cs
        };
        void TerrainMap::updateIconList()
        {
            mgnCriticalSectionScopedEntrance guard(mIconListCriticalSection);
            // Form list
            mIconList.clear();
            for (size_t i = 0; i < renderedTiles.size(); ++i)
            {
                TerrainTile * tile = renderedTiles[i];
                if (tile->mKey.magIndex != mTerrainView->getMagIndex()) continue;

                if (! tile->mIsFetchedUserObjects)
                    continue;

                for (size_t j = 0; j < tile->mPointUserMeshes.size(); ++j)
                {
                    mIconList.push_back( tile->mPointUserMeshes[j] );
                }
            }
            // Then sort it by distance
            // This is the weakest part, complexity is O(n*log(n))
            mIconList.sort(IconDistanceCompareFunctor(mTerrainView->getCamPosition()));
        }
        void TerrainMap::GetSelectedIcons(int x, int y, int radius, std::vector<int>& ids) const
        {
            mgnCriticalSectionScopedEntrance guard(const_cast<mgnCSHandle&>(mIconListCriticalSection));

            vec2 selection_pos((float)x, (float)y);
            float selection_radius = (float)radius;

            // Icons are sorted from farest to nearest, but we need to select nearest ones
            for (std::list<Icon*>::const_reverse_iterator it = mIconList.rbegin(); it != mIconList.rend(); ++it)
            {
                Icon * icon = *it;

                if (icon->getID() == 0) continue;

                // TODO: move following code to IconMesh class
                const math::Matrix4& proj = renderer_->projection_matrix();
                const math::Matrix4& view = renderer_->view_matrix();
                const math::Vector4& viewport = renderer_->viewport();

                vec4 icon_pos_world(icon->position() + icon->tile_position(), 1.0f);
                vec4 icon_pos_eye = view * icon_pos_world;

                vec2 icon_size;
                icon->GetIconSize(icon_size);

                vec4 vertices_eye[4];
                switch (icon->getOrigin())
                {
                case Billboard::kBottomLeft:
                    vertices_eye[0] = icon_pos_eye;
                    break;
                case Billboard::kBottomMiddle:
                    vertices_eye[0] = icon_pos_eye;
                    vertices_eye[0].x += -0.5f * icon_size.x;
                    break;
                default:
                    assert(false);
                    break;
                }
                vertices_eye[1] = vertices_eye[0];
                vertices_eye[1].x += icon_size.x;
                vertices_eye[2] = vertices_eye[0];
                vertices_eye[2].x += icon_size.x;
                vertices_eye[2].y -= icon_size.y; // sign "-" because of retarded view matrix (scale(1,-1,1))
                vertices_eye[3] = vertices_eye[0];
                vertices_eye[3].y -= icon_size.y;

                vec2 vertices_screen[4];
                for (int i = 0; i < 4; ++i)
                {
                    // Step 1: 4d Clip space
                    vec4 pos_clip = proj * vertices_eye[i];

                    // Step 2: 3d Normalized Device Coordinate space
                    vec3 pos_ndc = pos_clip.xyz() / pos_clip.w;

                    // Step 3: Window Coordinate space
                    vertices_screen[i].x = (pos_ndc.x + 1.0f) * 0.5f * viewport.z + viewport.x;
                    vertices_screen[i].y = (pos_ndc.y + 1.0f) * 0.5f * viewport.w + viewport.y;
                }

                if (math::CircleRectIntersection2D(selection_pos, selection_radius, vertices_screen))
                {
                    // Intersection detected
                    ids.push_back(icon->getID());
                }
            }
        }

        class TrackSelectionContext : public mgnMdIUserDataDrawContext {
        public:
            explicit TrackSelectionContext(const TerrainMap * terrain_map, graphics::Renderer * renderer,
                int x, int y, int radius)
            : mTerrainMap(terrain_map)
            {
                CalcWorldRect(renderer, (float)x, (float)y, (float)radius);
            }
            mgnMdWorldRect GetWorldRect() const { return mWorldRect; }
            int GetMapScaleIndex() const { return 1; }
            float GetMapScaleFactor() const { return 30.0f; }
            mgnMdWorldPosition GetGPSPosition() const { return mTerrainMap->GetGpsPosition(); }
            bool ShouldDrawPOIs() const { return false; }
            bool ShouldDrawLabels() const { return false; }

            void CalcWorldRect(graphics::Renderer * renderer, float x, float y, float radius)
            {
                vec2 screen_points[4] = {
                    vec2(x - radius, y - radius),
                    vec2(x + radius, y - radius),
                    vec2(x + radius, y + radius),
                    vec2(x - radius, y + radius)
                };
                mgnMdWorldPoint world_points[4];
                const math::Matrix4& proj = renderer->projection_matrix();
                const math::Matrix4& view = renderer->view_matrix();
                const math::Vector4& viewport = renderer->viewport();
                math::Matrix4 proj_inverse = proj.GetInverse();
                math::Matrix4 view_inverse = view.GetInverse();
                for (int i = 0; i < 4; ++i)
                {
                    const vec2& screen_point = screen_points[i];

                    // Convert screen point to world ray
                    vec3 ray;
                    math::ScreenToWorldRay(screen_point, viewport, proj_inverse, view_inverse, ray);

                    // Intersect ray with terrain
                    vec3 intersection;
                    mTerrainMap->IntersectionWithRay(ray, intersection);

                    // Transform point to world space
                    mTerrainMap->getTerrainView()->LocalToWorld(intersection.x, intersection.z, world_points[i]);
                }
                // Transform 4 points to bounding box
                mgnMdWorldPoint left_top, right_bottom;
                left_top = world_points[0];
                right_bottom = world_points[0];
                for (int i = 1; i < 4; ++i)
                {
                    if (left_top.mLatitude < world_points[i].mLatitude)
                        left_top.mLatitude = world_points[i].mLatitude;
                    if (left_top.mLongitude > world_points[i].mLongitude)
                        left_top.mLongitude = world_points[i].mLongitude;
                    if (right_bottom.mLatitude > world_points[i].mLatitude)
                        right_bottom.mLatitude = world_points[i].mLatitude;
                    if (right_bottom.mLongitude < world_points[i].mLongitude)
                        right_bottom.mLongitude = world_points[i].mLongitude;
                }
                mWorldRect.mUpperLeft = left_top;
                mWorldRect.mLowerRight = right_bottom;
            }

        private:
            const TerrainMap * mTerrainMap;
            mgnMdWorldRect mWorldRect;
        };

        class SelectedTracksObtainer : public mgnTerrainFetcher::ICallable {
        public:
            SelectedTracksObtainer(const mgnMdIUserDataDrawContext& context, mgnMdTerrainProvider& provider)
                : context(context)
                , provider(provider)
            {
            }

            void call() // overrides pure virtual function
            {
                provider.GetSelectedTracks(context, ids);
            }

            const mgnMdIUserDataDrawContext& context;
            mgnMdTerrainProvider& provider;
            std::vector<int> ids;
        };

        void TerrainMap::GetSelectedTracks(int x, int y, int radius, std::vector<int>& ids) const
        {
            TrackSelectionContext context(this, renderer_, x, y, radius);
            SelectedTracksObtainer obtainer(context, const_cast<mgnMdTerrainProvider&>(mTerrainProvider));
            mFetcher->invoke(&obtainer);
            ids.swap(obtainer.ids);
        }

        class SelectedTrailsObtainer : public mgnTerrainFetcher::ICallable {
        public:
            SelectedTrailsObtainer(const mgnMdIUserDataDrawContext& context, mgnMdTerrainProvider& provider,
                bool online)
                : context(context)
                , provider(provider)
                , online(online)
            {
            }

            void call() // overrides pure virtual function
            {
                provider.GetSelectedTrails(context, online, ids);
            }

            const mgnMdIUserDataDrawContext& context;
            mgnMdTerrainProvider& provider;
            std::vector<std::string> ids;
            bool online;
        };

        void TerrainMap::GetSelectedTrails(int x, int y, int radius, bool onLine, std::vector<std::string>& ids) const
        {
            TrackSelectionContext context(this, renderer_, x, y, radius);
            SelectedTrailsObtainer obtainer(context, const_cast<mgnMdTerrainProvider&>(mTerrainProvider), onLine);
            mFetcher->invoke(&obtainer);
            ids.swap(obtainer.ids);
        }

    } // namespace terrain
} // namespace mgn
