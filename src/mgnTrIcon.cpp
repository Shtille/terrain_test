#include "mgnTrIcon.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainProvider.h"
#include "mgnMdTerrainView.h"

#include "mgnMdBitmap.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

namespace mgn {
    namespace terrain {

        Icon::Icon(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const vec3* tile_position, const PointUserObjectInfo &pui, unsigned short magIndex)
        : Billboard(renderer, terrain_view, shader, tile_position, 0.001f)
        , mID(pui.id)
        {
            Create(pui, magIndex);
            MakeRenderable();
        }
        Icon::~Icon()
        {
        }
        int Icon::getID() const
        {
            return mID;
        }
        void Icon::GetIconSize(vec2& size)
        {
            vec4 world_position(mPosition + *mTilePosition, 1.0f);
            vec4 pos_eye = renderer_->view_matrix() * world_position;

            float icon_distance = pos_eye.z;
            float base_distance = (float)mTerrainView->getCamDistance();
            float scale = icon_distance;
            if (icon_distance > base_distance)
                scale = std::max(base_distance, 0.8f * icon_distance);

            size.x = mWidth * scale;
            size.y = mHeight * scale;
        }
        void Icon::render()
        {
            renderer_->ChangeTexture(mTexture);

            float tilt    = (float)mTerrainView->getCamTiltRad();
            float heading = (float)mTerrainView->getCamHeadingRad();

            vec4 world_position(mPosition + *mTilePosition, 1.0f);
            vec4 pos_eye = renderer_->view_matrix() * world_position;

            mShader->Uniform1f("u_occlusion_distance", pos_eye.xyz().Length());

            float icon_distance = pos_eye.z;
            float base_distance = (float)mTerrainView->getCamDistance();
            // Near icons should have the same size as middle ones
            // Far icons shouldn't be less than 0.8 of the original size
            float scale = icon_distance;
            if (icon_distance > base_distance)
                scale = std::max(base_distance, 0.8f * icon_distance);

            float cos_h = cos(heading);
            float sin_h = sin(heading);
            float cos_t = cos(tilt);
            float sin_t = sin(tilt);
            math::Matrix4 rotation_heading(
                cos_h, 0.0f, -sin_h, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                sin_h, 0.0f, cos_h, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );
            math::Matrix4 rotation_tilt(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, cos_t, sin_t, 0.0f,
                0.0f, -sin_t, cos_t, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );

            renderer_->PushMatrix();

            renderer_->Translate(mPosition);
            renderer_->Scale(scale);
            renderer_->MultMatrix(rotation_heading);
            renderer_->MultMatrix(rotation_tilt);
            mShader->UniformMatrix4fv("u_model", renderer_->model_matrix());
            Mesh::Render();

            renderer_->PopMatrix();
        }
        void Icon::Create(const PointUserObjectInfo &pui, unsigned short magIndex)
        {
            const int kTileResolution = GetTileResolution();

            float metersInPixelLat = (float)mTerrainView->getCellSizeLat(magIndex)/kTileResolution;
            float metersInPixelLon = (float)mTerrainView->getCellSizeLon(magIndex)/kTileResolution;

            const math::Vector4& viewport = renderer_->viewport();
            float fovx = mTerrainView->getFovX();
            const float kScaleModifier = 1.0f;
            float size_koeff = tanf(fovx * 0.5f) * 2.0f * kScaleModifier;
            // Label size (in meters)
            const float screen_width = std::max(viewport.z, viewport.w);
            float w = size_koeff * (float)pui.bmp->mWidth / screen_width;
            float h = size_koeff * (float)pui.bmp->mHeight / screen_width;

            mOrigin = (pui.centered) ? Billboard::kBottomMiddle : Billboard::kBottomLeft;
            mWidth = w;
            mHeight = h;

            // Coordinates of the middle-bottom point of label (relatively to tile center)
            mPosition.x = metersInPixelLon*(float)pui.lon;
            mPosition.y = (float)                 pui.alt;
            mPosition.z = metersInPixelLat*(float)pui.lat;

            unsigned int vertex_size = 5 * sizeof(float);
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            num_vertices_ = 4;
            num_indices_ = 4;
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);

            volatile int ind = 0;

            // bottom-left
            {
                // position
                vertices[ind++] = pui.centered ? (-w/2.0f) : 0.0f;
                vertices[ind++] = 0.0f;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 0.0f;
                vertices[ind++] = 1.0f;//h/tex_h);
            }
            // bottom-right
            {
                // position
                vertices[ind++] = pui.centered ? (w/2.0f) : w;
                vertices[ind++] = 0.0f;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 1.0f;//w/tex_w);
                vertices[ind++] = 1.0f;//h/tex_h);
            }
            // upper-left
            {
                // position
                vertices[ind++] = pui.centered ? (-w/2.0f) : 0.0f;
                vertices[ind++] = h;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 0.0f;
                vertices[ind++] = 0.0f;
            }
            // upper-right
            {
                // position
                vertices[ind++] = pui.centered ? (w/2.0f) : w;
                vertices[ind++] = h;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 1.0f;//w/tex_w);
                vertices[ind++] = 0.0f;
            }

            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 3;
        }

    } // namespace terrain
} // namespace mgn