#include "mgnTrRouteEndRenderer.h"

#include "mgnTrConstants.h"
#include "mgnTrMercatorProvider.h"

#include "mgnMdTerrainView.h"

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
        void RouteEndRenderer::Update(MercatorProvider * provider)
        {
            // TODO: do 1 fetch per second, not every frame
            static std::vector<mgnMdWorldPoint> route_points;
            route_points.clear();
            if (provider->FetchRouteEnd(route_points))
            {
                const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());

                mExists = true;
                float cam_dist = (float)mTerrainView->getCamDistance();
                mScale = std::max(cam_dist * 0.01f, 1.0f);
                mTerrainView->LocalToPixelDistance(mScale, mScale, kMSM);

                mPositions.clear();
                for (std::vector<mgnMdWorldPoint>::const_iterator it = route_points.begin();
                    it != route_points.end(); ++it)
                {
                    const mgnMdWorldPoint& route_point = *it;

                    double latitude = route_point.mLatitude;
                    double longitude = route_point.mLongitude;
                    double altitude = provider->GetAltitude(latitude, longitude);
                    vec3 position;
                    mTerrainView->WorldToPixel(latitude, longitude, altitude, position, kMSM);

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