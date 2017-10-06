#include "mgnTrHighlightTrackRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnTrTerrainFetcher.h"
#include "mgnTrTerrainTile.h"

#include "mgnPolygonClipping.h"
#include "mgnTimeManager.h"

namespace {
    const float kTrackWidth = 10.0f;
}
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

        HighlightTrackChunk::HighlightTrackChunk(SolidLineRenderer * owner, TerrainTile const * tile)
            : SolidLineChunk(owner, tile)
            , mOldGpsPosition(0.0, 0.0)
        {
        }
        HighlightTrackChunk::~HighlightTrackChunk()
        {
        }
        void HighlightTrackChunk::load(bool is_high_priority)
        {
            HighlightTrackRenderer * ht_renderer = dynamic_cast<HighlightTrackRenderer *>(mOwner);
            mgnTerrainFetcher * fetcher = ht_renderer->getFetcher();
            mgnMdTerrainProvider * provider = ht_renderer->provider();

            GeoHighlight highlight;
            provider->fetchHighlight( *mTile, highlight );

            if (highlight.points_count == 0) // highlight track may not exist
            {
                mIsDataReady = true;
                return;
            }

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

            mOldGpsPosition = mTile->GetGPSPosition();

            if (is_high_priority)
                fetcher->addCommand(mgnTerrainFetcher::CommandData(const_cast<TerrainTile*>(mTile), 
                    mgnTerrainFetcher::UPDATE_TRACKS));
            else
                fetcher->addCommandLow(mgnTerrainFetcher::CommandData(const_cast<TerrainTile*>(mTile), 
                    mgnTerrainFetcher::UPDATE_TRACKS));
        }
        void HighlightTrackChunk::update()
        {
            HighlightTrackRenderer * ht_renderer = dynamic_cast<HighlightTrackRenderer *>(mOwner);
            mgnTerrainFetcher * fetcher = ht_renderer->getFetcher();
            mgnMdTerrainProvider * provider = ht_renderer->provider();

            bool has_track_been_removed = false;

            // Check messages first
            const bool highlight_exists = provider->isHighlightExist();
            if ((mTile->mHighlightMessage & (int)GeoHighlight::kRemoveRoute) ||
                (!mParts.empty() && mIsFetched && !highlight_exists))
            {
                // Remove any fetching data
                bool data_dequeued = false;
                while (!data_dequeued)
                {
                    data_dequeued = fetcher->removeCommand(mgnTerrainFetcher::CommandData(const_cast<TerrainTile*>(mTile), 
                        mgnTerrainFetcher::UPDATE_TRACKS));
                }
                mFetchMap.clear();
                // We may clear these segments
                clear();
                mIsFetched = false;
                mIsDataReady = false;
                has_track_been_removed = true;
            }
            if ((mTile->mHighlightMessage & (int)GeoHighlight::kAddRoute) ||
                (mParts.empty() && !mIsFetched && highlight_exists))
            {
                // If track has been removed it should have low priority
                const bool first_time_load = !has_track_been_removed;
                load(first_time_load);
            }
            mTile->mHighlightMessage = (int)GeoHighlight::kNoMessages; // = 0
            if (mIsFetched) // data is ready
            {
                generateData();
                mIsDataReady = true;
            }

            mgnMdWorldPosition p = mTile->GetGPSPosition();
            const bool pos_has_changed = p.mLatitude != mOldGpsPosition.mLatitude
                || p.mLongitude != mOldGpsPosition.mLongitude;
            mOldGpsPosition = p;

            // Highlight trimming during simulation
            const bool need_to_fetch = !mParts.empty() && pos_has_changed && mIsFetched &&
                provider->fetchNeedToTrimHighlight();
            if (need_to_fetch)
            {
                const float kVehicleDistancePrecision = 1.0f;
                vec2 vpos; // vehicle pos in tile cs
                mTile->worldToTile(p.mLatitude, p.mLongitude, vpos.x, vpos.y);
                bool found_segment = false;
                for (std::vector<Part>::iterator itp = mParts.begin(); itp != mParts.end(); ++itp)
                {
                    Part& part = *itp;
                    for (std::list<SolidLineSegment*>::iterator it = part.segments.begin(); it != part.segments.end(); ++it)
                    {
                        SolidLineSegment * segment = *it;
                        vec2 line = segment->end() - segment->begin();
                        float len = line.Length();
                        if (len < clipping::eps) // zero length segment
                        {
                            continue;
                        }
                        float invlen = 1.0f / len;
                        vec2 side(-line.y*invlen, line.x*invlen);
                        vec2 to_vpos = vpos - segment->begin();
                        if (fabs(to_vpos & side) < kVehicleDistancePrecision) // some distance precision
                        {                    
                            float t = (to_vpos & line) * invlen * invlen;
                            if (t >= -0.1f && t < 1.0f) // found
                            {
                                // We have found our segment, so gonna trim all data preceding it
                                found_segment = true;
                                // Make projected position to current segment
                                vec2 projected_pos = segment->begin() + t * line;
                                // Trim current part
                                vec2 segment_end = segment->end();
                                std::list<SolidLineSegment*>::iterator it_found = it;
                                std::list<SolidLineSegment*>::iterator it_next = it_found; ++it_next;
                                for (std::list<SolidLineSegment*>::iterator it = part.segments.begin(); it != it_next; ++it)
                                    delete *it;
                                part.segments.erase(part.segments.begin(), it_next);
                                part.segments.push_front(new SolidLineSegment(projected_pos, segment_end, mTile, kTrackWidth, true));
                                // Trim highlight stubs
                                for (std::vector<Part>::iterator itp = mParts.begin(); itp != mParts.end(); ++itp)
                                {
                                    Part& selected_part = *itp;
                                    if ((selected_part.segments.size() == 1) && // part is just one segment
                                        (&selected_part != &part)) // current part differs from selected
                                    {
                                        SolidLineSegment * segment = selected_part.segments.front();
                                        if (segment->beenModified())
                                        {
                                            delete segment;
                                            selected_part.segments.clear();
                                        }
                                    }
                                }

                                createSegments();
                                fetchTriangles();
                                generateData(); // push render data to load immediately
                                break;
                            }
                        }
                    }
                    if (found_segment)
                        break;
                }
                if (!found_segment)
                {
                    GeoHighlight highlight;
                    provider->fetchHighlight( *mTile, highlight );
                    if (highlight.points_count == 0)
                    {
                        // We may clear these segments
                        clear();
                        mIsFetched = false;
                        mIsDataReady = false;
                    }
                }
            }
        }
        //=======================================================================
        HighlightTrackRenderer::HighlightTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos, mgnTerrainFetcher * fetcher)
            : SolidLineRenderer(renderer, terrain_view, provider, shader, gps_pos)
            , mFetcher(fetcher)
            , mOldGpsPosition(0.0, 0.0)
        {
            const mgnU32_t kTimerExpireInterval = 1500;
            mTimer = mgn::TimeManager::GetInstance()->AddTimer(kTimerExpireInterval);
        }
        HighlightTrackRenderer::~HighlightTrackRenderer()
        {
            mgn::TimeManager::GetInstance()->RemoveTimer(mTimer);
        }
        void HighlightTrackRenderer::update()
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
                recreateAll(); // update highlight widths
            }
        }
        void HighlightTrackRenderer::doFetch()
        {
        }
        mgnTerrainFetcher * HighlightTrackRenderer::getFetcher()
        {
            return mFetcher;
        }

    } // namespace terrain
} // namespace mgn