#ifndef __MGN_TERRAIN_ROUTE_END_RENDERER_H__
#define __MGN_TERRAIN_ROUTE_END_RENDERER_H__

#include "mgnTrRoutePointRenderer.h"

namespace mgn {
    namespace terrain {

        //! Base class for rendering route point
        class RouteEndRenderer : public RoutePointRenderer
        {
        public:
            explicit RouteEndRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader);
            virtual ~RouteEndRenderer();

            void Update(MercatorProvider * provider);
        };

    } // namespace terrain
} // namespace mgn

#endif