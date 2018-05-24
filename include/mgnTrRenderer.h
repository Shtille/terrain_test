#pragma once
#ifndef __MGN_TERRAIN_RENDERER_H__
#define __MGN_TERRAIN_RENDERER_H__

#include "MapDrawing/Graphics/mgnVector.h"
#include "MapDrawing/Graphics/mgnMatrix.h"
#include "Frustum.h"

#include "mgnMdMapObjectsVector.h"

#include <string>
#include <vector>

class mgnMdWorldPoint;
class mgnMdTerrainView;
class mgnMdTerrainProvider;

namespace mgn {
    class TimeManager;
    namespace graphics {
        class Renderer;
        class Shader;
    }
}

namespace mgn {
    namespace terrain {

        // Forward declaration
        class VehicleRenderer;
        class GpsMmPositionRenderer;
        class RouteBeginRenderer;
        class RouteEndRenderer;
        class GuidanceArrowRenderer;
        class ManeuverRenderer;
        class DirectionalLineRenderer;
        class ActiveTrackRenderer;
        class HighlightTrackRenderer;
        class PassiveHighlightTrackRenderer;
        class TerrainMap;
        class MercatorTree;
        class MercatorProvider;
        class Font;

        // Main class for rendering
        class Renderer {
        public:
            Renderer(mgn::graphics::Renderer * renderer, int size_x, int size_y,
                const char* font_path, float pixel_scale,
                mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * terrain_provider,
                MercatorProvider * mercator_provider);
            ~Renderer();

            void Update();
            void Render();

            void OnResize(int size_x, int size_y);

            void UpdateLocationIndicator(double lat, double lon, double altitude,
                double heading, double tilt, int color1, int color2);

            void IntersectionWithRay(const math::Vector3& ray, math::Vector3& intersection) const;

            //! Returns icon IDs under cursor
            void GetSelectedIcons(int x, int y, int radius, std::vector<int>& ids) const;
            //! Returns track IDs under cursor
            void GetSelectedTracks(int x, int y, int radius, std::vector<int>& ids) const;
            //! Returns trails IDs under cursor
            void GetSelectedTrails(int x, int y, int radius, bool online, std::vector<std::string>& ids) const;
            //! Returns icon IDs under cursor
            void GetSelectedPOIs(int x, int y, int radius, TMapObjectsVector& objects) const;
            //! Updates POI selection
            void UpdatePOISelection(const std::vector<int>& selection);

        private:
            void UpdateProjectionMatrix();
            void UpdateViewMatrix();
            void LoadShaders();
            void UpdateShaders();
            void OnViewChange();

            mgn::graphics::Renderer * mRenderer;
            int mSizeX, mSizeY;
            float mFovX, mFovY;
            math::Matrix4 mProjectionViewMatrix;
            math::Frustum mFrustum;

            mgn::TimeManager * mTimeManager;

            mgnMdTerrainView * mTerrainView;
            mgnMdTerrainProvider * mTerrainProvider;
            MercatorProvider * mMercatorProvider;
            TerrainMap * mTerrainMap;
            DirectionalLineRenderer * mDirectionalLineRenderer;
            ActiveTrackRenderer * mActiveTrackRenderer;
            HighlightTrackRenderer * mHighlightTrackRenderer;
            PassiveHighlightTrackRenderer * mPassiveHighlightTrackRenderer;
            GuidanceArrowRenderer * mGuidanceArrowRenderer;
            ManeuverRenderer* mManeuverRenderer;
            RouteBeginRenderer * mRouteBeginRenderer;
            RouteEndRenderer * mRouteEndRenderer;
            GpsMmPositionRenderer * mGpsMmPositionRenderer;
            VehicleRenderer * mVehicleRenderer;

            Font * mFont;
            MercatorTree * mMercatorTree;

            graphics::Shader * mPositionShader;
            graphics::Shader * mPositionNormalShader;
            graphics::Shader * mPositionTexcoordShader;
            graphics::Shader * mPositionShadeTexcoordShader;
            graphics::Shader * mBillboardShader;
            graphics::Shader * mMercatorTileShader;

            bool mHasWorldRectBeenSet;
        };

    } // namespace terrain
} // namespace mgn

#endif