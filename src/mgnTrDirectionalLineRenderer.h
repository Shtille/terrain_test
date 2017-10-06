#pragma once
#ifndef __MGN_TERRAIN_DIRECTIONAL_LINE_RENDERER_H__
#define __MGN_TERRAIN_DIRECTIONAL_LINE_RENDERER_H__

#include "mgnTrDottedLineRenderer.h"

namespace mgn {
    namespace terrain {

        //! Class for rendering directional line
        class DirectionalLineRenderer : public DottedLineRenderer
        {
        public:

            DirectionalLineRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            virtual ~DirectionalLineRenderer();

            void onViewChange();
            void update(const math::Frustum& frustum);
            void render(const math::Frustum& frustum);

        protected:

            // Some class tuning options
            const bool isUsingLongSegmentFix() { return false; }
            const bool isUsingSeparateSegments() { return true; }
            const bool isUsingPointsCache() { return false; }
            const bool isUsingFastNormalComputation() { return true; }
            const int maxRenderedPointsPerSegment() { return 100; }

            void createTexture();
            void doFetch();

            std::list<DottedLineSegment*> mCroppedSegments;

        private:

            // For flat profile cases
            void frustumCropSegment(const math::Frustum& frustum, DottedLineSegment* const & in, DottedLineSegment*& out);
            // For complicated cases
            void frustumCropSegment2(const math::Frustum& frustum, DottedLineSegment* const & in, DottedLineSegment*& out);

            // Some assumptions:
            const float assumedMinAltitude() { return 0.0f; }
            const float assumedMaxAltitude() { return 10000.0f; }
        };

    } // namespace terrain
} // namespace mgn

#endif
