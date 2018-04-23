#include "mgnTrDirectionalLineRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include <assert.h>

static inline int MakeIntColorFrom565(mgnU16_t color565)
{
    int r5 = (color565 >> 11) & 0x1f;
    int g6 = (color565 >> 5 ) & 0x3f;
    int b5 = (color565      ) & 0x1f;
    // Expand to 8 bits per channel, fill LSB with MSB
    int r = r5 * 255 / 31;
    int g = g6 * 255 / 63;
    int b = b5 * 255 / 31;
    int a = 0xff;
    // Pack 4 8 bit channels into 32 bit pixel
    int bgra = (b << 16) | (g << 8) | (r) | (a << 24);
    return bgra;
}

namespace mgn {
    namespace terrain {

        DirectionalLineRenderer::DirectionalLineRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                        graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
        : DottedLineRenderer(renderer, terrain_view, provider, shader, gps_pos)
        , mColor(0xffaaaaaa)
        , mFetchedColor(0xffaaaaaa)
        {
        }
        DirectionalLineRenderer::~DirectionalLineRenderer()
        {
            for (std::list<DottedLineSegment*>::iterator it = mCroppedSegments.begin();
                it != mCroppedSegments.end(); ++it)
                if (*it)
                    delete *it;
        }
        void DirectionalLineRenderer::onViewChange()
        {
            DottedLineRenderer::onViewChange();

            // We need to update all cropped segments
            for (std::list<DottedLineSegment*>::iterator it = mCroppedSegments.begin(); it != mCroppedSegments.end(); ++it)
            {
                DottedLineSegment * cropped_segment = *it;

                if (cropped_segment)
                    cropped_segment->mNeedToCalculate = true;
            }
        }
        void DirectionalLineRenderer::frustumCropSegment(const math::Frustum& frustum, DottedLineSegment * const & in, DottedLineSegment *& out)
        {
            if (in == NULL) return; // segment doesn't exist
            if (out)
            {
                delete out;
                out = NULL;
            }

            double local_x, local_y;
            terrain_view_->WorldToLocal(in->mBegin.world.point, local_x, local_y);
            vec3 offset((float)local_x, 0.0f, (float)local_y);
            // Transform points to world coords
            vec3 begin = in->mBegin.local.position + offset;
            vec3 end   = in->mEnd.local.position + offset;
            vec3 v = end - begin;
            float len = v.Length();
            if (len < 0.001f) // zero length segment
                return;

            vec3 intersections[2];
            int num_intersections = frustum.IntersectionsWithSegment(math::Segment(begin, end), intersections);
            if (num_intersections == 2) // segment is large and there are 2 points of intersection
            {
                vec3 & int_begin = intersections[0];
                vec3 & int_end = intersections[1];
                if (((int_end - int_begin) & v) < 0.0f) // have opposite directions
                {
                    std::swap(int_begin, int_end);
                }
                mgnMdWorldPoint p_begin, p_end;
                terrain_view_->LocalToWorld((double)int_begin.x, (double)int_begin.z, p_begin);
                terrain_view_->LocalToWorld((double)int_end.x, (double)int_end.z, p_end);
                out = new DottedLineSegment(p_begin, p_end);
                onAddSegment(out);
            }
            else if (num_intersections == 1) // one of points is inside frustum
            {
                if (frustum.IsPointIn(begin)) // begin is inside frustum
                {
                    vec3 & int_end = intersections[0];
                    mgnMdWorldPoint p_end;
                    terrain_view_->LocalToWorld((double)int_end.x, (double)int_end.z, p_end);
                    out = new DottedLineSegment(in->mBegin.world.point, p_end);
                    onAddSegment(out);
                }
                else // end is inside frustum
                {
                    vec3 & int_begin = intersections[0];
                    mgnMdWorldPoint p_begin;
                    terrain_view_->LocalToWorld((double)int_begin.x, (double)int_begin.z, p_begin);
                    out = new DottedLineSegment(p_begin, in->mEnd.world.point);
                    onAddSegment(out);
                }
            }
            else // num_intersections == 0
            {
                // Check is all its points in frustum
                if (frustum.IsPointIn(begin) && frustum.IsPointIn(end))
                {
                    out = new DottedLineSegment(in->mBegin.world.point, in->mEnd.world.point);
                    onAddSegment(out);
                }
                else
                {
                    // segment won't even be visible, so nothing to do
                }
            }
        }
        void DirectionalLineRenderer::frustumCropSegment2(const math::Frustum& frustum, DottedLineSegment* const & in, DottedLineSegment*& out)
        {
            if (in == NULL) return; // segment doesn't exist
            if (out)
            {
                delete out;
                out = NULL;
            }

            double local_x, local_y;
            terrain_view_->WorldToLocal(in->mBegin.world.point, local_x, local_y);
            vec3 offset((float)local_x, 0.0f, (float)local_y);
            // Transform points to world coords
            vec3 begin = in->mBegin.local.position + offset;
            vec3 end   = in->mEnd.local.position + offset;
            if (in->mEnd.local.position.Length() < 0.001f) // zero length segment (end local position equals to length)
                return;

            math::VerticalProfile profile(begin, end, assumedMinAltitude(), assumedMaxAltitude());
            math::Segment segments[6];
            int num_segments = frustum.IntersectionsWithProfile(profile, segments);
            if (num_segments == 0)
                return;
            else
            {
                // Merge all segments into one segment
                vec3 dir(in->mEnd.local.position.x, 0.0f, in->mEnd.local.position.z);
                float segment_length = sqrt(dir.x * dir.x + dir.z * dir.z); // begin is always (0,0) in local coords
                dir /= segment_length;

                float min_distance;
                float max_distance;
                for (int i = 0; i < num_segments; ++i)
                {
                    float distance;
                    distance = (segments[i].begin.x - begin.x)*dir.x + (segments[i].begin.z - begin.z)*dir.z;
                    if (i == 0 || distance < min_distance)
                        min_distance = distance;
                    if (i == 0 || distance > max_distance)
                        max_distance = distance;
                    distance = (segments[i].end.x - begin.x)*dir.x + (segments[i].end.z - begin.z)*dir.z;
                    if (distance < min_distance)
                        min_distance = distance;
                    if (distance > max_distance)
                        max_distance = distance;
                }

                // !!! We are only interest in points within our segment
                if (max_distance < 0.0f || min_distance > segment_length)
                    return;
                if (min_distance < 0.0f)
                    min_distance = 0.0f;
                if (max_distance > segment_length)
                    max_distance = segment_length;

                const vec3& view_dir = frustum.getDir();
                math::Segment result_segment;
                if (view_dir.x * dir.x + view_dir.z * dir.z > 0.0f) // same direction
                {
                    result_segment.begin.x = begin.x + min_distance * dir.x;
                    result_segment.begin.z = begin.z + min_distance * dir.z;
                    result_segment.end.x   = begin.x + max_distance * dir.x;
                    result_segment.end.z   = begin.z + max_distance * dir.z;
                }
                else // oppositely directed
                {
                    result_segment.begin.x = begin.x + max_distance * dir.x;
                    result_segment.begin.z = begin.z + max_distance * dir.z;
                    result_segment.end.x   = begin.x + min_distance * dir.x;
                    result_segment.end.z   = begin.z + min_distance * dir.z;
                }
                mgnMdWorldPoint p_begin, p_end;
                terrain_view_->LocalToWorld((double)result_segment.begin.x, (double)result_segment.begin.z, p_begin);
                terrain_view_->LocalToWorld((double)result_segment.end.x, (double)result_segment.end.z, p_end);
                out = new DottedLineSegment(p_begin, p_end);
                onAddSegment(out);
            }
        }
        void DirectionalLineRenderer::update(const math::Frustum& frustum)
        {
            DottedLineRenderer::update(frustum);

            assert(mSegments.size() == mCroppedSegments.size());
            // Crop all segments by frustum
            for (std::list<DottedLineSegment*>::iterator it1 = mSegments.begin(), it2 = mCroppedSegments.begin();
                it1 != mSegments.end(); // mCropSegments.size() == mSegments.size()
                ++it1, ++it2)
            {
                DottedLineSegment * const &  segment = *it1;
                DottedLineSegment *& cropped_segment = *it2;

                // Just to not recalc each update
                const bool need_to_recalc = (cropped_segment == NULL || cropped_segment->mNeedToCalculate);
                if (need_to_recalc)
                {
                    frustumCropSegment2(frustum, segment, cropped_segment);
                    if (cropped_segment)
                        cropped_segment->mNeedToCalculate = false;
                }
            }
        }
        void DirectionalLineRenderer::render(const math::Frustum& frustum)
        {
            if (!texture_ || mColor != mFetchedColor)
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

            // We will calculate each point every frame.
            // Pretty expensive, but in dynamics, it seems the only solution.
            double point_step = (double)scale * 2.0 / terrain_view_->getMetersPerLatitude();

            for (std::list<DottedLineSegment*>::iterator it = mCroppedSegments.begin();
                    it != mCroppedSegments.end(); ++it)
            {
                DottedLineSegment * segment = *it;
                if (segment == NULL) continue;

                double local_x, local_y;
                terrain_view_->WorldToLocal(segment->mBegin.world.point, local_x, local_y);
                float offset_x = (float)local_x;
                float offset_y = (float)local_y;

                // Skip segments that are not visible
                const int kNumPoints = 4;
                vec3 points[kNumPoints]; // in global CS
                points[0].x = offset_x;
                points[0].y = assumedMinAltitude();
                points[0].z = offset_y;
                points[1].x = offset_x + segment->mEnd.local.position.x;
                points[1].y = assumedMinAltitude();
                points[1].z = offset_y + segment->mEnd.local.position.z;
                points[2].x = offset_x;
                points[2].y = assumedMaxAltitude();
                points[2].z = offset_y;
                points[3].x = offset_x + segment->mEnd.local.position.x;
                points[3].y = assumedMaxAltitude();
                points[3].z = offset_y + segment->mEnd.local.position.z;

                if (!frustum.IsPolygonIn(kNumPoints, points))
                {
                    continue;
                }

                // Segment is visible, so we can continue calculation.
                double dlat = segment->mEnd.world.point.mLatitude - segment->mBegin.world.point.mLatitude;
                double dlon = segment->mEnd.world.point.mLongitude - segment->mBegin.world.point.mLongitude;
                double len = sqrt(dlat*dlat + dlon*dlon);
                if (len < 1e-9) // zero-length segments
                    continue;
                int num_points = (int)(len / point_step);
                num_points = std::min(num_points, maxRenderedPointsPerSegment());

                // Don't forget to transform to_traget to RHS
                // mBegin has (0,0) coords, so we can optimize computation (also ignore y direction)
                const vec3 to_target(segment->mEnd.local.position.z, 0.0f, segment->mEnd.local.position.x);

                // Then render all points
                for (int i = 0; i <= num_points; ++i)
                {
                    double cur_len = i * point_step;
                    DottedLinePointInfo point;
                    point.world.point.mLatitude  = segment->mBegin.world.point.mLatitude  + dlat*(cur_len/len);
                    point.world.point.mLongitude = segment->mBegin.world.point.mLongitude + dlon*(cur_len/len);
                    if (mHighAltitudes)
                    {
                        terrain_view_->WorldToLocal(point.world.point, local_x, local_y);
                        point.local.position.x = (float)local_x - offset_x;
                        point.local.position.z = (float)local_y - offset_y;
                        point.local.position.y = segment->mMinHeight;
                        calcFlatNormal(point, to_target);
                    }
                    else
                    {
                        point.obtain_position(terrain_view_, provider_, offset_x, offset_y, 
                            segment->mMinHeight, segment->mMaxHeight);
                        calcNormal(point, to_target);
                    }

                    renderer_->PushMatrix();
                    renderer_->Translate(point.local.position.x + offset_x, 
                                         point.local.position.y, 
                                         point.local.position.z + offset_y);
                    renderer_->Scale(scale);
                    renderer_->MultMatrix(point.rotation.matrix);
                    shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
                    renderer_->DrawElements(graphics::PrimitiveType::kTriangleStrip);
                    renderer_->PopMatrix();
                }
            }

            //context.setBlend(false);
            if (mHighAltitudes)
                renderer_->EnableDepthTest();
            else
                renderer_->context()->DisablePolygonOffset();

            shader_->Unbind();
        }
        void DirectionalLineRenderer::createTexture()
        {
            if (texture_)
                renderer_->DeleteTexture(texture_);
            mColor = mFetchedColor;
            unsigned char* data = reinterpret_cast<unsigned char*>(&mColor);
            renderer_->CreateTextureFromData(texture_, 1, 1,
                graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, data);
        }
        void DirectionalLineRenderer::doFetch()
        {
            GeoOffroad offroad;
            provider_->fetchOffroad(*this, offroad);

            if (!offroad.exist())
            {
                // offroad is no longer exists
                if (!mSegments.empty())
                {
                    for (std::list<DottedLineSegment*>::iterator it = mSegments.begin(); 
                        it != mSegments.end(); ++it)
                        if (*it)
                            delete *it;
                    mSegments.clear();
                    for (std::list<DottedLineSegment*>::iterator it = mCroppedSegments.begin();
                        it != mCroppedSegments.end(); ++it)
                        if (*it)
                            delete *it;
                    mCroppedSegments.clear();
                }
                return;
            }

            const bool position_has_changed = fabs(mGpsPosition.mLatitude  - mOldPosition.mLatitude ) > 1e-9 ||
                                              fabs(mGpsPosition.mLongitude - mOldPosition.mLongitude) > 1e-9;
            if (position_has_changed)
            {
                if (!mSegments.empty())
                {
                    DottedLineSegment *& start_line = mSegments.front();
                    DottedLineSegment *& dest_line = mSegments.back();

                    if (start_line)
                    {
                        if (offroad.phase == 1)
                        {
                            start_line->mBegin.world.point.mLatitude = mGpsPosition.mLatitude;
                            start_line->mBegin.world.point.mLongitude = mGpsPosition.mLongitude;
                            // Fix end point of segment and then crop it
                            start_line->mEnd.world.point.mLatitude = offroad.start_line.end.mLatitude;
                            start_line->mEnd.world.point.mLongitude = offroad.start_line.end.mLongitude;
                            onAddSegment(start_line);

                            DottedLineSegment *& cropped_start_line = mCroppedSegments.front();
                            if (cropped_start_line)
                                cropped_start_line->mNeedToCalculate = true;
                        }
                        else
                        {
                            // need unload start line
                            delete start_line;
                            start_line = NULL;
                            DottedLineSegment *& cropped_start = mCroppedSegments.front();
                            delete cropped_start;
                            cropped_start = NULL;
                        }
                    }
                    if (dest_line)
                    {
                        if (offroad.phase == 3)
                        {
                            dest_line->mBegin.world.point.mLatitude = mGpsPosition.mLatitude;
                            dest_line->mBegin.world.point.mLongitude = mGpsPosition.mLongitude;
                            // Fix end point of segment and then crop it
                            dest_line->mEnd.world.point.mLatitude = offroad.dest_line.end.mLatitude;
                            dest_line->mEnd.world.point.mLongitude = offroad.dest_line.end.mLongitude;
                            onAddSegment(dest_line);

                            DottedLineSegment *& cropped_dest_line = mCroppedSegments.back();
                            if (cropped_dest_line)
                                cropped_dest_line->mNeedToCalculate = true;
                        }
                    }
                }
            }

            const bool need_load_segments = mSegments.empty();
            if (need_load_segments)
            {
                // Load start line
                if (offroad.start_line.empty)
                {
                    mSegments.push_back(NULL);
                    mCroppedSegments.push_back(NULL);
                }
                else // start line exists
                {
                    mFetchedColor = MakeIntColorFrom565(offroad.start_line.color);

                    DottedLineSegment * segment = new DottedLineSegment(
                        offroad.start_line.begin, offroad.start_line.end);
                    onAddSegment(segment);
                    mSegments.push_back(segment);

                    DottedLineSegment * cropped_segment = new DottedLineSegment(mgnMdWorldPoint(),mgnMdWorldPoint());
                    if (cropped_segment)
                        cropped_segment->mNeedToCalculate = true;
                    mCroppedSegments.push_back(cropped_segment);
                }
                // Load dest line
                if (offroad.dest_line.empty)
                {
                    mSegments.push_back(NULL);
                    mCroppedSegments.push_back(NULL);
                }
                else // dest line exists
                {
                    mFetchedColor = MakeIntColorFrom565(offroad.dest_line.color);

                    DottedLineSegment * segment = new DottedLineSegment(
                        offroad.dest_line.begin, offroad.dest_line.end);
                    onAddSegment(segment);
                    mSegments.push_back(segment);

                    DottedLineSegment * cropped_segment = new DottedLineSegment(mgnMdWorldPoint(),mgnMdWorldPoint());
                    if (cropped_segment)
                        cropped_segment->mNeedToCalculate = true;
                    mCroppedSegments.push_back(cropped_segment);
                }
            }
        }

    } // namespace terrain
} // namespace mgn