#include "mgnTrRouteEndRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnMdWorldPoint.h"

namespace mgn {
    namespace terrain {

        RouteEndRenderer::RouteEndRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
            graphics::Shader * shader)
        : RoutePointRenderer(renderer, terrain_view, shader)
        {
            mColor.Set(1.0f, 0.0f, 0.0f);
        }
        RouteEndRenderer::~RouteEndRenderer()
        {

        }
        void RouteEndRenderer::Update(mgnMdTerrainProvider * provider)
        {
            // TODO: do 1 fetch per second, not every frame
            static std::vector<mgnMdWorldPoint> route_points;
            route_points.clear();
            if (provider->fetchRouteEnd(route_points))
            {
                mExists = true;
                float cam_dist = (float)mTerrainView->getCamDistance();
                mScale = std::max(cam_dist * 0.01f, 1.0f);
                mPositions.clear();
                for (std::vector<mgnMdWorldPoint>::const_iterator it = route_points.begin();
                    it != route_points.end(); ++it)
                {
                    const mgnMdWorldPoint& route_point = *it;
                    double local_x, local_y;
                    mTerrainView->WorldToLocal(route_point, local_x, local_y);
                    vec3 position;
                    position.x = (float)local_x;
                    position.z = (float)local_y;
                    position.y = (float)provider->getAltitude(route_point.mLatitude, route_point.mLongitude);
                    mPositions.push_back(position);
                }
            }
            else
            {
                mExists = false;
            }
        }

    } // namespace terrain
} // namespace mgn