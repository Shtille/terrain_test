#include "mgnTrRenderer.h"

#define MGNTR_MERCATOR_TILE

#ifndef MGNTR_MERCATOR_TILE
#include "mgnTrVehicleRenderer.h"
#include "mgnTrGpsMmPositionRenderer.h"
#include "mgnTrRouteBeginRenderer.h"
#include "mgnTrRouteEndRenderer.h"
#include "mgnTrGuidanceArrowRenderer.h"
#include "mgnTrManeuverRenderer.h"
#include "mgnTrDirectionalLineRenderer.h"
#include "mgnTrActiveTrackRenderer.h"
#include "mgnTrHighlightTrackRenderer.h"
#include "mgnTrPassiveHighlightTrackRenderer.h"
#include "mgnTrTerrainMap.h"
#endif
#include "mgnTrFontAtlas.h"

#include "mgnMdTerrainView.h"
#include "mgnTimeManager.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"
#include "MapDrawing/Graphics/Renderer.h"

#include "mgnOmManager.h"

#include "TrailDbTileViewport.h"

// Shader sources
#include "shaders/mgnTrPositionShaderSource.h"
#include "shaders/mgnTrPositionNormalShaderSource.h"
#include "shaders/mgnTrPositionTexcoordShaderSource.h"
#include "shaders/mgnTrPositionShadeTexcoordShaderSource.h"
#include "shaders/mgnTrBillboardShaderSource.h"
#include "shaders/mgnTrMercatorTileShaderSource.h"



#ifdef MGNTR_MERCATOR_TILE
#include "mgnTrConstants.h"
#include "mercator/mgnTrMercatorTree.h"
#endif

//#define COUNT_PERFORMANCE

#ifdef COUNT_PERFORMANCE
#include "mgnLog.h"
static mgn::Timer * s_timer = NULL;
#endif

namespace mgn {
    namespace terrain {

