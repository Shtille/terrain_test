#include "mgnTrSolidLineRenderer.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"
#include "mgnPolygonClipping.h"

#include "mgnTrTerrainTile.h"
#include "mgnTrTerrainMap.h"

#include <algorithm>
#include <assert.h>

namespace {
    const float kTrackWidth = 10.0f;
}

namespace mgn {
    namespace terrain {

        SolidLineSegmentRenderData::SolidLineSegmentRenderData(graphics::Renderer * renderer, SolidLineSegment * owner_segment)
            : renderer_(renderer)
            , mOwnerSegment(owner_segment)
            , vertex_buffer_(NULL)
            , index_buffer_(NULL)
        {
        }
        SolidLineSegmentRenderData::~SolidLineSegmentRenderData()
        {
            if (vertex_buffer_)
                renderer_->DeleteVertexBuffer(vertex_buffer_);
            if (index_buffer_)
                renderer_->DeleteIndexBuffer(index_buffer_);
        }
        bool SolidLineSegmentRenderData::prepare(const std::vector<vec2>& vertices_array)
        {
            // 1. Transform triangles into arrays
            const size_t num_vertices = vertices_array.size();
            const size_t vertex_size = 3 * sizeof(float);

            const size_t num_indices = num_vertices;
            const size_t index_size = sizeof(mgnI16_t);

            float * vertices = reinterpret_cast<float*>(new char[num_vertices * vertex_size]);
            if (vertices == NULL)
                return false;

            mgnI16_t * indices = new mgnI16_t[num_indices];
            if (indices == NULL)
            {
                delete[] vertices;
                return false;
            }

            TerrainTile const * tile = mOwnerSegment->getTile();
            assert(tile);

            volatile int vert_i = 0;
            volatile mgnI16_t ind_i = 0;
            for (std::vector<vec2>::const_iterator it = vertices_array.begin(); it != vertices_array.end(); ++it)
            {
                const vec2& vertex = *it;

                vertices[vert_i++] = vertex.x;
                vertices[vert_i++] = tile->getHeightFromTilePos(vertex.x, vertex.y);
                vertices[vert_i++] = vertex.y;
                indices[ind_i] = ind_i, ++ind_i;
            }

            // 2. Map it into video memory
            renderer_->AddVertexBuffer(vertex_buffer_, num_vertices * vertex_size, vertices, graphics::BufferUsage::kStaticDraw);
            renderer_->AddIndexBuffer(index_buffer_, num_indices, index_size, indices, graphics::BufferUsage::kStaticDraw);
        
            delete[] vertices;
            delete[] indices;

            return vertex_buffer_ != NULL
                && index_buffer_  != NULL;
        }
        void SolidLineSegmentRenderData::render()
        {
            renderer_->ChangeVertexBuffer(vertex_buffer_);
            renderer_->ChangeIndexBuffer(index_buffer_);
            renderer_->DrawElements(graphics::PrimitiveType::kTriangles);
        }
        //=======================================================================
        SolidLineSegment::SolidLineSegment()
            : mRenderData(NULL)
            , mTerrainTile(NULL)
            , mWidth(kTrackWidth)
            , mNeedToAlloc(false)
            , mBeenModified(false)
        {
        }
        SolidLineSegment::SolidLineSegment(const vec2& b, const vec2& e, const TerrainTile * tile, float width, bool modified)
            : mRenderData(NULL)
            , mTerrainTile(tile)
            , mBegin(b)
            , mEnd(e)
            , mNeedToAlloc(true)
            , mBeenModified(modified)
        {
            mWidth = WidthCalculation(tile, width);
        }
        SolidLineSegment::SolidLineSegment(const mgnMdWorldPoint& cur, const mgnMdWorldPoint& next, const TerrainTile * tile, float width)
            : mRenderData(NULL)
            , mTerrainTile(tile)
            , mNeedToAlloc(true)
            , mBeenModified(false)
        {
            assert(tile);
            mWidth = WidthCalculation(tile, width);
            tile->worldToTile(cur.mLatitude,
                              cur.mLongitude,
                              mBegin.x, mBegin.y);
            tile->worldToTile(next.mLatitude,
                              next.mLongitude,
                              mEnd.x, mEnd.y);
        }
        SolidLineSegment::~SolidLineSegment()
        {
            if (mRenderData) delete mRenderData;
        }
        vec2 SolidLineSegment::begin() const
        {
            return mBegin;
        }
        vec2 SolidLineSegment::end() const
        {
            return mEnd;
        }
        vec2 SolidLineSegment::center() const
        {
            return vec2(0.5f*(mBegin.x+mEnd.x),0.5f*(mBegin.y+mEnd.y));
        }
        TerrainTile const * SolidLineSegment::getTile() const
        {
            return mTerrainTile;
        }
        float SolidLineSegment::width() const
        {
            return mWidth;
        }
        bool SolidLineSegment::needToAlloc() const
        {
            return mNeedToAlloc;
        }
        bool SolidLineSegment::beenModified() const
        {
            return mBeenModified;
        }
        bool SolidLineSegment::hasData() const
        {
            return mRenderData != NULL;
        }
        float SolidLineSegment::WidthCalculation(const TerrainTile * tile, float width)
        {
            float cam_dist = (float)tile->mOwner->getTerrainView()->getCamDistance();
            float distance_gain = cam_dist * 0.010f;
            return width * 0.1f * std::max(distance_gain, 1.0f);
            // float kWidthMultiplier = (float)tile->mOwner->getTerrainView()->getCellSizeLat() / 512.0f;
            // return width * kWidthMultiplier;
        }
        void SolidLineSegment::prepare(graphics::Renderer * renderer, const std::vector<vec2>& vertices)
        {
            mNeedToAlloc = false;
            if (vertices.size() == 0) return;
            mRenderData = new SolidLineSegmentRenderData(renderer, this);
            if (!mRenderData->prepare(vertices))
            {
                // Prepare may fail due to some memory issues
                delete mRenderData;
                mRenderData = NULL;
            }
        }
        void SolidLineSegment::render(const math::Frustum& frustum)
        {
            if (mRenderData && mTerrainTile)
            {
                // Height requests are pretty heavy, so 2 points check should be enough.
                const int num_points = 2;
                vec3 points[num_points];
                points[0].y = mTerrainTile->getHeightFromTilePos(mBegin.x, mBegin.y);
                mTerrainTile->tileToLocal(mBegin.x, mBegin.y, points[0].x, points[0].z);
                points[1].y = mTerrainTile->getHeightFromTilePos(mEnd.x, mEnd.y);
                mTerrainTile->tileToLocal(mEnd.x, mEnd.y, points[1].x, points[1].z);

                if (frustum.IsPolygonIn(num_points, points))
                    mRenderData->render();
            }
        }
        //=======================================================================
        SolidLineChunk::SolidLineChunk(SolidLineRenderer * owner, TerrainTile const * tile)
            : mTile(tile)
            , mOwner(owner)
            , mIsFetched(false)
            , mIsDataReady(false)
        {
            mOwner->Attach(this);
        }
        SolidLineChunk::~SolidLineChunk()
        {
            mOwner->Detach(this);
            clear();
        }
        bool SolidLineChunk::isDataReady()
        {
            return mIsDataReady;
        }
        void SolidLineChunk::clear()
        {
            for (std::vector<Part>::iterator itp = mParts.begin(); itp != mParts.end(); ++itp)
            {
                Part& part = *itp;
                for (std::list<SolidLineSegment*>::iterator it = part.segments.begin(); it != part.segments.end(); ++it)
                {
                    SolidLineSegment * segment = *it;
                    delete segment;
                }
                part.segments.clear();
            }
            mParts.clear();
        }
        void SolidLineChunk::update()
        {
        }
        void SolidLineChunk::render(const math::Frustum& frustum)
        {
            graphics::Renderer * renderer = mOwner->renderer_;
            graphics::Shader * shader = mOwner->shader_;
            float offset = mOwner->offset_;

            renderer->ChangeVertexFormat(mOwner->vertex_format_);

            shader->Bind();
            shader->UniformMatrix4fv("u_model", renderer->model_matrix());

            // We wont use texture
            renderer->ChangeTexture(0);

            if (mOwner->mIsUsingQuads)
                renderer->DisableDepthTest();
            else
                renderer->context()->EnablePolygonOffset(offset, offset);

            for (std::vector<Part>::iterator itp = mParts.begin(); itp != mParts.end(); ++itp)
            {
                Part& part = *itp;
                shader->Uniform4f("u_color", part.color.x, part.color.y, part.color.z, 1.0f);
                for (std::list<SolidLineSegment*>::iterator it = part.segments.begin(); it != part.segments.end(); ++it)
                {
                    SolidLineSegment * segment = *it;
                    segment->render(frustum);
                }
            }

            if (mOwner->mIsUsingQuads)
                renderer->EnableDepthTest();
            else
                renderer->context()->DisablePolygonOffset();

            shader->Unbind();
        }
        void SolidLineChunk::fetchTriangles()
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const bool flat_tile = mTile->mBoundingBox.extent.y < 0.01f;
            for (FetchMap::iterator it = mFetchMap.begin(); it != mFetchMap.end(); ++it)
            {
                SolidLineSegment * segment = it->first;
                SolidLineFetchData& data = it->second;

                std::vector<vec2>&     result_vertices  = mFetchMap[segment].result_vertices;
                vec2&                  terrain_begin    = mFetchMap[segment].terrain_begin;
                float&                 tile_size_x      = mFetchMap[segment].tile_size_x;
                float&                 tile_size_y      = mFetchMap[segment].tile_size_y;
                SolidLineSegment*      prev_segment     = mFetchMap[segment].prev_segment;

                // We'll assume that trajectory's width won't change
                float hw = 0.5f * segment->width();

                // 3. Decal creation algorithm

                // 3.1. Prepare a clipping figure
                vec2 cur_line = segment->end() - segment->begin();
                if (cur_line.Sqr() < clipping::eps) continue; // segment has zero length
                vec2 side = cur_line.Side() * hw;
                vec2 clipper[4]; // clockwise winding
                clipper[0] = segment->begin() - side; // to da right from the beginning
                clipper[1] = segment->begin() + side; // to da left from the beginning
                clipper[2] = segment->end() + side; // to da left from the end
                clipper[3] = segment->end() - side; // to da right from the end

                // 3.2. Select all triangles which can be obstacled by our rectangle

                // 3.2.1. Convert the rect to AABB (actually don't know which way is better: AABB or OBB)
                vec2 rect_min(clipper[0]), rect_max(clipper[0]); // rect bounds (will be used for chunk search)
                if (prev_segment) // there is a joint
                {
                    // Adjust rect AABB by joint circle's AABB
                    vec2 circle_min(segment->begin().x - hw, segment->begin().y - hw), 
                         circle_max(segment->begin().x + hw, segment->begin().y + hw);
                    rect_min = circle_min;
                    rect_max = circle_max;
                }
                for (size_t i = 1; i < 4; ++i)
                {
                    if (clipper[i].x < rect_min.x)
                        rect_min.x = clipper[i].x;
                    if (clipper[i].y < rect_min.y)
                        rect_min.y = clipper[i].y;
                    if (clipper[i].x > rect_max.x)
                        rect_max.x = clipper[i].x;
                    if (clipper[i].y > rect_max.y)
                        rect_max.y = clipper[i].y;
                }

                // 3.2.2. Make triangles from terrain data (next code is specific to this application)
                std::vector<vec2> vertices;
                int min_ind_x = (int)floor((terrain_begin.x - rect_max.x)/tile_size_x);
                int max_ind_x = (int)floor((terrain_begin.x - rect_min.x)/tile_size_x);
                int min_ind_y = (int)floor((terrain_begin.y - rect_max.y)/tile_size_y);
                int max_ind_y = (int)floor((terrain_begin.y - rect_min.y)/tile_size_y);
                if (max_ind_x < 0 || min_ind_x > kTileHeightSamples-2 || 
                    max_ind_y < 0 || min_ind_y > kTileHeightSamples-2)
                    continue;

                // 3.2.3. Don't split clipper rectangle if tile is flat
                if (flat_tile)
                {
                    // Segment quad
                    result_vertices.reserve(6);
                    result_vertices.push_back(clipper[0]);
                    result_vertices.push_back(clipper[3]);
                    result_vertices.push_back(clipper[1]);
                    result_vertices.push_back(clipper[1]);
                    result_vertices.push_back(clipper[3]);
                    result_vertices.push_back(clipper[2]);
                }
                else // not flat - common case
                {
                    // clamp indices to be in bound
                    min_ind_x = TerrainTile::clampIndX( min_ind_x );
                    max_ind_x = TerrainTile::clampIndX( max_ind_x );
                    if (min_ind_x > max_ind_x) // this case shouldn't be
                        std::swap(min_ind_x, max_ind_x);
                    min_ind_y = TerrainTile::clampIndY( min_ind_y );
                    max_ind_y = TerrainTile::clampIndY( max_ind_y );
                    if (min_ind_y > max_ind_y) // this case shouldn't be
                        std::swap(min_ind_y, max_ind_y);
                    vertices.reserve( (max_ind_y-min_ind_y+1) * (max_ind_x-min_ind_x+1) * 6 );
                    for (int j = min_ind_y; j <= max_ind_y; ++j)
                    {
                        for (int i = min_ind_x; i <= max_ind_x; ++i)
                        {
                            vertices.push_back(vec2(terrain_begin.x - (i  ) * tile_size_x, terrain_begin.y - (j  ) * tile_size_y));
                            vertices.push_back(vec2(terrain_begin.x - (i+1) * tile_size_x, terrain_begin.y - (j  ) * tile_size_y));
                            vertices.push_back(vec2(terrain_begin.x - (i+1) * tile_size_x, terrain_begin.y - (j+1) * tile_size_y));
                            vertices.push_back(vec2(terrain_begin.x - (i  ) * tile_size_x, terrain_begin.y - (j  ) * tile_size_y));
                            vertices.push_back(vec2(terrain_begin.x - (i+1) * tile_size_x, terrain_begin.y - (j+1) * tile_size_y));
                            vertices.push_back(vec2(terrain_begin.x - (i  ) * tile_size_x, terrain_begin.y - (j+1) * tile_size_y));
                        }
                    }

                    // 3.3. Enumerate all triangles and find intersections for each side by our rect.
                    result_vertices.reserve( vertices.size() );
                    clipping::fixed_poly_t c_clipper, c_subject;
                    c_clipper.len = 4;
                    memcpy(c_clipper.v, (clipping::vec_t*)&clipper[0], sizeof(clipping::vec_t) * 4);
                    c_subject.len = 3;
                    int winding_dir = clipping::fixed_poly_winding(&c_clipper);
                    for (size_t j = 0; j < vertices.size(); j += 3)
                    {
                        memcpy(c_subject.v, &vertices[j], sizeof(clipping::vec_t) * 3);
                        clipping::fixed_poly_t res;
                        clipping::fixed_poly_clip(&c_subject, &c_clipper, winding_dir, &res);
                        for (int i = 2; i < res.len; ++i) // transform polygon into triangle fan
                        {
                            result_vertices.push_back(vec2(res.v[0  ].x, res.v[0  ].y));
                            result_vertices.push_back(vec2(res.v[i-1].x, res.v[i-1].y));
                            result_vertices.push_back(vec2(res.v[i  ].x, res.v[i  ].y));
                        }
                    }
                }

                // 3.4. Make joint figure.
                if (prev_segment) // there is a need to create a joint
                {
                    vec2 prev_line = prev_segment->end() - prev_segment->begin();
                    if (prev_line.Sqr() < clipping::eps) continue; // zero segments fix
                    vec2 prev_side = prev_line.Side() * hw;
                    vec2 p1 = segment->begin() + side; // to da left from the end
                    vec2 p2 = segment->begin() - side; // to da right from the end
                    vec2 p1_prev = prev_segment->end() + prev_side;
                    vec2 p2_prev = prev_segment->end() - prev_side;
                    vec2 first_point, second_point;
                    if ((prev_line.GetNormalized() & side.GetNormalized()) > 0.0f) // left hand filling
                    {
                        first_point = p1_prev;
                        second_point = p1;
                    }
                    else // right hand filling
                    {
                        first_point = p2;
                        second_point = p2_prev;
                    }
                    const float kDeltaAngle = 0.1f; // angle step in which vertices will be created
                    vec2 v1 = (first_point - prev_segment->end()).GetNormalized();
                    vec2 v2 = (second_point - prev_segment->end()).GetNormalized();
                    vec2 left = v1.Side();
                    float dot = std::min(std::max(v1 & v2, -1.0f), 1.0f);
                    if (1.0f - dot < clipping::eps) // polygon won't be convex with this precision
                        continue;
                    float angle = acos(dot); // angle between vectors
                    int n = (int)ceil(angle/kDeltaAngle);

                    std::vector<vec2> joint_clipper;
                    joint_clipper.reserve( n + 2 );
                    joint_clipper.push_back( prev_segment->end() );
                    joint_clipper.push_back( first_point );
                    for (int i = 0; i < n; ++i)
                    {
                        // direction is CW
                        float a = std::min((i+1) * kDeltaAngle, angle); // next point's angle
                        vec2 vc = v1 * (cos(a) * hw) - left * (sin(a) * hw);
                        joint_clipper.push_back( prev_segment->end() + vc );
                    }
                    if (flat_tile)
                    {
                        // Joint clipper is a triangle fan
                        for (int i = 0; i < n; ++i)
                        {
                            result_vertices.push_back(joint_clipper[0]);
                            result_vertices.push_back(joint_clipper[i+2]);
                            result_vertices.push_back(joint_clipper[i+1]);
                        }
                    }
                    else // not flat
                    {
                        clipping::poly_t d_clipper = {(int)joint_clipper.size(), 0, &joint_clipper[0]};
                        clipping::poly_t d_subject = {3, 0, NULL};
                        for (size_t j = 0; j < vertices.size(); j += 3)
                        {
                            d_subject.v = &vertices[j];
                            clipping::poly res = clipping::poly_clip(&d_subject, &d_clipper);
                            for (int i = 2; i < res->len; ++i) // transform polygon into triangle fan
                            {
                                result_vertices.push_back(vec2(res->v[0  ].x, res->v[0  ].y));
                                result_vertices.push_back(vec2(res->v[i-1].x, res->v[i-1].y));
                                result_vertices.push_back(vec2(res->v[i  ].x, res->v[i  ].y));
                            }
                            clipping::poly_free(res);
                        }
                    }
                }
            }
        }
        void SolidLineChunk::recreate()
        {
            if (!mParts.empty())
            {
                // Request highlight recreation
                mTile->mHighlightMessage = (int)GeoHighlight::kRemoveRoute | (int)GeoHighlight::kAddRoute;
            }
        }
        void SolidLineChunk::createSegments()
        {
            assert(mFetchMap.empty());
            for (std::vector<Part>::iterator itp = mParts.begin(); itp != mParts.end(); ++itp)
            {
                Part& part = *itp;
                SolidLineSegment * previous_segment = NULL;
                for (std::list<SolidLineSegment*>::iterator its = part.segments.begin(); its != part.segments.end(); ++its)
                {
                    SolidLineSegment * segment = *its;

                    if (! segment->needToAlloc())
                    {
                        previous_segment = segment;
                        continue;
                    }

                    // Fill track fetch data
                    SolidLineFetchData& fetch_data = mFetchMap[segment]; // insert a new segment
                    mTile->origin(fetch_data.terrain_begin.x, fetch_data.terrain_begin.y);
                    fetch_data.tile_size_x = mTile->getCellSizeX();
                    fetch_data.tile_size_y = mTile->getCellSizeY();

                    // Get the previous segment
                    fetch_data.prev_segment = previous_segment;
                    previous_segment = segment;
                } // for segment
            } // parts
        }
        void SolidLineChunk::generateData()
        {
            if (!mFetchMap.empty())
            {
                for (FetchMap::iterator it = mFetchMap.begin(); it != mFetchMap.end(); ++it)
                {
                    SolidLineSegment * segment = it->first;
                    SolidLineFetchData& data = it->second;
                    
                    segment->prepare(mOwner->renderer_, data.result_vertices);
                }
                mFetchMap.clear();
            }
        }
        //=======================================================================
        SolidLineRenderer::SolidLineRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
            : renderer_(renderer)
            , terrain_view_(terrain_view)
            , provider_(provider)
            , shader_(shader)
            , gps_pos_(gps_pos)
            , offset_(-1.0f)
            , mGpsPosition(0.0, 0.0)
            , mStoredMagIndex(0)
            , mIsUsingQuads(false)
        {
            graphics::VertexAttribute attribs[] = {
                graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)
            };
            renderer_->AddVertexFormat(vertex_format_, &attribs[0], 1);
        }
        SolidLineRenderer::~SolidLineRenderer()
        {
            if (vertex_format_)
                renderer_->DeleteVertexFormat(vertex_format_);
            // We shouldn't delete parts, each tile owning it will do it.
        }
        void SolidLineRenderer::onGpsPositionChange(double lat, double lon)
        {
            mGpsPosition.mLatitude = lat;
            mGpsPosition.mLongitude = lon;
            mGpsPosition.mHeading = 0;

            doFetch(); // fetch line data
        }
        void SolidLineRenderer::onViewChange()
        {
            // At high altitudes we wont use z test and our segments will be rendered just as quads
            mIsUsingQuads = terrain_view_->getCamTiltRad() > 1.0;
        }
        void SolidLineRenderer::recreateAll()
        {
            for (ChunkList::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
            {
                SolidLineChunk * chunk = *it;
                chunk->recreate();
            }
        }
        void SolidLineRenderer::update()
        {
        }
        void SolidLineRenderer::render(const math::Frustum& frustum)
        {
            assert(false);
            renderer_->ChangeVertexFormat(vertex_format_);

            shader_->Bind();

            if (mIsUsingQuads)
                renderer_->DisableDepthTest();
            else
                renderer_->context()->EnablePolygonOffset(-1.0f, -1.0f);

            for (ChunkList::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
            {
                SolidLineChunk * chunk = *it;
                chunk->render(frustum);
            }

            if (mIsUsingQuads)
                renderer_->EnableDepthTest();
            else
                renderer_->context()->DisablePolygonOffset();

            shader_->Unbind();
        }
        mgnMdTerrainView * SolidLineRenderer::terrain_view()
        {
            return terrain_view_;
        }
        mgnMdTerrainProvider * SolidLineRenderer::provider()
        {
            return provider_;
        }
        const mgnMdWorldPosition * SolidLineRenderer::gps_pos()
        {
            return gps_pos_;
        }
        mgnMdWorldRect SolidLineRenderer::GetWorldRect() const
        {
            return terrain_view_->getWorldRect();
        }
        int SolidLineRenderer::GetMapScaleIndex() const
        {
            return 1;
        }
        float SolidLineRenderer::GetMapScaleFactor() const
        {
            return 30.0f;
        }
        mgnMdWorldPosition SolidLineRenderer::GetGPSPosition() const
        {
            return *gps_pos_;
        }
        bool SolidLineRenderer::ShouldDrawPOIs() const
        {
            return false;
        }
        bool SolidLineRenderer::ShouldDrawLabels() const
        {
            return false;
        }
        void SolidLineRenderer::Attach(SolidLineChunk * chunk)
        {
            mChunks.push_back(chunk);
        }
        void SolidLineRenderer::Detach(SolidLineChunk * chunk)
        {
            ChunkList::iterator it = std::find(mChunks.begin(), mChunks.end(), chunk);
            if (it != mChunks.end())
                mChunks.erase(it);
        }

    } // namespace terrain
} // namespace mgn