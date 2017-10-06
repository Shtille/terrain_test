#include "mgnTrGuidanceArrowRenderer.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnMdWorldPoint.h"
#include "mgnMdIUserDataDrawContext.h"

#include <cmath>

class ArrowPathContext : public mgnMdIUserDataDrawContext {
public:
    explicit ArrowPathContext(mgnMdTerrainView * terrain_view, const mgnMdWorldPosition * gps_pos)
    : gps_pos_(gps_pos)
    {
        world_rect_ = terrain_view->getLowDetailWorldRect();
    }
    mgnMdWorldRect GetWorldRect() const { return world_rect_; }
    int GetMapScaleIndex() const { return 1; }
    float GetMapScaleFactor() const { return 30.0f; }
    mgnMdWorldPosition GetGPSPosition() const { return *gps_pos_; }
    bool ShouldDrawPOIs() const { return false; }
    bool ShouldDrawLabels() const { return false; }

private:
    const mgnMdWorldPosition * gps_pos_;
    mgnMdWorldRect world_rect_;
};

namespace mgn {
    namespace terrain {

        GuidanceArrowRenderer::GuidanceArrowRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
        : renderer_(renderer)
        , terrain_view_(terrain_view)
        , shader_(shader)
        , gps_pos_(gps_pos)
        , position_()
        , heading_(0.0f)
        , scale_(1.0f)
        , line_width_(1.0f)
        , exists_(false)
        , need_rotation_matrix_(false)
        {
            Create();
        }
        GuidanceArrowRenderer::~GuidanceArrowRenderer()
        {
            if (vertex_format_)
                renderer_->DeleteVertexFormat(vertex_format_);
            for (int i = 0; i < 3; ++i)
            {
                if (vertex_buffers_[i])
                    renderer_->DeleteVertexBuffer(vertex_buffers_[i]);
            }
            for (int i = 0; i < 2; ++i)
            {
                if (index_buffers_[i])
                    renderer_->DeleteIndexBuffer(index_buffers_[i]);
            }
        }
        void GuidanceArrowRenderer::UpdateLineWidth(float fovx, float distance_to_camera)
        {
            const float kLineCoeff = 10.0f;
            float alpha = atan(size()/distance_to_camera);
            line_width_ = (2.0f * alpha / fovx) * kLineCoeff;
        }
        void GuidanceArrowRenderer::Update(mgnMdTerrainProvider * provider)
        {
            float cam_dist = (float)terrain_view_->getCamDistance();

            GeoGuidanceArrow arrow;
            float scale = std::max(cam_dist * 0.01f, 1.0f);
            arrow.distance = scale * 20.0f;
            ArrowPathContext context(terrain_view_, gps_pos_);
            if (provider->fetchGuidanceArrow(context, arrow) && terrain_view_->getMagIndex() < 5)
            {
                exists_ = true;
                double local_x, local_y;
                terrain_view_->WorldToLocal(mgnMdWorldPoint(arrow.point.mLatitude, arrow.point.mLongitude), 
                    local_x, local_y);
                position_.x = (float)local_x;
                position_.z = (float)local_y;
                position_.y = (float)provider->getAltitude(arrow.point.mLatitude, arrow.point.mLongitude);
                scale_ = scale;
                heading_ = arrow.heading;
                // Finally
                CalcRotation(provider);
            }
            else
            {
                exists_ = false;
            }
        }
        void GuidanceArrowRenderer::Render(boost::optional<std::pair<vec2, float> > shadow)
        {
            if (!exists_) return;

            // maneuver arrow intersection checking
            if (shadow && (position().xz() - shadow->first).Length() < size() + shadow->second)
            {
                return;
            }

            shader_->Bind();

            renderer_->PushMatrix();
            renderer_->Translate(position_);
            renderer_->Scale(scale_);
            if (need_rotation_matrix_)
            {
                renderer_->MultMatrix(rotation_matrix_);
            }
            else
            {
                float cos_h = cos(heading_);
                float sin_h = sin(heading_);
                math::Matrix4 rotation_heading(
                    cos_h, 0.0f, -sin_h, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    sin_h, 0.0f, cos_h, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f);
                renderer_->MultMatrix(rotation_heading);
            }
            shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());

            renderer_->ChangeVertexFormat(vertex_format_);

            renderer_->ClearStencil(3); // 1 & 2 arrow's extrusions
            renderer_->ClearStencilBuffer();

