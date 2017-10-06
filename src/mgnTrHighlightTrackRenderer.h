#pragma once
#ifndef __MGN_TERRAIN_HIGHLIGHT_TRACK_RENDERER_H__
#define __MGN_TERRAIN_HIGHLIGHT_TRACK_RENDERER_H__

#include "mgnTrSolidLineRenderer.h"

namespace mgn {
    class Timer;
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

        private:
            mgnMdWorldPosition mOldGpsPosition;
        };

        //! Class for rendering highlight track
        class HighlightTrackRenderer : public SolidLineRenderer
        {
        public:
            HighlightTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos, mgnTerrainFetcher * fetcher);
            virtual ~HighlightTrackRenderer();

            virtual void update();

            mgnTerrainFetcher * getFetcher();

        protected:
            void doFetch();

        private:
            HighlightTrackRenderer(const HighlightTrackRenderer&);
            void operator =(const HighlightTrackRenderer&);

            mgnTerrainFetcher * mFetcher;
            mgnMdWorldPosition  mOldGpsPosition; //!< old gps position
            mgn::Timer * mTimer;
        };

    } // namespace terrain
} // namespace mgn

#endif