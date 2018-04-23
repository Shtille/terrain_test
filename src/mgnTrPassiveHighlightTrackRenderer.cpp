#include "mgnTrPassiveHighlightTrackRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnTrTerrainFetcher.h"
#include "mgnTrTerrainTile.h"

#include "mgnPolygonClipping.h"
#include "mgnTimeManager.h"

static inline vec3 MakeColorFrom565(mgnU16_t color565)
{
    return vec3(
        ((color565 >> 11)       ) / 31.0f,
        ((color565 >>  5) & 0x3f) / 63.0f,
        ((color565      ) & 0x1f) / 31.0f
        );
}

namespace mgn {
    namespace terrain {

        PassiveHighlightTrackChunk::PassiveHighlightTrackChunk(SolidLineRenderer * owner, TerrainTile const * tile)
            : SolidLineChunk(owner, tile)
        {
        }
        PassiveHighlightTrackChunk::~PassiveHighlightTrackChunk()
        {
        }
        void PassiveHighlightTrackChunk::load(bool is_high_priority)
        {
            PassiveHighlightTrackRenderer * ht_renderer = dynamic_cast<PassiveHighlightTrackRenderer *>(mOwner);
            mgnTerrainFetcher * fetcher = ht_renderer->getFetcher();
            mgnMdTerrainProvider * provider = ht_renderer->provider();

            GeoHighlight highlight;
            provider->fetchPassiveHighlight( *mTile, highlight );

            if (highlight.points_count == 0) // highlight track may not exist
            {
                mIsDataReady = true;
                return;
            }

            // Update track width on any load
            const float kTrackWidth = ht_renderer->mTrackWidth;

            mParts.resize(highlight.parts.size());
            for (unsigned int j = 0; j < highlight.parts.size(); ++j)
            {
                GeoHighlight::Part& highlight_part = highlight.parts[j];
                SolidLineChunk::Part& track_part = mParts[j];
                track_part.color = MakeColorFrom565(highlight_part.color);
                for (unsigned int i = 0; i < highlight_part.points.size()-1; ++i)
                {
                    const mgnMdWorldPoint& cur  = highlight_part.points[i];
                    const mgnMdWorldPoint& next = highlight_part.points[i+1];
                    track_part.segments.push_back(new SolidLineSegment(cur, next, mTile, kTrackWidth));
                }
            }

            createSegments();

            if (is_high_priority)
                fetcher->addCommand(mgnTerrainFetcher::CommandData(const_cast<TerrainTile*>(mTile), 
                    mgnTerrainFetcher::FETCH_PASSIVE_HIGHLIGHT));
            else
                fetcher->addCommandLow(mgnTerrainFetcher::CommandData(const_cast<TerrainTile*>(mTile), 
                    mgnTerrainFetcher::FETCH_PASSIVE_HIGHLIGHT));
        }
        void PassiveHighlightTrackChunk::update()
        {
            PassiveHighlightTrackRenderer * ht_renderer = dynamic_cast<PassiveHighlightTrackRenderer *>(mOwner);
            mgnTerrainFetcher * fetcher = ht_renderer->getFetcher();
            mgnMdTerrainProvider * provider = ht_renderer->provider();

            bool need_update = false;
            if (mTile->mUpdatePassiveHighlight && mTile->isFetchedTerrain())
            {
                const_cast<TerrainTile*>(mTile)->mUpdatePassiveHighlight = false;
                need_update = true;
            }

            if (need_update)
            {
                // Remove any fetching data
                bool data_dequeued = false;
                while (!data_dequeued)
                {
                    data_dequeued = fetcher->removeCommand(mgnTerrainFetcher::CommandData(const_cast<TerrainTile*>(mTile), 
                        mgnTerrainFetcher::FETCH_PASSIVE_HIGHLIGHT));
                }
                mFetchMap.clear();
                // We may clear these segments
                clear();
                mIsFetched = false;
                mIsDataReady = false;

                load(true);
            }
            if (mIsFetched) // data is ready
            {
                generateData();
                mIsDataReady = true;
            }
        }
        void PassiveHighlightTrackChunk::RequestUpdate()
        {
            const_cast<TerrainTile*>(mTile)->mUpdatePassiveHighlight = true;
        }
        //=======================================================================
        PassiveHighlightTrackRenderer::PassiveHighlightTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos, mgnTerrainFetcher * fetcher,
                mgn::TimeManager * time_manager)
            : SolidLineRenderer(renderer, terrain_view, provider, shader, gps_pos)
            , mFetcher(fetcher)
            , mTimeManager(time_manager)
            , mOldGpsPosition(0.0, 0.0)
            , mTrackWidth(10.0f)
        {
            const mgnU32_t kTimerExpireInterval = 1500;
            mTimer = mTimeManager->AddTimer(kTimerExpireInterval);
        }
        PassiveHighlightTrackRenderer::~PassiveHighlightTrackRenderer()
        {
            mTimeManager->RemoveTimer(mTimer);
        }
        void PassiveHighlightTrackRenderer::update()
        {
            unsigned short mag_index = terrain_view_->getMagIndex();
            if (mag_index != mStoredMagIndex)
            {
                mTimer->Reset(); // reset timer time to 0
                mTimer->Start();
                mStoredMagIndex = mag_index;
            }
            if (mTimer->HasExpired()) // we haven't changed zoom during this interval
            {
                mTimer->Reset();
                mTimer->Stop();

                for (ChunkList::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
                {
                    PassiveHighlightTrackChunk * chunk = dynamic_cast<PassiveHighlightTrackChunk *>(*it);
                    chunk->RequestUpdate();
                }
            }
        }
        void PassiveHighlightTrackRenderer::doFetch()
        {
        }
        mgnTerrainFetcher * PassiveHighlightTrackRenderer::getFetcher()
        {
            return mFetcher;
        }

    } // namespace terrain
} // namespace mgn