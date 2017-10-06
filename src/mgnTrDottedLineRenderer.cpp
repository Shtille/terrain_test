#include "mgnTrDottedLineRenderer.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include <cmath>
#include <cstddef>

namespace mgn {
    namespace terrain {

        void DottedLinePointInfo::set_begin(mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider, float& minh, float& maxh)
        {
            const float dxm = ((float)terrain_view->getMagnitude())/111111.0f;
            local.position.x = 0.0f;
            local.position.z = 0.0f;
            local.position.y = (float)provider->getAltitude(
                world.point.mLatitude,
                world.point.mLongitude,
                dxm);
            minh = local.position.y;
            maxh = local.position.y;
        }
        void DottedLinePointInfo::obtain_position(mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider, float offset_x, float offset_y, float& minh, float& maxh)
        {
            const float dxm = ((float)terrain_view->getMagnitude())/111111.0f;
            double local_x, local_y;
            terrain_view->WorldToLocal(world.point, local_x, local_y);
            local.position.x = (float)local_x - offset_x;
            local.position.z = (float)local_y - offset_y;
            local.position.y = (float)provider->getAltitude(
                world.point.mLatitude,
                world.point.mLongitude,
                dxm);
            if (local.position.y < minh)
                minh = local.position.y;
            if (local.position.y > maxh)
                maxh = local.position.y;
        }
        void DottedLinePointInfo::fill_rotation(const vec3& vx, const vec3& vy, const vec3& vz)
        {
            // Our vectors are in right hand space rotated by 90° from original one.
            // So transform it into left hand gay space by swapping x and z.
            // But we need RHS here, so we should invert z axis.
            rotation.matrix.sa[ 0] = vx.z;
            rotation.matrix.sa[ 1] = vx.y;
            rotation.matrix.sa[ 2] = vx.x;
            rotation.matrix.sa[ 3] = 0.0f;

            rotation.matrix.sa[ 4] = vy.z;
            rotation.matrix.sa[ 5] = vy.y;
            rotation.matrix.sa[ 6] = vy.x;
            rotation.matrix.sa[ 7] = 0.0f;

            rotation.matrix.sa[ 8] = -vz.z;
            rotation.matrix.sa[ 9] = -vz.y;
            rotation.matrix.sa[10] = -vz.x;
            rotation.matrix.sa[11] = 0.0f;

            rotation.matrix.sa[12] = 0.0f;
            rotation.matrix.sa[13] = 0.0f;
            rotation.matrix.sa[14] = 0.0f;
            rotation.matrix.sa[15] = 1.0f;
        }

