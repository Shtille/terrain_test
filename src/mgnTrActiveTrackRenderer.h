#pragma once
#ifndef __MGN_TERRAIN_ACTIVE_TRACK_RENDERER_H__
#define __MGN_TERRAIN_ACTIVE_TRACK_RENDERER_H__

#include "mgnTrDottedLineRenderer.h"

namespace mgn {
    namespace terrain {

        class ActiveTrackSegment : public DottedLineSegment
        {
        public:
            ActiveTrackSegment(const mgnMdWorldPoint& begin, const mgnMdWorldPoint& end, bool has_break = false)
                : DottedLineSegment(begin, end)
                , mHasBreak(has_break)
            {
            }
            virtual ~ActiveTrackSegment() {}

            bool mHasBreak;
        };

        //! Class for rendering active track
        class ActiveTrackRenderer : public DottedLineRenderer
        {
        public:

            ActiveTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            virtual ~ActiveTrackRenderer();

        protected:

            // Some class tuning options
            const bool isUsingLongSegmentFix() { return true; }
            const bool isUsingSeparateSegments() { return false; }
            const bool isUsingPointsCache() { return false; }
            const float elementSize() { return 0.5f; }
            const float elementStep() { return 2.0f; }

            void clearSegments();

            void createTexture();
            void doFetch();

        private:

            int mNumPoints; //!< old points count of UGDS track
        };

    } // namespace terrain
} // namespace mgn

#endif