    Renderer::Renderer(mgn::graphics::Renderer * renderer, int size_x, int size_y,
        const char* font_path, float pixel_scale,
        mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * terrain_provider,
        MercatorProvider * mercator_provider)
    : mRenderer(renderer)
    , mSizeX(size_x)
    , mSizeY(size_y)
    , mTerrainView(terrain_view)
    , mTerrainProvider(terrain_provider)
    , mMercatorProvider(mercator_provider)
    , mPositionShader(NULL)
    , mPositionNormalShader(NULL)
    , mPositionTexcoordShader(NULL)
    , mPositionShadeTexcoordShader(NULL)
    , mBillboardShader(NULL)
    , mMercatorTileShader(NULL)
    , mHasWorldRectBeenSet(false)
    {
        // At first we must initialize shaders to use it further
        LoadShaders();

        // Set projection matrix from terrain view's fovx value
        UpdateProjectionMatrix();

        mTimeManager = new mgn::TimeManager();

        // Sizes taken from RenderToolManagerGL.cpp
        const int size3 = static_cast<int>(18.0f /** pixel_scale*/ + 0.5f);
        const int border = static_cast<int>(3.0f /*+ pixel_scale*/ + 0.5f);
        mFont = Font::CreateWithBorder(renderer, font_path, size3, border,
            Color(0.0f, 0.0f, 0.0f), Color(1.0f, 1.0f, 1.0f), pixel_scale);

#ifndef MGNTR_MERCATOR_TILE
        mVehicleRenderer = new VehicleRenderer(renderer, terrain_view,
            mPositionShadeTexcoordShader);
        mTerrainMap = new TerrainMap(renderer, terrain_view, terrain_provider,
            mPositionTexcoordShader, mBillboardShader,
            mVehicleRenderer->GetPosPtr(), font_path, pixel_scale);
        mDirectionalLineRenderer = new DirectionalLineRenderer(renderer, terrain_view, terrain_provider,
            mPositionTexcoordShader, mVehicleRenderer->GetPosPtr());
        mActiveTrackRenderer = new ActiveTrackRenderer(renderer, terrain_view, terrain_provider,
            mPositionTexcoordShader, mVehicleRenderer->GetPosPtr());
        mHighlightTrackRenderer = new HighlightTrackRenderer(renderer, terrain_view, terrain_provider,
            mPositionShader, mVehicleRenderer->GetPosPtr(), mTerrainMap->getFetcherPtr(), mTimeManager);
        mPassiveHighlightTrackRenderer = new PassiveHighlightTrackRenderer(renderer, terrain_view, terrain_provider,
            mPositionShader, mVehicleRenderer->GetPosPtr(), mTerrainMap->getFetcherPtr(), mTimeManager);
        mTerrainMap->setHighlightRenderer( mHighlightTrackRenderer );
        mGuidanceArrowRenderer = new GuidanceArrowRenderer(renderer, terrain_view,
            mPositionShader, mVehicleRenderer->GetPosPtr());
        mManeuverRenderer = new ManeuverRenderer(renderer, terrain_view,
            mPositionShader, mVehicleRenderer->GetPosPtr());
        mRouteBeginRenderer = new RouteBeginRenderer(renderer, terrain_view,
            mPositionNormalShader);
        mRouteEndRenderer = new RouteEndRenderer(renderer, terrain_view,
            mPositionNormalShader);
        mGpsMmPositionRenderer = new GpsMmPositionRenderer(renderer, terrain_view,
            mPositionNormalShader);

        mTerrainMap->setHighlightRenderer( mHighlightTrackRenderer );
        mTerrainMap->setPassiveHighlightRenderer(mPassiveHighlightTrackRenderer);
#else
        mMercatorTree = new MercatorTree(renderer, mMercatorTileShader, mBillboardShader, mFont,
            &mFrustum, terrain_view, mercator_provider);
        mMercatorTree->Initialize(mFovY, size_y);
#endif

#ifdef COUNT_PERFORMANCE
        s_timer = mTimeManager->AddTimer(1000);
        s_timer->Start();
#endif

        // Update viewport manually on creation since owner doesn't do it
        mRenderer->UpdateSizes(size_x, size_y);
        mRenderer->SetViewport(size_x, size_y);
    }
    Renderer::~Renderer()
    {
#ifdef COUNT_PERFORMANCE
        mTimeManager->RemoveTimer(s_timer);
#endif
#ifdef MGNTR_MERCATOR_TILE
        delete mMercatorTree;
#else   
        delete mGpsMmPositionRenderer;
        delete mRouteEndRenderer;
        delete mRouteBeginRenderer;
        delete mManeuverRenderer;
        delete mGuidanceArrowRenderer;
        delete mActiveTrackRenderer;
        delete mDirectionalLineRenderer;
        delete mTerrainMap;
        delete mPassiveHighlightTrackRenderer;
        delete mHighlightTrackRenderer; // should be deleted after TerrainMap
        delete mVehicleRenderer;
#endif
        delete mFont;

        delete mTimeManager;
    }
    void Renderer::Update()
    {
        UpdateProjectionMatrix(); // znear and zfar may change
        UpdateViewMatrix();

        // Update frustum
        mProjectionViewMatrix = mRenderer->projection_matrix() * mRenderer->view_matrix();
        mFrustum.Load(mProjectionViewMatrix);

        mTimeManager->Update();

#ifdef COUNT_PERFORMANCE
        if (s_timer->HasExpired())
        {
            s_timer->Reset();
            LOG_INFO(0, ("FPS: %.2f", mTimeManager->GetFrameRate()));
        }
#endif

#ifndef MGNTR_MERCATOR_TILE
        mVehicleRenderer->Update();
        mDirectionalLineRenderer->update(mFrustum);
        mActiveTrackRenderer->update(mFrustum);
        mHighlightTrackRenderer->update();
        mPassiveHighlightTrackRenderer->update();
        mRouteBeginRenderer->Update(mTerrainProvider);
        mRouteEndRenderer->Update(mTerrainProvider);
        mGpsMmPositionRenderer->Update(mTerrainProvider);
        mManeuverRenderer->Update(mTerrainProvider);
        mTerrainMap->Update();

        {
            mGuidanceArrowRenderer->Update(mTerrainProvider);
            float fovx = mTerrainView->getFovX();
            const math::Matrix4& view = mRenderer->view_matrix();
            float distance_to_camera = math::DistanceToCamera(mGuidanceArrowRenderer->position(), view);
            mGuidanceArrowRenderer->UpdateLineWidth(fovx, distance_to_camera);
        }
#else
        mMercatorTree->Update();
#endif

        OnViewChange();

        UpdateShaders();
    }
    void Renderer::Render()
    {
        mRenderer->ClearColor(0.5f, 0.6f, 0.8f, 1.0f);
        mRenderer->ClearColorAndDepthBuffers();

#ifndef MGNTR_MERCATOR_TILE
        mTerrainMap->render(mFrustum);

        mDirectionalLineRenderer->render(mFrustum);
        mActiveTrackRenderer->render(mFrustum);

        mTerrainMap->renderLabels();

        mRouteBeginRenderer->Render(mFrustum,
            mVehicleRenderer->GetCenter(), mVehicleRenderer->GetSize());
        mRouteEndRenderer->Render(mFrustum,
            mVehicleRenderer->GetCenter(), mVehicleRenderer->GetSize());
        mGpsMmPositionRenderer->Render();

        // Maneuver arrow rendering
        {
            boost::optional<std::pair<vec2, float> > shadow = boost::none;
            if (mVehicleRenderer)
            {
                shadow = std::make_pair(mVehicleRenderer->GetCenter().xz(), mVehicleRenderer->GetSize());
            }
            mManeuverRenderer->Render(shadow);
        }

        // Guidance arrow rendering
        mGuidanceArrowRenderer->Render(mManeuverRenderer->shadow());

        mVehicleRenderer->Render();
#else
        mRenderer->EnableBlend();
        mMercatorTree->Render();
        mMercatorTree->RenderLabels();
#endif
    }
    void Renderer::OnResize(int size_x, int size_y)
    {
        mSizeX = size_x;
        mSizeY = size_y;

        // Renderer receives resize message from EML directly, don't need to update viewport.
        UpdateProjectionMatrix();
    }
    void Renderer::OnViewChange()
    {
#ifndef MGNTR_MERCATOR_TILE
        const bool is_camera_changed = mTerrainView->checkViewChange();
        if (!is_camera_changed && mHasWorldRectBeenSet) return;

        // All user functions should be written down here:
        mDirectionalLineRenderer->onViewChange();
        mActiveTrackRenderer->onViewChange();
        mHighlightTrackRenderer->onViewChange();
        mPassiveHighlightTrackRenderer->onViewChange();
        mTerrainMap->updateIconList();
        // Inform online raster maps manager about new world rect
        mgnMdWorldRect world_rect = mTerrainView->getLowDetailWorldRect();
        // We need MapDraw to be initialized and then DataProvider not null
        mHasWorldRectBeenSet = mgn::online_map::Manager::GetInstance()->SetWorldRect(
            world_rect.left(), world_rect.top(), world_rect.right(), world_rect.bottom());
        TrailDb::TileViewport::GetInstance()->SetWorldRect(
            world_rect.left(), world_rect.top(), world_rect.right(), world_rect.bottom());
#endif
    }
    void Renderer::UpdateProjectionMatrix()
    {
        //mRenderer->SetProjectionMatrix(math::PerspectiveMatrix(45.0f, mSizeX, mSizeY, 0.1f, 100.0f));
        float zfar = mTerrainView->getZFar();
        float znear = mTerrainView->getZNear();
#ifdef MGNTR_MERCATOR_TILE
        const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
        mTerrainView->LocalToPixelDistance(zfar, zfar, kMSM);
        mTerrainView->LocalToPixelDistance(znear, znear, kMSM);
#endif
        mFovX = mTerrainView->getFovX();
        mFovY = ((float)mSizeY / (float)mSizeX) * mFovX;
        if (mFovX < mFovY) // book orientation field of view fix
        {
            float old_fovy = mFovY;
            mFovY = mFovX;
            mFovX *= mFovX / old_fovy;
        }
        mRenderer->SetProjectionMatrix(math::RetardedPerspectiveMatrix(mFovX, mSizeX, mSizeY, znear, zfar));
    }
    void Renderer::UpdateViewMatrix()
    {
        float camera_distance = (float)mTerrainView->getCamDistance();
        float camera_tilt = (float)mTerrainView->getCamTiltRad();
        float camera_heading = (float)mTerrainView->getCamHeadingRad();
        float center_height = (float)mTerrainView->getCenterHeight();

#ifdef MGNTR_MERCATOR_TILE
        const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
        mTerrainView->LocalToPixelDistance(camera_distance, camera_distance, kMSM);
        mTerrainView->LocalToPixelDistance(center_height, center_height, kMSM);
#endif

        math::Matrix4 view_matrix;
        view_matrix = math::Scale4(1.0f, -1.0f, 1.0f);
        if (mTerrainView->isPanMap())
        {
            float cos_h = cos(camera_heading);
            float sin_h = sin(camera_heading);
            float cos_t = cos(camera_tilt);
            float sin_t = sin(camera_tilt);
            math::Matrix4 rotation_heading(
                cos_h, 0.0f, sin_h, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                -sin_h, 0.0f, cos_h, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );
            math::Matrix4 rotation_tilt(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, cos_t, -sin_t, 0.0f,
                0.0f, sin_t, cos_t, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );
            view_matrix *= math::Translate(0.0f, 0.0f, camera_distance);
            view_matrix *= rotation_tilt;
            view_matrix *= rotation_heading;
#ifndef MGNTR_MERCATOR_TILE
            view_matrix *= math::Translate(0.0f, -center_height, 0.0f);
#else
            const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
            vec3 translation;
            mTerrainView->PixelLocation(translation, kMSM);
            view_matrix *= math::Translate(-translation.x, -center_height, -translation.z);
#endif
        }
        else
        {
            float cos_t = cos(camera_tilt);
            float sin_t = sin(camera_tilt);
            float alpha = atan( (2.0f * mTerrainView->getAnchorRatioX() - 1.0f) * tan(0.5f * mFovX) );
            float beta  = atan( (2.0f * mTerrainView->getAnchorRatioY() - 1.0f) * tan(0.5f * mFovY) );
            float v = camera_distance * cos_t -
                (camera_distance * sin_t * tan(math::kHalfPi - beta - camera_tilt));
            float tx = camera_distance * tan(alpha);
            float gamma = atan(tx/(std::abs(v) + camera_distance * cos_t));

            float heading = camera_heading - gamma;
            float cos_h = cos(heading);
            float sin_h = sin(heading);
            math::Matrix4 rotation_heading(
                cos_h, 0.0f, sin_h, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                -sin_h, 0.0f, cos_h, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );
            math::Matrix4 rotation_tilt(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, cos_t, -sin_t, 0.0f,
                0.0f, sin_t, cos_t, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );
            view_matrix *= math::Translate(tx, -v * sin_t, camera_distance - v * cos_t);
            view_matrix *= rotation_tilt;
            view_matrix *= rotation_heading;
#ifndef MGNTR_MERCATOR_TILE
            view_matrix *= math::Translate(0.0f, -center_height, 0.0f);
#else
            const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
            vec3 translation;
            mTerrainView->PixelLocation(translation, kMSM);
            view_matrix *= math::Translate(-translation.x, -center_height, -translation.z);
#endif
        }
        mRenderer->SetViewMatrix(view_matrix);
    }
    void Renderer::LoadShaders()
    {
        {
            const char * attribs[] = {"a_position"};
            mRenderer->AddShader(mPositionShader, kPositionVertexShaderSource, kPositionFragmentShaderSource, attribs, 1);
        }
        {
            const char * attribs[] = {"a_position", "a_normal"};
            mRenderer->AddShader(mPositionNormalShader, kPositionNormalVertexShaderSource, kPositionNormalFragmentShaderSource, attribs, 2);
        }
        {
            // Temporary shader for dotted line rendering
            // TODO: should be exchanged for something more optimal and complex
            const char * attribs[] = {"a_position", "a_texcoord"};
            mRenderer->AddShader(mPositionTexcoordShader, kPositionTexcoordVertexShaderSource, kPositionTexcoordFragmentShaderSource, attribs, 2);
            if (mPositionTexcoordShader)
            {
                mPositionTexcoordShader->Bind();
                mPositionTexcoordShader->Uniform1i("u_texture", 0);
                mPositionTexcoordShader->Unbind();
            }
        }
        {
            const char * attribs[] = {"a_position", "a_shade", "a_texcoord"};
            mRenderer->AddShader(mPositionShadeTexcoordShader, kPositionShadeTexcoordVertexShaderSource, kPositionShadeTexcoordFragmentShaderSource, attribs, 3);
            if (mPositionShadeTexcoordShader)
            {
                mPositionShadeTexcoordShader->Bind();
                mPositionShadeTexcoordShader->Uniform1i("u_texture", 0);
                mPositionShadeTexcoordShader->Unbind();
            }
        }
        {
            const char * attribs[] = {"a_position", "a_texcoord"};
            mRenderer->AddShader(mBillboardShader, kBillboardVertexShaderSource, kBillboardFragmentShaderSource, attribs, 2);
            if (mBillboardShader)
            {
                mBillboardShader->Bind();
                mBillboardShader->Uniform1i("u_texture", 0);
                mBillboardShader->Unbind();
            }
        }
        {
            const char * attribs[] = {"a_position"};
            mRenderer->AddShader(mMercatorTileShader, kMercatorTileVertexShaderSource, kMercatorTileFragmentShaderSource, attribs, 1);
            if (mMercatorTileShader)
            {
                const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
                float height_multiplier = kMSM / 111111.0f / 360.0f;
                mMercatorTileShader->Bind();
                mMercatorTileShader->Uniform1f("u_map_size_max", kMSM);
                mMercatorTileShader->Uniform1f("u_height_multiplier", height_multiplier);
                mMercatorTileShader->Uniform1f("u_height_min", GetHeightMin());
                mMercatorTileShader->Uniform1f("u_height_range", GetHeightRange());
                mMercatorTileShader->Uniform1i("u_texture", 0);
                mMercatorTileShader->Uniform1i("u_height_map", 1);
                mMercatorTileShader->Unbind();
            }
        }
    }
    void Renderer::UpdateShaders()
    {
        const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
        const bool use_fog = mTerrainView->useFog();
        const float fog_modifier = (use_fog) ? 1.0f : 0.0f;
        float zfar = mTerrainView->getZFar();
        mTerrainView->LocalToPixelDistance(zfar, zfar, kMSM);

        mPositionShader->Bind();
        mPositionShader->UniformMatrix4fv("u_projection", mRenderer->projection_matrix());
        mPositionShader->UniformMatrix4fv("u_view", mRenderer->view_matrix());
        mPositionShader->Uniform1f("u_fog_modifier", fog_modifier);
        mPositionShader->Uniform1f("u_z_far", zfar);

        mPositionNormalShader->Bind();
        mPositionNormalShader->UniformMatrix4fv("u_projection", mRenderer->projection_matrix());
        mPositionNormalShader->UniformMatrix4fv("u_view", mRenderer->view_matrix());
        mPositionNormalShader->Uniform1f("u_fog_modifier", fog_modifier);
        mPositionNormalShader->Uniform1f("u_z_far", zfar);

        mPositionTexcoordShader->Bind();
        mPositionTexcoordShader->UniformMatrix4fv("u_projection", mRenderer->projection_matrix());
        mPositionTexcoordShader->UniformMatrix4fv("u_view", mRenderer->view_matrix());
        mPositionTexcoordShader->Uniform1f("u_fog_modifier", fog_modifier);
        mPositionTexcoordShader->Uniform1f("u_z_far", zfar);

        mPositionShadeTexcoordShader->Bind();
        mPositionShadeTexcoordShader->UniformMatrix4fv("u_projection_view", mProjectionViewMatrix);
        // Used for vehicle rendering only, thus we don't need a fog for it.

        mBillboardShader->Bind();
        mBillboardShader->UniformMatrix4fv("u_projection", mRenderer->projection_matrix());
        mBillboardShader->UniformMatrix4fv("u_view", mRenderer->view_matrix());
        mBillboardShader->Uniform1f("u_fog_modifier", fog_modifier);
        mBillboardShader->Uniform1f("u_z_far", zfar);

        //if (use_fog)
        //{
        //    vec4 params;
        //    float size = mVehicleRenderer->GetSize();
        //    math::CalculateObjectOcclusionParams(mRenderer->projection_matrix(), mRenderer->view_matrix(),
        //        mRenderer->viewport(), mVehicleRenderer->GetCenter(), size, params);

        //    mBillboardShader->Uniform1i("u_occlusion_enabled", 1);
        //    mBillboardShader->Uniform2f("u_occludee_params.center", params.x, params.y);
        //    mBillboardShader->Uniform1f("u_occludee_params.radius", params.z);
        //    mBillboardShader->Uniform1f("u_occludee_params.distance", params.w);
        //    mBillboardShader->Uniform1f("u_occludee_params.size", size);
        //}
        //else
        {
            mBillboardShader->Uniform1i("u_occlusion_enabled", 0);
        }

        mMercatorTileShader->Bind();
        mMercatorTileShader->UniformMatrix4fv("u_projection_view_model", mProjectionViewMatrix);
    }
    void Renderer::UpdateLocationIndicator(double lat, double lon, double altitude,
        double heading, double tilt,int color1, int color2)
    {
#ifndef MGNTR_MERCATOR_TILE
        mVehicleRenderer->UpdateLocationIndicator(lat, lon, altitude, heading, tilt, color1, color2);
        mDirectionalLineRenderer->onGpsPositionChange();
        mActiveTrackRenderer->onGpsPositionChange();
        mHighlightTrackRenderer->onGpsPositionChange(lat, lon);
        mPassiveHighlightTrackRenderer->onGpsPositionChange(lat, lon);
#endif
    }
    void Renderer::IntersectionWithRay(const math::Vector3& ray, math::Vector3& intersection) const
    {
        mTerrainView->IntersectionWithRay(ray, intersection);
    }
    void Renderer::GetSelectedIcons(int x, int y, int radius, std::vector<int>& ids) const
    {
#ifndef MGNTR_MERCATOR_TILE
        mTerrainMap->GetSelectedIcons(x, mSizeY - y, radius, ids);
#endif
    }
    void Renderer::GetSelectedTracks(int x, int y, int radius, std::vector<int>& ids) const
    {
#ifndef MGNTR_MERCATOR_TILE
        mTerrainMap->GetSelectedTracks(x, mSizeY - y, radius, ids);
#endif
    }
    void Renderer::GetSelectedTrails(int x, int y, int radius, bool online, std::vector<std::string>& ids) const
    {
#ifndef MGNTR_MERCATOR_TILE
        mTerrainMap->GetSelectedTrails(x, mSizeY - y, radius, online, ids);
#endif
    }
    void Renderer::GetSelectedPOIs(int x, int y, int radius, TMapObjectsVector& objects) const
    {
#ifndef MGNTR_MERCATOR_TILE
        mTerrainMap->GetSelectedPOIs(x, mSizeY - y, radius, objects);
#endif
    }
    void Renderer::UpdatePOISelection(const std::vector<int>& selection)
    {
#ifndef MGNTR_MERCATOR_TILE
        mTerrainMap->UpdatePOISelection(selection);
#endif
    }

    } // namespace terrain
} // namespace mgn