        DottedLineRenderer::DottedLineRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
            : renderer_(renderer)
            , terrain_view_(terrain_view)
            , provider_(provider)
            , shader_(shader)
            , gps_pos_(gps_pos)
            , mGpsPosition(0.0, 0.0)
            , mOldPosition(0.0, 0.0)
            , mHighAltitudes(false)
            , mAnySegmentToFill(false)
            , texture_(NULL)
        {
            create();
        }
        DottedLineRenderer::~DottedLineRenderer()
        {
            for (std::list<DottedLineSegment*>::iterator it = mSegments.begin();
                it != mSegments.end(); ++it)
                if (*it)
                    delete *it;
            mSegments.clear();

            if (vertex_format_)
                renderer_->DeleteVertexFormat(vertex_format_);
            if (vertex_buffer_)
                renderer_->DeleteVertexBuffer(vertex_buffer_);
            if (index_buffer_)
                renderer_->DeleteIndexBuffer(index_buffer_);
            if (texture_)
                renderer_->DeleteTexture(texture_);
        }
        void DottedLineRenderer::onAddSegment(DottedLineSegment * segment)
        {
            if (segment == NULL) return;

            double local_x, local_y;
            terrain_view_->WorldToLocal(segment->mBegin.world.point, local_x, local_y);
            float offset_x = (float)local_x;
            float offset_y = (float)local_y;
            segment->mBegin.set_begin(terrain_view_, provider_, segment->mMinHeight, segment->mMaxHeight);
            segment->mEnd.obtain_position(terrain_view_, provider_, offset_x, offset_y, 
                segment->mMinHeight, segment->mMaxHeight);
        }
        void DottedLineRenderer::onAddSegment(DottedLineSegment * segment, DottedLineSegment * prev_segment)
        {
            double local_x, local_y;
            terrain_view_->WorldToLocal(segment->mBegin.world.point, local_x, local_y);
            float offset_x = (float)local_x;
            float offset_y = (float)local_y;
            segment->mBegin.set_begin(terrain_view_, provider_, segment->mMinHeight, segment->mMaxHeight);
            segment->mEnd.obtain_position(terrain_view_, provider_, offset_x, offset_y, 
                segment->mMinHeight, segment->mMaxHeight);

            mgnMdWorldPoint& begin = segment->mBegin.world.point;
            mgnMdWorldPoint& end = segment->mEnd.world.point;
            // We will fill segment world data
            double dlat = end.mLatitude - begin.mLatitude;
            double dlon = end.mLongitude - begin.mLongitude;
            double len = sqrt( dlat * dlat + dlon * dlon );
            if (prev_segment)
            {
                if (prev_segment->mOffset > len || len < 1e-9)
                {
                    // Segment is too small, we can skip it
                    segment->mOffset = prev_segment->mOffset - len;
                    segment->mCalculated = true;
                }
                else
                {
                    const double kDivisionStepMeters = elementStep();
                    const double division_step = kDivisionStepMeters / terrain_view_->getMetersPerLatitude();
                    int num_steps = (int)((len - prev_segment->mOffset)/ division_step);
                    segment->mOffset = prev_segment->mOffset + division_step*(num_steps+1) - len;
                }
            }
            else
            {
                const double kDivisionStepMeters = elementStep();
                const double division_step = kDivisionStepMeters / terrain_view_->getMetersPerLatitude();
                int num_steps = (int)(len / division_step);
                segment->mOffset = division_step*(num_steps+1) - len;
            }
        }
        void DottedLineRenderer::onGpsPositionChange()
        {
            mGpsPosition = *gps_pos_;

            doFetch(); // fetch offroad data

            mOldPosition = mGpsPosition;
        }
        void DottedLineRenderer::onViewChange()
        {
            mHighAltitudes = terrain_view_->getMagnitude() / 111111.0 > 0.05;
        }
        void DottedLineRenderer::fillSegment(DottedLineSegment * prev_segment, DottedLineSegment * segment)
        {
            segment->mCalculated = true;
            segment->mNeedToCalculate = false;

            const double kDivisionStepMeters = elementStep();
            const double division_step = kDivisionStepMeters / terrain_view_->getMetersPerLatitude();

            mgnMdWorldPoint& begin = segment->mBegin.world.point;
            mgnMdWorldPoint& end = segment->mEnd.world.point;
            // We will fill segment world data
            double dlat = end.mLatitude - begin.mLatitude;
            double dlon = end.mLongitude - begin.mLongitude;
            double len = sqrt( dlat * dlat + dlon * dlon );
            double offset = (prev_segment) ? prev_segment->mOffset : 0.0;
            int num_steps = (int)((len - offset) / division_step);
            int num_points = num_steps + 1;

            double local_x, local_y;
            terrain_view_->WorldToLocal(begin, local_x, local_y);
            float offset_x = (float)local_x;
            float offset_y = (float)local_y;

            // Don't forget to transform to_traget to RHS
            // mBegin has (0,0) coords, so we can optimize computation (also ignore y direction)
            const vec3 to_target(segment->mEnd.local.position.z, 0.0f, segment->mEnd.local.position.x);

            const bool is_long_segment = isUsingLongSegmentFix() && num_steps > 100;
            if (is_long_segment)
            {
                segment->mSkipPoint = num_steps/100;
                segment->mNumPoints = 0;
                segment->mPoints = new DottedLinePointInfo[101]; // may be 101, depends on rounding
            }
            else
            {
                segment->mNumPoints = num_points;
                segment->mPoints = new DottedLinePointInfo[num_points];
            }
            int num_to_skip = 0;
            for (int i = 0; i <= num_steps; ++i)
            {
                if (isUsingLongSegmentFix())
                {
                    if(num_to_skip > 0 && i != num_steps)
                    {
                        --num_to_skip;
                        continue;
                    }
                    num_to_skip = segment->mSkipPoint;
                }
                int ind;
                if (is_long_segment) // need to count points
                    ind = segment->mNumPoints++;
                else
                    ind = i;
                double cur_len = offset + division_step * i;
                segment->mPoints[ind].world.point.mLatitude = begin.mLatitude + (dlat / len) * cur_len;
                segment->mPoints[ind].world.point.mLongitude = begin.mLongitude + (dlon / len) * cur_len;
                segment->mPoints[ind].obtain_position(terrain_view_, provider_, offset_x, offset_y, 
                    segment->mMinHeight, segment->mMaxHeight);
                calcNormal(segment->mPoints[ind], to_target);
            }
        }
        void DottedLineRenderer::calcNormal(DottedLinePointInfo& point, float circle_size)
        {
            vec3 normal;
            float dxm = ((float)terrain_view_->getMagnitude())/111111.0f;
            // Fast, but inaccurate computation
            double d = circle_size * elementSize() * 1.41;
            double dlon = d / terrain_view_->getMetersPerLongitude();
            double dlat = d / terrain_view_->getMetersPerLatitude();
            float h_x_plus  = (float)provider_->getAltitude(point.world.point.mLatitude, point.world.point.mLongitude + dlon, dxm); // x+1
            float h_x_minus = (float)provider_->getAltitude(point.world.point.mLatitude, point.world.point.mLongitude - dlon, dxm); // x-1
            float h_y_plus  = (float)provider_->getAltitude(point.world.point.mLatitude + dlat, point.world.point.mLongitude, dxm); // y+1
            float h_y_minus = (float)provider_->getAltitude(point.world.point.mLatitude - dlat, point.world.point.mLongitude, dxm); // y-1
            float sx = h_x_plus - h_x_minus;
            float sy = h_y_plus - h_y_minus;
            // assume that tile cell sizes in both directions are the same
            // also swap x and z to convert LHS normal to RHS
            normal.Set(-sy, 2.0f*(float)d, -sx);
            normal.Normalize();
            if (normal.y > 0.99f)
            {
                point.fill_rotation(UNIT_X, UNIT_Y, UNIT_Z);
            }
            else
            {
                vec3 side = normal ^ UNIT_Y;
                side.Normalize();
                vec3 dir = normal ^ side;
                dir.Normalize();
                point.fill_rotation(dir, normal, side);

                // Correct altitude
                float d_yz = normal.z / sqrtf(1.0f - normal.x * normal.x);
                float d_yx = normal.x / sqrtf(1.0f - normal.z * normal.z);

                const float size = (float)d;
                float h_mid = point.local.position.y;
                float h_c;
                float dh = 0.0f;
                h_c = h_mid - d_yz * size; // +z = +lon
                dh = std::max(h_x_plus - h_c, dh);
                h_c = h_mid + d_yz * size; // -z = -lon
                dh = std::max(h_x_minus - h_c, dh);
                h_c = h_mid - d_yx * size; // +x = +lat
                dh = std::max(h_y_plus - h_c, dh);
                h_c = h_mid + d_yx * size; // -x = -lat
                dh = std::max(h_y_minus - h_c, dh);
                point.local.position.y += dh;
            }
        }
        void DottedLineRenderer::calcNormal(DottedLinePointInfo& point, const vec3& to_target)
        {
            vec3 normal;
            if (isUsingFastNormalComputation())
            {
                float dxm = ((float)terrain_view_->getMagnitude())/111111.0f;
                // Fast, but inaccurate computation
                double d = terrain_view_->getCellSizeLat()/double(GetTileHeightSamples()-1);
                double dlon = d / terrain_view_->getMetersPerLongitude();
                double dlat = d / terrain_view_->getMetersPerLatitude();
                float sx =      
                    (float)provider_->getAltitude(point.world.point.mLatitude, 
                                                                     point.world.point.mLongitude + dlon,
                                                                     dxm) - // x+1
                    (float)provider_->getAltitude(point.world.point.mLatitude,
                                                                     point.world.point.mLongitude - dlon,
                                                                     dxm);  // x-1
                float sy =      
                    (float)provider_->getAltitude(point.world.point.mLatitude + dlat, 
                                                                     point.world.point.mLongitude,
                                                                     dxm) - // y+1
                    (float)provider_->getAltitude(point.world.point.mLatitude - dlat,
                                                                     point.world.point.mLongitude,
                                                                     dxm);  // y-1
                // assume that tile cell sizes in both directions are the same
                // also swap x and z to convert LHS normal to RHS
                normal.Set(-sy, 2.0f*(float)d, -sx);
                normal.Normalize();
            }
            else // more slow, but accurate computation
            {
                double d = 1.5;
                double dlon = d / terrain_view_->getMetersPerLongitude();
                double dlat = d / terrain_view_->getMetersPerLatitude();
                double local_x, local_y;
                terrain_view_->WorldToLocal(point.world.point, local_x, local_y);
                vec3 p0, p1, p2, p3, p4;
                // Our CS is left hand, so transform it to right hand by swapping x and z
                p0.x = (float)local_y;
                p0.z = (float)local_x;
                p0.y = (float)point.local.position.y;
                p1.x = (float)local_y;
                p1.z = (float)(local_x + d);
                p1.y = (float)provider_->getAltitude(point.world.point.mLatitude, point.world.point.mLongitude + dlon);
                p2.x = (float)(local_y + d);
                p2.z = (float)local_x;
                p2.y = (float)provider_->getAltitude(point.world.point.mLatitude + dlat, point.world.point.mLongitude);
                p3.x = (float)local_y;
                p3.z = (float)(local_x - d);
                p3.y = (float)provider_->getAltitude(point.world.point.mLatitude, point.world.point.mLongitude - dlon);
                p4.x = (float)(local_y - d);
                p4.z = (float)local_x;
                p4.y = (float)provider_->getAltitude(point.world.point.mLatitude - dlat, point.world.point.mLongitude);
                vec3 n1 = (p1 - p0) ^ (p2 - p0);
                vec3 n2 = (p2 - p0) ^ (p3 - p0);
                vec3 n3 = (p3 - p0) ^ (p4 - p0);
                vec3 n4 = (p4 - p0) ^ (p1 - p0);
                normal = n1 + n2 + n3 + n4;
                normal.Normalize();
            }

            // Now we can compute orientation
            const vec3& up = normal; // up is always as normal
            // dir lies in the same vertical plane as "to target" vector
            // so vertical plane has normal:
            vec3 vp_normal = to_target ^ UNIT_Y;
            vp_normal.Normalize();
            // dir is perpendicular to both plane normals:
            vec3 dir = normal ^ vp_normal;
            dir.Normalize();
            // choose right dir direction (pun, lol'd)
            if ((dir & to_target) < 0.0f)
                dir = -dir;
            // finding side is pretty simple:
            vec3 side = dir ^ up;
            side.Normalize();
            point.fill_rotation(dir, up, side);
        }
        void DottedLineRenderer::calcFlatNormal(DottedLinePointInfo& point, const vec3& to_target)
        {
            // Same as calcNormal, but on high altitudes
            vec3 dir = to_target;
            dir.Normalize();
            vec3 side = dir ^ UNIT_Y;
            side.Normalize();
            vec3 up = side ^ dir;
            up.Normalize();
            point.fill_rotation(dir, up, side);
        }
        void DottedLineRenderer::update(const math::Frustum&)
        {
            if (isUsingPointsCache() && mAnySegmentToFill)
            {
                mAnySegmentToFill = false;

                DottedLineSegment * prev_segment = NULL;
                for (std::list<DottedLineSegment*>::iterator it = mSegments.begin(); it != mSegments.end(); ++it)
                {
                    DottedLineSegment * segment = *it;
                    if (segment->mNeedToCalculate)
                    {
                        fillSegment(prev_segment, segment);
                    }
                    prev_segment = segment;
                }
            }
        }
        void DottedLineRenderer::render(const math::Frustum& frustum)
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
            //context.setBlend(true);

