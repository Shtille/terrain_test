#ifndef __MGN_TERRAIN_ROUTE_BEGIN_RENDERER_H__
#define __MGN_TERRAIN_ROUTE_BEGIN_RENDERER_H__

#include "mgnTrRoutePointRenderer.h"

namespace mgn {
    namespace terrain {

        //! Base class for rendering route point
        class RouteBeginRenderer : public RoutePointRenderer
        {
        public:
            explicit RouteBeginRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader);
            virtual ~RouteBeginRenderer();

            void Update(mgnMdTerrainProvider * provider);
        };

    } // namespace terrain
} // namespace mgn

#endif