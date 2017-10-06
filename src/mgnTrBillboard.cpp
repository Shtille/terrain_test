#include "mgnTrBillboard.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainView.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

namespace mgn {
    namespace terrain {

        Billboard::Billboard(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const vec3* tile_position, float scale)
        : Mesh(renderer)
        , mTerrainView(terrain_view)
        , mShader(shader)
        , mTexture(NULL)
        , mTilePosition(tile_position)
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
            const int kTileResolution = GetTileResolution();

            renderer_->ChangeTexture(mTexture);

            float tilt    = (float)mTerrainView->getCamTiltRad();
            float heading = (float)mTerrainView->getCamHeadingRad();
            float s       = (float)mTerrainView->getCellSizeLon() / kTileResolution;
            float scale   = (float)mTerrainView->getCamDistance() * mScale / s;

            vec3 world_position(mPosition + *mTilePosition);
            float icon_distance = math::Distance(world_position, mTerrainView->getCamPosition());

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
            const int kTileResolution = GetTileResolution();
            float s = (float)mTerrainView->getCellSizeLon() / kTileResolution;
            float scale   = (float)mTerrainView->getCamDistance() * mScale / s;
            size.x = mWidth * scale;
            size.y = mHeight * scale;
        }
        const vec3& Billboard::position() const
        {
            return mPosition;
        }
        const vec3& Billboard::tile_position() const
        {
            return *mTilePosition;
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