            float cam_dist = (float)terrain_view_->getCamDistance();
            float distance_gain = cam_dist * 0.010f;
            float scale = std::max(distance_gain, 1.0f);

            if (isUsingPointsCache())
            {
                float num_skipping = std::max(distance_gain - 1.0f, 0.0f);
                float dxm = ((float)terrain_view_->getMagnitude())/111111.0f;
                int skipping = 0;

                // Start line rendering
                for (std::list<DottedLineSegment*>::iterator it = mSegments.begin();
                        it != mSegments.end(); ++it)
                {
                    DottedLineSegment * segment = *it;
                    if (segment == NULL) continue;

                    if (isUsingSeparateSegments())
                        skipping = 0; // reset skipping count

                    int skipping_current;
                    if (isUsingLongSegmentFix() && segment->mSkipPoint != 0)
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
                    const int num_points = 4;
                    vec3 points[num_points]; // in global CS
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

                    if (!frustum.IsPolygonIn(num_points, points))
                    {
                        /*
                        Calculate that skipping value will after skipping this segment (pun, tho' :D)
                        I'll describe my formula:
                            N - number of points in segment
                            S0 - initial skipping value
                            S - skipping value on the current segment
                            S1 - skipping value that will be after (required)
                            if (N-1 > S0)
                             S1 = (S+1)-((N-1-S0)%(S+1))-1
                            else
                             S1 = S0-N+1
                        */
                        int num_points = segment->mNumPoints;
                        if (num_points > 0)
                        {
                            if (num_points > skipping + 1)
                                skipping = skipping_current - (num_points - 1 - skipping) % (skipping_current + 1);
                            else
                                skipping = skipping + 1 - num_points;
                        }
                        continue;
                    }

                    if (!segment->mCalculated) // do this only with visible segments
                    {
                        segment->mNeedToCalculate = true;
                        mAnySegmentToFill = true;
                        continue;
                    }

                    for (int i = 0; i < segment->mNumPoints; ++i)
                    {
                        if (skipping > 0)
                        {
                            --skipping;
                            continue;
                        }
                        skipping = skipping_current;

                        DottedLinePointInfo& point = segment->mPoints[i];

                        // not skipping by now
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
                }
            }
            else // not using cache
            {
                float num_skipping = std::max(distance_gain - 1.0f, 0.0f);
                int skipping = 0;
                // We will calculate each point every frame.
                // Pretty expensive, but in dynamics, it seems the only solution.
                const double kDivisionStepMeters = elementStep();
                const double division_step = kDivisionStepMeters / terrain_view_->getMetersPerLatitude();

                DottedLineSegment * prev_segment = NULL;

                for (std::list<DottedLineSegment*>::iterator it = mSegments.begin();
                        it != mSegments.end(); ++it)
                {
                    DottedLineSegment * segment = *it;

                    if (segment->mCalculated) // segment could be too short
                    {
                        prev_segment = segment;
                        continue;
                    }

                    if (isUsingSeparateSegments())
                        skipping = 0; // reset skipping count

                    int skipping_current;
                    if (isUsingLongSegmentFix() && segment->mSkipPoint != 0)
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
                        prev_segment = segment;
                        continue;
                    }

                    // Segment is visible, so we can continue calculation.
                    double dlat = segment->mEnd.world.point.mLatitude - segment->mBegin.world.point.mLatitude;
                    double dlon = segment->mEnd.world.point.mLongitude - segment->mBegin.world.point.mLongitude;
                    double len = sqrt(dlat*dlat + dlon*dlon);
                    if (len < 1e-9) // zero-length segments
                        continue;

                    double offset = (prev_segment) ? prev_segment->mOffset : 0.0;
                    int num_points = (int)((len - offset) / division_step);
                    //num_points = std::min(num_points, maxRenderedPointsPerSegment());

                    // Then render all points
                    for (int i = 0; i <= num_points; ++i)
                    {
                        if (skipping > 0)
                        {
                            --skipping;
                            continue;
                        }
                        skipping = skipping_current;
                        double cur_len = i * division_step + offset;
                        DottedLinePointInfo point;
                        point.world.point.mLatitude  = segment->mBegin.world.point.mLatitude  + (dlat / len) * cur_len;
                        point.world.point.mLongitude = segment->mBegin.world.point.mLongitude + (dlon / len) * cur_len;
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
                    prev_segment = segment;
                }
            }

            //context.setBlend(false);
            if (mHighAltitudes)
                renderer_->EnableDepthTest();
            else
                renderer_->context()->DisablePolygonOffset();

            shader_->Unbind();
        }
        void DottedLineRenderer::create()
        {
            graphics::VertexAttribute attribs[] = {
                graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3), // position
                graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 2)  // texcoord
            };
            renderer_->AddVertexFormat(vertex_format_, &attribs[0], 2);