            renderer_->EnableStencilTest();

            renderer_->ChangeIndexBuffer(index_buffers_[0]);

            // Draw original arrow, same as in 2D mode
            renderer_->StencilBaseFunction();
            shader_->Uniform4f("u_color", 1.0f, 0.713f, 0.223f, 1.0f);
            renderer_->ChangeVertexBuffer(vertex_buffers_[2]);
            renderer_->DrawElements(graphics::PrimitiveType::kTriangleStrip);

            // Draw black extruded arrow
            renderer_->StencilHighlightFunction(1);
            shader_->Uniform4f("u_color", 0.0f, 0.0f, 0.0f, 1.0f);
            renderer_->ChangeVertexBuffer(vertex_buffers_[1]);
            renderer_->DrawElements(graphics::PrimitiveType::kTriangleStrip);

            // Draw white extruded arrow
            renderer_->StencilHighlightFunction(2);
            shader_->Uniform4f("u_color", 1.0f, 1.0f, 1.0f, 1.0f);
            renderer_->ChangeVertexBuffer(vertex_buffers_[0]);
            renderer_->DrawElements(graphics::PrimitiveType::kTriangleStrip);

            renderer_->DisableStencilTest();

            // Draw extrusion for arrow    
            renderer_->context()->EnablePolygonOffset(-1.0f, -1.0f);
            renderer_->context()->SetLineWidth(line_width_);
            shader_->Uniform4f("u_color", 0.0f, 0.0f, 0.0f, 1.0f);
            renderer_->ChangeVertexBuffer(vertex_buffers_[2]);
            renderer_->ChangeIndexBuffer(index_buffers_[1]);
            renderer_->DrawElements(graphics::PrimitiveType::kLines);
            renderer_->context()->SetLineWidth(1.0f);
            renderer_->context()->DisablePolygonOffset();

            renderer_->PopMatrix();

            shader_->Unbind();
        }
        float GuidanceArrowRenderer::size() const
        {
            return scale_ * 1.7f;
        }
        bool GuidanceArrowRenderer::exists() const
        {
            return exists_;
        }
        const vec3& GuidanceArrowRenderer::position() const
        {
            return position_;
        }
        void GuidanceArrowRenderer::Create()
        {
            graphics::VertexAttribute attribs[] = {
                graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)
            };
            renderer_->AddVertexFormat(vertex_format_, &attribs[0], 1);

