#pragma once
#ifndef __MGN_TERRAIN_PASSIVE_HIGHLIGHT_TRACK_RENDERER_H__
#define __MGN_TERRAIN_PASSIVE_HIGHLIGHT_TRACK_RENDERER_H__

#include "mgnTrSolidLineRenderer.h"

namespace mgn {
    class Timer;
    class TimeManager;
}

namespace mgn {
    namespace terrain {

        class mgnTerrainFetcher;

        //! Class for rendering highlight track chunk
        class PassiveHighlightTrackChunk : public SolidLineChunk
        {
        public:
            PassiveHighlightTrackChunk(SolidLineRenderer * owner, TerrainTile const * tile);
            virtual ~PassiveHighlightTrackChunk();

            void load(bool is_high_priority); //!< initially loads data
            void update();

            void RequestUpdate();

        private:
        };

        //! Class for rendering highlight track
        class PassiveHighlightTrackRenderer : public SolidLineRenderer
        {
            friend class PassiveHighlightTrackChunk;
        public:
            PassiveHighlightTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos, mgnTerrainFetcher * fetcher,
                mgn::TimeManager * time_manager);
            virtual ~PassiveHighlightTrackRenderer();

            virtual void update();

            mgnTerrainFetcher * getFetcher();

        protected:
            void doFetch();

        private:
            mgnTerrainFetcher * mFetcher;
            mgn::TimeManager * mTimeManager;
            mgnMdWorldPosition  mOldGpsPosition; //!< old gps position
            float mTrackWidth;
            mgn::Timer * mTimer;
        };

    } // namespace terrain
} // namespace mgn

#endif