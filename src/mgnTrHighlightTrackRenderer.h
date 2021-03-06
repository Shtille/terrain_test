#pragma once
#ifndef __MGN_TERRAIN_HIGHLIGHT_TRACK_RENDERER_H__
#define __MGN_TERRAIN_HIGHLIGHT_TRACK_RENDERER_H__

#include "mgnTrSolidLineRenderer.h"

namespace mgn {
    class Timer;
    class TimeManager;
}

namespace mgn {
    namespace terrain {

        class mgnTerrainFetcher;

        //! Class for rendering highlight track chunk
        class HighlightTrackChunk : public SolidLineChunk
        {
        public:
            HighlightTrackChunk(SolidLineRenderer * owner, TerrainTile const * tile);
            virtual ~HighlightTrackChunk();

            void load(bool is_high_priority); //!< initially loads data
            void update();

            void checkPointsCount();

        private:
            mgnMdWorldPosition mOldGpsPosition;
            int mLastPointsCount;
            bool mLoadRequested;
        };

        //! Class for rendering highlight track
        class HighlightTrackRenderer : public SolidLineRenderer
        {
            friend class HighlightTrackChunk;
        public:
            HighlightTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos, mgnTerrainFetcher * fetcher,
                mgn::TimeManager * time_manager);
            virtual ~HighlightTrackRenderer();

            virtual void update();

            mgnTerrainFetcher * getFetcher();

        protected:
            void doFetch();
            void checkPointsCount();

        private:
            mgnTerrainFetcher * mFetcher;
            mgn::TimeManager * mTimeManager;
            mgnMdWorldPosition  mOldGpsPosition; //!< old gps position
            float mTrackWidth;
            mgn::Timer * mTimer;
            mgn::Timer * mPointCheckTimer;
        };

    } // namespace terrain
} // namespace mgn

#endif