            CreateArrow();
            CreateExtrusion();
        }
        void GuidanceArrowRenderer::CreateArrow()
        {
            const float kArrowHeight = 0.5f;
            const float kArrowLegLength = 2.0f;
            const float kArrowLegWidth = 1.0f;
            const float kArrowLength = 0.7f * kArrowLegLength;
            const float kArrowWidth = 1.5f * kArrowLength;
            const float kArrowHalfLength = 0.5f*(kArrowLength + kArrowLegLength);

            const float kExtrusionFirst = 0.2f;
            const float kXFirst = kExtrusionFirst * 1.732f; // * sqrt(3)
            const float kExtrusionSecond = 0.1f;
            const float kXSecond = kExtrusionSecond * 1.732f; // * sqrt(3)

            const unsigned int num_vertices = 14;
            const unsigned int vertex_size = 3 * sizeof(float);

            float vertices_first[num_vertices][3] = {
                {-0.5f*kArrowLegWidth-kExtrusionFirst,             -kExtrusionFirst, -kArrowHalfLength-kExtrusionFirst},
                { 0.5f*kArrowLegWidth+kExtrusionFirst,             -kExtrusionFirst, -kArrowHalfLength-kExtrusionFirst},
                {-0.5f*kArrowLegWidth-kExtrusionFirst, kArrowHeight+kExtrusionFirst, -kArrowHalfLength-kExtrusionFirst},
                { 0.5f*kArrowLegWidth+kExtrusionFirst, kArrowHeight+kExtrusionFirst, -kArrowHalfLength-kExtrusionFirst},

                {-0.5f*kArrowLegWidth-kExtrusionFirst,             -kExtrusionFirst, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowLegWidth+kExtrusionFirst,             -kExtrusionFirst, kArrowLegLength-kArrowHalfLength},
                {-0.5f*kArrowLegWidth-kExtrusionFirst, kArrowHeight+kExtrusionFirst, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowLegWidth+kExtrusionFirst, kArrowHeight+kExtrusionFirst, kArrowLegLength-kArrowHalfLength},

                {-0.5f*kArrowWidth-kXFirst,             -kExtrusionFirst, kArrowLegLength-kArrowHalfLength-kExtrusionFirst},
                { 0.5f*kArrowWidth+kXFirst,             -kExtrusionFirst, kArrowLegLength-kArrowHalfLength-kExtrusionFirst},
                {-0.5f*kArrowWidth-kXFirst, kArrowHeight+kExtrusionFirst, kArrowLegLength-kArrowHalfLength-kExtrusionFirst},
                { 0.5f*kArrowWidth+kXFirst, kArrowHeight+kExtrusionFirst, kArrowLegLength-kArrowHalfLength-kExtrusionFirst},

                {0.0f,             -kExtrusionFirst, kArrowHalfLength+kXFirst},
                {0.0f, kArrowHeight+kExtrusionFirst, kArrowHalfLength+kXFirst}
            };

            float vertices_second[num_vertices][3] = {
                {-0.5f*kArrowLegWidth-kExtrusionSecond,             -kExtrusionSecond, -kArrowHalfLength-kExtrusionSecond},
                { 0.5f*kArrowLegWidth+kExtrusionSecond,             -kExtrusionSecond, -kArrowHalfLength-kExtrusionSecond},
                {-0.5f*kArrowLegWidth-kExtrusionSecond, kArrowHeight+kExtrusionSecond, -kArrowHalfLength-kExtrusionSecond},
                { 0.5f*kArrowLegWidth+kExtrusionSecond, kArrowHeight+kExtrusionSecond, -kArrowHalfLength-kExtrusionSecond},

                {-0.5f*kArrowLegWidth-kExtrusionSecond,             -kExtrusionSecond, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowLegWidth+kExtrusionSecond,             -kExtrusionSecond, kArrowLegLength-kArrowHalfLength},
                {-0.5f*kArrowLegWidth-kExtrusionSecond, kArrowHeight+kExtrusionSecond, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowLegWidth+kExtrusionSecond, kArrowHeight+kExtrusionSecond, kArrowLegLength-kArrowHalfLength},

                {-0.5f*kArrowWidth-kXSecond,             -kExtrusionSecond, kArrowLegLength-kArrowHalfLength-kExtrusionSecond},
                { 0.5f*kArrowWidth+kXSecond,             -kExtrusionSecond, kArrowLegLength-kArrowHalfLength-kExtrusionSecond},
                {-0.5f*kArrowWidth-kXSecond, kArrowHeight+kExtrusionSecond, kArrowLegLength-kArrowHalfLength-kExtrusionSecond},
                { 0.5f*kArrowWidth+kXSecond, kArrowHeight+kExtrusionSecond, kArrowLegLength-kArrowHalfLength-kExtrusionSecond},

                {0.0f,             -kExtrusionSecond, kArrowHalfLength+kXSecond},
                {0.0f, kArrowHeight+kExtrusionSecond, kArrowHalfLength+kXSecond}
            };

            float vertices[num_vertices][3] = {
                {-0.5f*kArrowLegWidth,         0.0f, -kArrowHalfLength},
                { 0.5f*kArrowLegWidth,         0.0f, -kArrowHalfLength},
                {-0.5f*kArrowLegWidth, kArrowHeight, -kArrowHalfLength},
                { 0.5f*kArrowLegWidth, kArrowHeight, -kArrowHalfLength},

                {-0.5f*kArrowLegWidth,         0.0f, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowLegWidth,         0.0f, kArrowLegLength-kArrowHalfLength},
                {-0.5f*kArrowLegWidth, kArrowHeight, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowLegWidth, kArrowHeight, kArrowLegLength-kArrowHalfLength},

                {-0.5f*kArrowWidth,         0.0f, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowWidth,         0.0f, kArrowLegLength-kArrowHalfLength},
                {-0.5f*kArrowWidth, kArrowHeight, kArrowLegLength-kArrowHalfLength},
                { 0.5f*kArrowWidth, kArrowHeight, kArrowLegLength-kArrowHalfLength},

                {0.0f,         0.0f, kArrowHalfLength},
                {0.0f, kArrowHeight, kArrowHalfLength}
            };

            const unsigned int num_indices = 39;
            const unsigned int index_size = sizeof(unsigned short);
            unsigned short indices[num_indices] = {
                     2, 0, 3, 1, 7, 5, 11, 9, 13, 12, 10, 8, 6, 4, 2, 0, // 16
                0,2, 2, 3, 6, 7, // 6
                7,1, 1, 0, 5, 4, // 6
                4,9, 9, 8, 12, 12, // 6
                12,13, 13, 10, 11 // 5
            };

            // 2. Map it into video memory
            renderer_->AddVertexBuffer(vertex_buffers_[0], num_vertices * vertex_size, vertices_first, graphics::BufferUsage::kStaticDraw);
            renderer_->AddVertexBuffer(vertex_buffers_[1], num_vertices * vertex_size, vertices_second, graphics::BufferUsage::kStaticDraw);
            renderer_->AddVertexBuffer(vertex_buffers_[2], num_vertices * vertex_size, vertices, graphics::BufferUsage::kStaticDraw);
            renderer_->AddIndexBuffer(index_buffers_[0], num_indices, index_size, indices, graphics::BufferUsage::kStaticDraw);
        }
        void GuidanceArrowRenderer::CreateExtrusion()
        {
            // Vertices have been created in previous step

            const unsigned int num_indices = 42;
            const unsigned int index_size = sizeof(unsigned short);
            unsigned short indices[num_indices] = {
                0,2, 2,3, 3,1, 1,0, // 8
                0,4, 2,6, 3,7, 1,5, // 8
                4,8, 6,10, 7,11, 5,9, // 8
                4,6, 5,7, 8,10, 9,11, 12,13, // 10
                8,12, 10,13, 9,12, 11,13 // 8
            };

            // 2. Map it into video memory
            renderer_->AddIndexBuffer(index_buffers_[1], num_indices, index_size, indices, graphics::BufferUsage::kStaticDraw);
        }
        void GuidanceArrowRenderer::CalcRotation(mgnMdTerrainProvider * provider)
        {
            float dxm = ((float)terrain_view_->getMagnitude())/111111.0f;
            need_rotation_matrix_ = dxm <= 0.05f;
            if (!need_rotation_matrix_)
                return;

            // Code taken from DottedLineRenderer
            mgnMdWorldPoint world_point;
            terrain_view_->LocalToWorld(position_.x, position_.z, world_point);
            
            // Fast, but inaccurate computation
            double d = terrain_view_->getCellSizeLat()/double(GetTileHeightSamples()-1);
            double dlon = d / terrain_view_->getMetersPerLongitude();
            double dlat = d / terrain_view_->getMetersPerLatitude();
            float sx =      
                (float)provider->getAltitude(world_point.mLatitude, world_point.mLongitude + dlon, dxm) - // x+1
                (float)provider->getAltitude(world_point.mLatitude, world_point.mLongitude - dlon, dxm);  // x-1
            float sy =      
                (float)provider->getAltitude(world_point.mLatitude + dlat, world_point.mLongitude, dxm) - // y+1
                (float)provider->getAltitude(world_point.mLatitude - dlat, world_point.mLongitude, dxm);  // y-1
            // assume that tile cell sizes in both directions are the same
            // also swap x and z to convert LHS normal to RHS
            vec3 normal;
            normal.Set(-sy, 2.0f*(float)d, -sx);
            normal.Normalize();

            vec3 to_target(-sin(heading_), 0.0f, cos(heading_));
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

            // Our vectors are in right hand space rotated by 90� from original one.
            // So transform it into left hand gay space by swapping x and z.
            // But we need RHS here, so we should invert z axis.
            rotation_matrix_.sa[ 0] = dir.z;
            rotation_matrix_.sa[ 1] = dir.y;
            rotation_matrix_.sa[ 2] = dir.x;
            rotation_matrix_.sa[ 3] = 0.0f;

            rotation_matrix_.sa[ 4] = up.z;
            rotation_matrix_.sa[ 5] = up.y;
            rotation_matrix_.sa[ 6] = up.x;
            rotation_matrix_.sa[ 7] = 0.0f;

            rotation_matrix_.sa[ 8] = -side.z;
            rotation_matrix_.sa[ 9] = -side.y;
            rotation_matrix_.sa[10] = -side.x;
            rotation_matrix_.sa[11] = 0.0f;

            rotation_matrix_.sa[12] = 0.0f;
            rotation_matrix_.sa[13] = 0.0f;
            rotation_matrix_.sa[14] = 0.0f;
            rotation_matrix_.sa[15] = 1.0f;
        }

    } // namespace terrain
} // namespace mgn