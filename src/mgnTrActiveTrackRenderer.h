#pragma once
#ifndef __MGN_TERRAIN_ACTIVE_TRACK_RENDERER_H__
#define __MGN_TERRAIN_ACTIVE_TRACK_RENDERER_H__

#include "mgnTrDottedLineRenderer.h"

namespace mgn {
    namespace terrain {

        class ActiveTrackSegment : public DottedLineSegment
        {
        public:
            ActiveTrackSegment(const mgnMdWorldPoint& begin, const mgnMdWorldPoint& end, bool has_break, bool to_location)
                : DottedLineSegment(begin, end)
                , mHasBreak(has_break)
                , mToLocation(to_location)
            {
            }
            virtual ~ActiveTrackSegment() {}

            bool mHasBreak;
            bool mToLocation; //!< segment is temporary, shows the way to current location
        };

        //! Class for rendering active track
        class ActiveTrackRenderer : public DottedLineRenderer
        {
        public:

            ActiveTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            virtual ~ActiveTrackRenderer();

            void render(const math::Frustum& frustum);

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
            void addSegment(const mgnMdWorldPoint& begin, const mgnMdWorldPoint& end, bool has_break, bool to_location);

        private:

            int mNumPoints; //!< old points count of UGDS track
            bool mVisible;
            bool mIsLoading;
        };

    } // namespace terrain
} // namespace mgn

#endif
