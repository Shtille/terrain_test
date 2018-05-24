#include "mgnTrBillboard.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainView.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

namespace mgn {
    namespace terrain {

        Billboard::Billboard(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, float scale)
        : Mesh(renderer)
        , mTerrainView(terrain_view)
        , mShader(shader)
        , mTexture(NULL)
        , mScale(scale)
        , mOwnsTexture(true)
        {
        }
        Billboard::~Billboard()
        {
            if (mTexture && mOwnsTexture)
                renderer_->DeleteTexture(mTexture);
        }
        void Billboard::render()
        {
            const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
            const int lod = mTerrainView->GetLod();
            const float cell_size = static_cast<float>(1 << (GetMaxLod() - lod));

            renderer_->ChangeTexture(mTexture);

            float cam_distance;
            mTerrainView->LocalToPixelDistance((float)mTerrainView->getCamDistance(), cam_distance, kMSM);
            vec3 cam_position;
            mTerrainView->LocalToPixel(mTerrainView->getCamPosition(), cam_position, kMSM);

            float tilt    = (float)mTerrainView->getCamTiltRad();
            float heading = (float)mTerrainView->getCamHeadingRad();
            float scale   = cam_distance * mScale / cell_size;

            vec3 world_position(mPosition);
            float icon_distance = math::Distance(world_position, cam_position);

            mShader->Uniform1f("u_occlusion_distance", icon_distance);

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
        void Billboard::setTexture(graphics::Texture * texture, bool owns_texture)
        {
            mTexture = texture;
            mOwnsTexture = owns_texture;
        }
        void Billboard::GetIconSize(vec2& size)
        {
            const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
            const int lod = mTerrainView->GetLod();
            const float cell_size = static_cast<float>(1 << (GetMaxLod() - lod));
            float cam_distance;
            mTerrainView->LocalToPixelDistance((float)mTerrainView->getCamDistance(), cam_distance, kMSM);
            float scale   = cam_distance * mScale / cell_size;
            size.x = mWidth * scale;
            size.y = mHeight * scale;
        }
        const vec3& Billboard::position() const
        {
            return mPosition;
        }
        Billboard::OriginType Billboard::getOrigin() const
        {
            return mOrigin;
        }
        void Billboard::FillAttributes()
        {
            // Specify attributes
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // vertex
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 2)); // texture coordinate
        }

    } // namespace terrain
} // namespace mgn