            const float kSize = elementSize();

            const unsigned int num_vertices = 4;
            const unsigned int vertex_size = 5 * sizeof(float);

            float vertices[num_vertices][5] = {
                {-kSize, 0.0f, -kSize, 0.0f, 0.0f},
                { kSize, 0.0f, -kSize, 1.0f, 0.0f},
                {-kSize, 0.0f,  kSize, 0.0f, 1.0f},
                { kSize, 0.0f,  kSize, 1.0f, 1.0f}
            };

            const unsigned int num_indices = 4;
            const unsigned int index_size = sizeof(unsigned short);
            unsigned short indices[num_indices] = {0,1,2,3};

            // 2. Map it into video memory
            renderer_->AddVertexBuffer(vertex_buffer_, num_vertices * vertex_size, vertices, graphics::BufferUsage::kStaticDraw);
            renderer_->AddIndexBuffer(index_buffer_, num_indices, index_size, indices, graphics::BufferUsage::kStaticDraw);
        }
        void DottedLineRenderer::cropSegment(DottedLineSegment *& segment)
        {
            mgnMdWorldRect rect = GetWorldRect();
            const bool begin_in_rect = 
                segment->mBegin.world.point.mLatitude < rect.mUpperLeft.mLatitude &&
                segment->mBegin.world.point.mLatitude > rect.mLowerRight.mLatitude &&
                segment->mBegin.world.point.mLongitude > rect.mUpperLeft.mLongitude &&
                segment->mBegin.world.point.mLongitude < rect.mLowerRight.mLongitude;
            const bool end_in_rect = 
                segment->mEnd.world.point.mLatitude < rect.mUpperLeft.mLatitude &&
                segment->mEnd.world.point.mLatitude > rect.mLowerRight.mLatitude &&
                segment->mEnd.world.point.mLongitude > rect.mUpperLeft.mLongitude &&
                segment->mEnd.world.point.mLongitude < rect.mLowerRight.mLongitude;
            if (begin_in_rect && end_in_rect)
            {
                // both points are in rect, there is no need to crop
                return;
            }
            else if (!begin_in_rect && !end_in_rect)
            {
                // both points are outside, delete segment
                delete segment;
                segment = NULL;
                return;
            }
            mgnMdWorldPoint& begin = segment->mBegin.world.point; // most common situation
            mgnMdWorldPoint& end   = segment->mEnd.world.point;
            if (end_in_rect)
            {
                begin = segment->mEnd.world.point;
                end   = segment->mBegin.world.point;
            }
            mgnMdWorldPoint dir;
            dir.mLatitude = end.mLatitude - begin.mLatitude;
            dir.mLongitude = end.mLongitude - begin.mLongitude;
            double len = sqrt(dir.mLatitude * dir.mLatitude + dir.mLongitude * dir.mLongitude);
            if (len < 1e-9) return;
            dir.mLatitude  /= len; // normalize
            dir.mLongitude /= len;
            bool found = false;
            if (dir.mLongitude < 0.0) // check intersection with left edge
            {
                double lon = rect.mUpperLeft.mLongitude;
                double dist = (lon - begin.mLongitude)/dir.mLongitude;
                double lat = begin.mLatitude + dist * dir.mLatitude;
                if (lat < rect.mUpperLeft.mLatitude && lat > rect.mLowerRight.mLatitude)
                {
                    found = true;
                    end.mLatitude = lat;
                    end.mLongitude = lon;
                }
            }
            if (!found && dir.mLongitude >= 0.0) // check intersection with right edge
            {
                double lon = rect.mLowerRight.mLongitude;
                double dist = (lon - begin.mLongitude)/dir.mLongitude;
                double lat = begin.mLatitude + dist * dir.mLatitude;
                if (lat < rect.mUpperLeft.mLatitude && lat > rect.mLowerRight.mLatitude)
                {
                    found = true;
                    end.mLatitude = lat;
                    end.mLongitude = lon;
                }
            }
            if (!found && dir.mLatitude < 0.0) // check intersection with bottom edge
            {
                double lat = rect.mLowerRight.mLatitude;
                double dist = (lat - begin.mLatitude)/dir.mLatitude;
                double lon = begin.mLongitude + dist * dir.mLongitude;
                if (lon > rect.mUpperLeft.mLongitude && lon < rect.mLowerRight.mLongitude)
                {
                    found = true;
                    end.mLatitude = lat;
                    end.mLongitude = lon;
                }
            }
            if (!found && dir.mLatitude >= 0.0) // check intersection with top edge
            {
                double lat = rect.mUpperLeft.mLatitude;
                double dist = (lat - begin.mLatitude)/dir.mLatitude;
                double lon = begin.mLongitude + dist * dir.mLongitude;
                if (lon > rect.mUpperLeft.mLongitude && lon < rect.mLowerRight.mLongitude)
                {
                    found = true;
                    end.mLatitude = lat;
                    end.mLongitude = lon;
                }
            }
        }
        void DottedLineRenderer::subdivideSegment(const mgnMdWorldPoint& p1, const mgnMdWorldPoint& p2, 
                                                       std::vector<mgnMdWorldPoint>* points)
        {
            const double kDivisionStep = 1000.0; // 1 km
            double division_step = kDivisionStep / terrain_view_->getMetersPerLatitude();
            double dlat = p2.mLatitude - p1.mLatitude;
            double dlon = p2.mLongitude - p1.mLongitude;
            double len = sqrt(dlat * dlat + dlon * dlon);
            if (division_step > len) // need to subdivide segment
            {
                points->push_back(p1);
                int num_steps = (int)(len / division_step);
                for (int i = 1; i <= num_steps; ++i)
                {
                    double cur_len = i * division_step;
                    points->push_back(mgnMdWorldPoint(p1.mLatitude  + dlat * (cur_len / len),
                                                      p1.mLongitude + dlon * (cur_len / len)));
                }
                points->push_back(p2);
            }
            else
            {
                points->push_back(p1);
                points->push_back(p2);
            }
        }
        mgnMdWorldRect DottedLineRenderer::GetWorldRect() const
        {
            return terrain_view_->getWorldRect();
        }
        int DottedLineRenderer::GetMapScaleIndex() const
        {
            return 1;
        }
        float DottedLineRenderer::GetMapScaleFactor() const
        {
            return 30.0f;
        }
        mgnMdWorldPosition DottedLineRenderer::GetGPSPosition() const
        {
            return *gps_pos_;
        }
        bool DottedLineRenderer::ShouldDrawPOIs() const
        {
            return false;
        }
        bool DottedLineRenderer::ShouldDrawLabels() const
        {
            return false;
        }

    } // namespace terrain
} // namespace mgn