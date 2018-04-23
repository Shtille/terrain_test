#include "mgnTrActiveTrackRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnUgdsTrackUserData.h" // for tracks and active track
#include "UGDSInterface.hpp"
#include "UGDSTypesAndConsts.hpp"
#include "mgnUgdsManager.h"
#include "mgnTrackDataProvider.h"

#include <assert.h>
#include <cmath>

namespace mgn {
    namespace terrain {

        ActiveTrackRenderer::ActiveTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                        graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
        : DottedLineRenderer(renderer, terrain_view, provider, shader, gps_pos)
        , mNumPoints(0)
        , mVisible(true)
        , mIsLoading(false)
        {
        }
        ActiveTrackRenderer::~ActiveTrackRenderer()
        {
        }
        void ActiveTrackRenderer::createTexture()
        {
            int w = 32;
            int h = 32;

            int *pixels = new int[w*h];

            for(int y = 0; y < h; ++y)
            {
                for(int x = 0; x < w; ++x)
                {
                    float r = (float)w/2;
                    float dx = (float)(x-w/2);
                    float dy = (float)(y-h/2);
                    float len = sqrt(dx*dx + dy*dy);
                    if (len <= r)
                        pixels[y*w+x] = 0xffff0000;
                    else
                        pixels[y*w+x] = 0;
                }
            }

            unsigned char* data = reinterpret_cast<unsigned char*>(pixels);
            renderer_->CreateTextureFromData(texture_, w, h,
                graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, data);

            delete[] pixels;
        }
        void ActiveTrackRenderer::clearSegments()
        {
            for (std::list<DottedLineSegment*>::iterator it = mSegments.begin(); it != mSegments.end(); ++it)
            {
                DottedLineSegment * segment = *it;
                if (segment)
                    delete segment;
            }
            mSegments.clear();
            mNumPoints = 0;
            mVisible = true;
            mIsLoading = false;
        }
        void ActiveTrackRenderer::doFetch()
        {
            mgnUgdsManager * manager = mgnUgdsManager::GetInstance();
            CUGDSInterface * ugds = manager->GetUGDS();
            int num_points = ugds->TRK_PointsExport(UGDS_ActiveTrackID);
            if (num_points > mNumPoints && mVisible)
            {
                if (mNumPoints == 0)
                {
                    // Check active track visibility
                    // NOTE: We assume that track visibility is constant per track
                    mgnUDO::TrackDataProvider tdp;
                    mgnUDO::TrackInfo track_info;
                    if (tdp.GetObjInfoById(UGDS_ActiveTrackID, track_info))
                    {
                        mVisible = track_info.IsVisible();
                        if (!mVisible)
                            return;
                    }
                }
                UGDS_TrackPoint_t *points = new UGDS_TrackPoint_t[num_points];
                if (ugds->TRK_PointsExport(UGDS_ActiveTrackID, 
                                          0,                            // start index
                                          num_points-1,                 // end index
                                          num_points,                   // buffer size
                                          points) > 0)
                {
                    vec2 cur, next;
                    if (!mIsLoading && mNumPoints > 0 && !mSegments.empty() &&
                        !(dynamic_cast<ActiveTrackSegment*>(mSegments.back()))->mHasBreak)
                    {
                        // Correct last segment(s)
                        while (!mSegments.empty() &&
                            (dynamic_cast<ActiveTrackSegment*>(mSegments.back()))->mToLocation)
                        {
                            delete mSegments.back();
                            mSegments.pop_back();
                        }

                        mgnMdWorldPoint curp, nextp;
                        curp.mLatitude = points[mNumPoints-1].GeoPt.GetDoubleLat();
                        curp.mLongitude = points[mNumPoints-1].GeoPt.GetDoubleLon();
                        nextp.mLatitude = points[mNumPoints].GeoPt.GetDoubleLat();
                        nextp.mLongitude = points[mNumPoints].GeoPt.GetDoubleLon();
                        bool has_break = (points[mNumPoints].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                        addSegment(curp, nextp, has_break, false);
                    }
                    const int kMaxPointsProcessedPerTick = 100;
                    if (num_points > mNumPoints + kMaxPointsProcessedPerTick)
                    {
                        mIsLoading = true;
                        for (int i = mNumPoints; i < mNumPoints + kMaxPointsProcessedPerTick; ++i)
                        {
                            mgnMdWorldPoint curp, nextp;
                            curp.mLatitude = points[i].GeoPt.GetDoubleLat();
                            curp.mLongitude = points[i].GeoPt.GetDoubleLon();
                            bool cur_has_break = (points[i].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                            if (cur_has_break)
                                continue;
                            nextp.mLatitude = points[i+1].GeoPt.GetDoubleLat();
                            nextp.mLongitude = points[i+1].GeoPt.GetDoubleLon();
                            bool next_has_break = (points[i+1].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                            addSegment(curp, nextp, next_has_break, false);
                        }
                        mNumPoints += kMaxPointsProcessedPerTick;
                    }
                    else // standard mode
                    {
                        mIsLoading = false;
                        for (int i = mNumPoints; i < num_points-1; ++i)
                        {
                            mgnMdWorldPoint curp, nextp;
                            curp.mLatitude = points[i].GeoPt.GetDoubleLat();
                            curp.mLongitude = points[i].GeoPt.GetDoubleLon();
                            bool cur_has_break = (points[i].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                            if (cur_has_break)
                                continue;
                            nextp.mLatitude = points[i+1].GeoPt.GetDoubleLat();
                            nextp.mLongitude = points[i+1].GeoPt.GetDoubleLon();
                            bool next_has_break = (points[i+1].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                            addSegment(curp, nextp, next_has_break, false);
                        }
                        mNumPoints = num_points;
                    }
                    
                    // to the current location - we will use it only in simulation mode
                    if (!mIsLoading && (points[num_points-1].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) != UGDS_TKPTS_BREAK)
                    {
                        mgnMdWorldPoint curp, nextp;
                        curp.mLatitude = points[num_points-1].GeoPt.GetDoubleLat();
                        curp.mLongitude = points[num_points-1].GeoPt.GetDoubleLon();
                        nextp.mLatitude = mGpsPosition.mLatitude;
                        nextp.mLongitude = mGpsPosition.mLongitude;
                        addSegment(curp, nextp, false, true);
                    }
                }
                delete[] points;
            }
            else if (num_points < mNumPoints)
            {
                clearSegments();
            }
            else if (!mVisible && num_points == 0)
            {
                // Seems that track has been cleared - so we need to clear the visibility flag.
                mVisible = true;
            }

            // We should also update the last segment to current vehicle position on change
            if (!mIsLoading && !mSegments.empty() && !(dynamic_cast<ActiveTrackSegment*>(mSegments.back()))->mHasBreak) 
            {
                const bool position_has_changed = fabs(mGpsPosition.mLatitude  - mOldPosition.mLatitude ) > 1e-9 ||
                                                  fabs(mGpsPosition.mLongitude - mOldPosition.mLongitude) > 1e-9;
                if (position_has_changed)
                {
                    mgnMdWorldPoint curp, nextp;
                    curp.mLatitude = mSegments.back()->mBegin.world.point.mLatitude;
                    curp.mLongitude = mSegments.back()->mBegin.world.point.mLongitude;
                    nextp.mLatitude = mGpsPosition.mLatitude;
                    nextp.mLongitude = mGpsPosition.mLongitude;

                    while (!mSegments.empty() &&
                        (dynamic_cast<ActiveTrackSegment*>(mSegments.back()))->mToLocation)
                    {
                        delete mSegments.back();
                        mSegments.pop_back();
                    }
                    
                    addSegment(curp, nextp, false, true);
                }
            }
        }
        void ActiveTrackRenderer::addSegment(const mgnMdWorldPoint& begin, const mgnMdWorldPoint& end, bool has_break, bool to_location)
        {
            double dlat = end.mLatitude - begin.mLatitude;
            double dlon = end.mLongitude - begin.mLongitude;
            double len = sqrt( dlat * dlat + dlon * dlon );

            const double kDivisionStepMeters = elementStep();
            const double division_step = kDivisionStepMeters / terrain_view_->getMetersPerLatitude();
            const double max_length = 0.5 * division_step * maxRenderedPointsPerSegment();

            const int kMaxStepsAllowed = 100;
            int num_steps = (int)(len / max_length) + 1;

            if (len > max_length && num_steps <= kMaxStepsAllowed) // need to subdivide segment
            {
                mgnMdWorldPoint current_begin, current_end;
                double offset = 0.0;
                double distance = len;
                for (int i = 0; i < num_steps; ++i)
                {
                    double length = std::min(max_length, distance);
                    if (!has_break && length < 1e-9) // skip zero-length segments
                        break;

                    current_begin.mLatitude  = begin.mLatitude  + (dlat / len) * offset;
                    current_begin.mLongitude = begin.mLongitude + (dlon / len) * offset;
                    current_end.mLatitude    = begin.mLatitude  + (dlat / len) * (offset + length);
                    current_end.mLongitude   = begin.mLongitude + (dlon / len) * (offset + length);

                    DottedLineSegment * prev_segment = (mSegments.empty()) ? NULL : mSegments.back();
                    DottedLineSegment * segment = new ActiveTrackSegment(current_begin, current_end,
                        (i+1 == num_steps) ? has_break : false, to_location);
                    mSegments.push_back(segment);
                    onAddSegment(segment, prev_segment);

                    offset += max_length;
                    distance -= max_length;
                }
            }
            else
            {
                DottedLineSegment * prev_segment = (mSegments.empty()) ? NULL : mSegments.back();
                DottedLineSegment * segment = new ActiveTrackSegment(begin, end, has_break, to_location);
                mSegments.push_back(segment);
                onAddSegment(segment, prev_segment);
            }
        }
        void ActiveTrackRenderer::render(const math::Frustum& frustum)
        {
            if (!texture_)
                createTexture();

            renderer_->ChangeVertexFormat(vertex_format_);
            renderer_->ChangeVertexBuffer(vertex_buffer_);
            renderer_->ChangeIndexBuffer(index_buffer_);

            shader_->Bind();

            renderer_->ChangeTexture(texture_);

            if (mHighAltitudes)
                renderer_->DisableDepthTest();
            else
                renderer_->context()->EnablePolygonOffset(-1.0f, -1.0f);

            float cam_dist = (float)terrain_view_->getCamDistance();
            float distance_gain = cam_dist * 0.010f;
            float scale = std::max(distance_gain, 1.0f);

            float num_skipping = std::max(distance_gain - 1.0f, 0.0f);
            int skipping = 0;
            // We will calculate each point every frame.
            // Pretty expensive, but in dynamics, it seems the only solution.
            const double kDivisionStepMeters = elementStep();
            const double division_step = kDivisionStepMeters / terrain_view_->getMetersPerLatitude();

            const int kMaxRenderedPoints = 1000;
            int rendered_points = 0;

            double prev_segment_offset = 0.0;

            for (std::list<DottedLineSegment*>::reverse_iterator it = mSegments.rbegin();
                    it != mSegments.rend(); ++it)
            {
                ActiveTrackSegment * segment = dynamic_cast<ActiveTrackSegment*>(*it);

                int skipping_current;
                if (segment->mSkipPoint != 0)
                {
                    skipping = 0; // reset skipping count
                    skipping_current = (int)(num_skipping / (float)segment->mSkipPoint);
                }
                else
                    skipping_current = (int) num_skipping;

                double local_x, local_y;
                terrain_view_->WorldToLocal(segment->mBegin.world.point, local_x, local_y);
                float offset_x = (float)local_x;
                float offset_y = (float)local_y;

                // Skip segments that are not visible
                const int kNumPoints = 4;
                vec3 points[kNumPoints]; // in global CS
                points[0].x = offset_x;
                points[0].y = segment->mMinHeight;
                points[0].z = offset_y;
                points[1].x = offset_x + segment->mEnd.local.position.x;
                points[1].y =            segment->mMinHeight;
                points[1].z = offset_y + segment->mEnd.local.position.z;
                points[2].x = offset_x;
                points[2].y = segment->mMaxHeight;
                points[2].z = offset_y;
                points[3].x = offset_x + segment->mEnd.local.position.x;
                points[3].y =            segment->mMaxHeight;
                points[3].z = offset_y + segment->mEnd.local.position.z;

                if (!frustum.IsPolygonIn(kNumPoints, points))
                {
                    continue;
                }

                // Segment is visible, so we can continue calculation.
                double dlat = segment->mBegin.world.point.mLatitude - segment->mEnd.world.point.mLatitude;
                double dlon = segment->mBegin.world.point.mLongitude - segment->mEnd.world.point.mLongitude;
                double len = sqrt(dlat*dlat + dlon*dlon);
                if (len < 1e-9) // zero-length segments
                    continue;

                double offset = prev_segment_offset;
                int num_points = (int)((len - offset) / division_step);
                num_points = std::min(num_points, maxRenderedPointsPerSegment());

                bool skip_this_segment = false;
                if (prev_segment_offset > len)
                {
                    // Segment is too small, we can skip it
                    prev_segment_offset = prev_segment_offset - len;
                    skip_this_segment = true;
                }
                else
                {
                    int num_steps = (int)((len - prev_segment_offset)/ division_step);
                    prev_segment_offset = prev_segment_offset + division_step*(num_steps+1) - len;
                }
                if (skip_this_segment)
                    continue;

                // Then render all points
                for (int i = 0; i <= num_points; ++i)
                {
                    if (skipping > 0)
                    {
                        --skipping;
                        continue;
                    }
                    ++rendered_points;
                    skipping = skipping_current;
                    double cur_len = i * division_step + offset;
                    DottedLinePointInfo point;
                    point.world.point.mLatitude  = segment->mEnd.world.point.mLatitude  + (dlat / len) * cur_len;
                    point.world.point.mLongitude = segment->mEnd.world.point.mLongitude + (dlon / len) * cur_len;
                    point.obtain_position(terrain_view_, provider_, offset_x, offset_y, 
                        segment->mMinHeight, segment->mMaxHeight);
                    calcNormal(point, scale);

                    renderer_->PushMatrix();
                    renderer_->Translate(point.local.position.x + offset_x, 
                                         point.local.position.y, 
                                         point.local.position.z + offset_y);
                    renderer_->Scale(scale);
                    if (!mHighAltitudes)
                        renderer_->MultMatrix(point.rotation.matrix);
                    shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
                    renderer_->DrawElements(graphics::PrimitiveType::kTriangleStrip);
                    renderer_->PopMatrix();
                }
                if (rendered_points >= kMaxRenderedPoints)
                    break;
            }

            if (mHighAltitudes)
                renderer_->EnableDepthTest();
            else
                renderer_->context()->DisablePolygonOffset();

            shader_->Unbind();
        }

    } // namespace terrain
} // namespace mgn