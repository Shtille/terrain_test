#include "mgnTrVehicleRenderer.h"

#include "mgnMdTerrainView.h"

namespace mgn {
    namespace terrain {

        VehicleRenderer::VehicleRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
            graphics::Shader * shader)
            : Mesh(renderer)
            , mTerrainView(terrain_view)
            , mShader(shader)
            , mTexture(NULL)
            , mGpsPosition(0,0)
            , mColor1(0xff9c6510)
            , mColor2(0xffe0c048)
        {
            Create();
            MakeRenderable();
        }
        VehicleRenderer::~VehicleRenderer()
        {
            if (mTexture)
                renderer_->DeleteTexture(mTexture);
        }
        mgnMdWorldPosition VehicleRenderer::GetPos() const
        {
            return mGpsPosition;
        }
        const mgnMdWorldPosition * VehicleRenderer::GetPosPtr() const
        {
            return &mGpsPosition;
        }
        vec3 VehicleRenderer::GetCenter() const
        {
            return mPosition;
        }
        float VehicleRenderer::GetSize() const
        {
            return 0.897f * mScale;
        }
        void VehicleRenderer::Update()
        {
            double vehicle_x, vehicle_y;
            mgnMdWorldPoint world_point(mGpsPosition.mLatitude, mGpsPosition.mLongitude);

            mTerrainView->WorldToLocal(world_point, vehicle_x, vehicle_y);

            mScale = (float)std::max(mTerrainView->getCamDistance() * 0.08, 1.0);
            float delta_h = 0.2f * mScale;
            if (delta_h < 1.0f) delta_h = 0.0f;
            mPosition.x = static_cast<float>(vehicle_x);
            mPosition.y = static_cast<float>(mAltitude) + delta_h;
            mPosition.z = static_cast<float>(vehicle_y);
        }
        void VehicleRenderer::Render()
        {
            if (!mTexture)
                CreateTexture();

            float dxm = ((float)mTerrainView->getMagnitude())/111111.f;

            renderer_->ChangeTexture(mTexture);

            mShader->Bind();

            float cos_h = cos(mHeading);
            float sin_h = sin(mHeading);
            float cos_t = cos(mTilt);
            float sin_t = sin(mTilt);
            math::Matrix4 rotation_heading(
                cos_h, 0.0f, -sin_h, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                sin_h, 0.0f, cos_h, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );
            math::Matrix4 rotation_tilt(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, cos_t, -sin_t, 0.0f,
                0.0f, sin_t, cos_t, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
                );

            renderer_->PushMatrix();
            renderer_->Translate(mPosition.x, dxm>0.05f ? 0.2f : mPosition.y, mPosition.z);
            renderer_->MultMatrix(rotation_heading);
            if (dxm <= 0.05)
                renderer_->MultMatrix(rotation_tilt);
            renderer_->Scale(mScale);
            mShader->UniformMatrix4fv("u_model", renderer_->model_matrix());
            Mesh::Render();
            renderer_->PopMatrix();

            mShader->Unbind();

            renderer_->ChangeTexture(NULL);
        }
        void VehicleRenderer::UpdateLocationIndicator(double lat, double lon, double altitude,
            double heading, double tilt, int color1, int color2)
        {
            const double kRadToDeg = 57.295779513;
            mGpsPosition.mLatitude = lat;
            mGpsPosition.mLongitude = lon;
            mGpsPosition.mHeading = static_cast<short>(heading * kRadToDeg);

            mAltitude = altitude;
            mHeading  = (float)heading;
            mTilt     = (float)tilt;

            if ((mColor1 != color1 || mColor2 != color2) && mTexture)
            {
                mColor1 = color1;
                mColor2 = color2;
                renderer_->DeleteTexture(mTexture);
                mTexture = NULL;
            }
            Update();
        }
        void VehicleRenderer::Create()
        {
            const int NUM_BANDS = 9;
            const float size_x = 0.666f;
            const float size_y = 0.333f;
            const float size_z = 1.500f;
            const float offset_z = -0.603f;
            const float step = size_x / NUM_BANDS;

            unsigned int vertex_size = 6*sizeof(float);
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            num_vertices_ = 8*NUM_BANDS + 9; // [2*NB+3, 2*NB+1] light, 1, [2*NB+3, 2*NB+1] dark
            num_indices_ = 8*NUM_BANDS + 13; // 2*(2*NB+1)+2, 2, 2*(2*NB+3), 1
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);

            // Color 1 is dark
            const float g1 = 0.5f;
            // Color 2 is light
            const float g2 = 1.0f;

            // Fill vertices
            const int kDarkOffset  = 6*(4*NUM_BANDS + 5);
            const int kLeftPoint   = 0;
            const int kRightPoint  = 6*(2*NUM_BANDS + 2);
            const int kTopPoint    = 6*(NUM_BANDS + 1);
            const int kBottomPoint = 6*(3*NUM_BANDS + 3);
            const int kFrontPoint  = 6*(4*NUM_BANDS + 4);
            // Left point (light)
            vertices[kLeftPoint+ 0] = -size_x; // x
            vertices[kLeftPoint+ 1] = 0.0f; // y
            vertices[kLeftPoint+ 2] = offset_z; // z
            vertices[kLeftPoint+ 3] = g2; // shading
            vertices[kLeftPoint+ 4] = 0.0f; // tex u
            vertices[kLeftPoint+ 5] = 0.0f; // tex v
            // Left point (dark)
            vertices[kDarkOffset+kLeftPoint+ 0] = -size_x; // x
            vertices[kDarkOffset+kLeftPoint+ 1] = 0.0f; // y
            vertices[kDarkOffset+kLeftPoint+ 2] = offset_z; // z
            vertices[kDarkOffset+kLeftPoint+ 3] = g1; // shading
            vertices[kDarkOffset+kLeftPoint+ 4] = 0.0f; // tex u
            vertices[kDarkOffset+kLeftPoint+ 5] = 0.0f; // tex v
            // Right point (light)
            vertices[kRightPoint+0] = size_x; // x
            vertices[kRightPoint+1] = 0.0f; // y
            vertices[kRightPoint+2] = offset_z; // z
            vertices[kRightPoint+3] = g2; // shading
            vertices[kRightPoint+4] = 0.0f; // tex u
            vertices[kRightPoint+5] = 0.0f; // tex v
            // Right point (dark)
            vertices[kDarkOffset+kRightPoint+0] = size_x; // x
            vertices[kDarkOffset+kRightPoint+1] = 0.0f; // y
            vertices[kDarkOffset+kRightPoint+2] = offset_z; // z
            vertices[kDarkOffset+kRightPoint+3] = g1; // shading
            vertices[kDarkOffset+kRightPoint+4] = 0.0f; // tex u
            vertices[kDarkOffset+kRightPoint+5] = 0.0f; // tex v
            float x1 = -size_x;
            for(int i = 1; i < NUM_BANDS + 1; ++i)
            {
                float alpha = 1.0f + x1/size_x; // x1 < 0
                x1 += step;

                // Left point (light)
                int iL = kLeftPoint + 6*i;
                vertices[iL + 0] = x1; // x
                vertices[iL + 1] = size_y*sqrt(x1/size_x+1.0f); // y <--
                vertices[iL + 2] = offset_z; // z
                vertices[iL + 3] = g2; // shading
                vertices[iL + 4] = alpha; // tex u
                vertices[iL + 5] = 0.0f; // tex v

                // Left point (dark)
                int iD = kDarkOffset+kLeftPoint + 6*i;
                vertices[iD + 0] = vertices[iL + 0]; // x
                vertices[iD + 1] = vertices[iL + 1]; // y
                vertices[iD + 2] = vertices[iL + 2]; // z
                vertices[iD + 3] = g1; // shading
                vertices[iD + 4] = vertices[iL + 4]; // tex u
                vertices[iD + 5] = vertices[iL + 5]; // tex v

                // Right point (light)
                int jL = kRightPoint - 6*i;
                vertices[jL + 0] = -x1; // x
                vertices[jL + 1] = size_y*sqrt(x1/size_x+1.0f); // y <--
                vertices[jL + 2] = offset_z; // z
                vertices[jL + 3] = g2; // shading
                vertices[jL + 4] = alpha; // tex u
                vertices[jL + 5] = 0.0f; // tex v

                // Right point (dark)
                int jD = kDarkOffset+kRightPoint - 6*i;
                vertices[jD + 0] = vertices[jL + 0]; // x
                vertices[jD + 1] = vertices[jL + 1]; // y
                vertices[jD + 2] = vertices[jL + 2]; // z
                vertices[jD + 3] = g1; // shading
                vertices[jD + 4] = vertices[jL + 4]; // tex u
                vertices[jD + 5] = vertices[jL + 5]; // tex v

                // Make the same at opposite side (to do not do another cycle)

                // Left point opposite (light)
                int ioL = kRightPoint + 6*i;
                vertices[ioL + 0] = x1; // x
                vertices[ioL + 1] = 0.0f; // y
                vertices[ioL + 2] = offset_z; // z
                vertices[ioL + 3] = g2; // shading
                vertices[ioL + 4] = alpha; // tex u
                vertices[ioL + 5] = 0.0f; // tex v

                // Left point opposite (dark)
                int ioD = kDarkOffset+kRightPoint + 6*i;
                vertices[ioD + 0] = vertices[ioL + 0]; // x
                vertices[ioD + 1] = vertices[ioL + 1]; // y
                vertices[ioD + 2] = vertices[ioL + 2]; // z
                vertices[ioD + 3] = g1; // shading
                vertices[ioD + 4] = vertices[ioL + 4]; // tex u
                vertices[ioD + 5] = vertices[ioL + 5]; // tex v

                // Right point opposite (light)
                int joL = kFrontPoint - 6*i;
                vertices[joL + 0] = -x1; // x
                vertices[joL + 1] = 0.0f; // y
                vertices[joL + 2] = offset_z; // z
                vertices[joL + 3] = g2; // shading
                vertices[joL + 4] = alpha; // tex u
                vertices[joL + 5] = 0.0f; // tex v

                // Right point opposite (dark)
                int joD = kDarkOffset+kFrontPoint - 6*i;
                vertices[joD + 0] = vertices[joL + 0]; // x
                vertices[joD + 1] = vertices[joL + 1]; // y
                vertices[joD + 2] = vertices[joL + 2]; // z
                vertices[joD + 3] = g1; // shading
                vertices[joD + 4] = vertices[joL + 4]; // tex u
                vertices[joD + 5] = vertices[joL + 5]; // tex v
            }
            // Top point (light)
            vertices[kTopPoint + 0] = 0.0f; // x
            vertices[kTopPoint + 1] = size_y; // y
            vertices[kTopPoint + 2] = offset_z; // z
            vertices[kTopPoint + 3] = g2; // shading
            vertices[kTopPoint + 4] = 1.0f; // tex u
            vertices[kTopPoint + 5] = 0.0f; // tex v
            // Top point (dark)
            vertices[kDarkOffset+kTopPoint + 0] = 0.0f; // x
            vertices[kDarkOffset+kTopPoint + 1] = size_y; // y
            vertices[kDarkOffset+kTopPoint + 2] = offset_z; // z
            vertices[kDarkOffset+kTopPoint + 3] = g1; // shading
            vertices[kDarkOffset+kTopPoint + 4] = 1.0f; // tex u
            vertices[kDarkOffset+kTopPoint + 5] = 0.0f; // tex v
            // Origin point (light)
            vertices[kBottomPoint + 0] = 0.0f; // x
            vertices[kBottomPoint + 1] = 0.0f; // y
            vertices[kBottomPoint + 2] = offset_z; // z
            vertices[kBottomPoint + 3] = g2; // shading
            vertices[kBottomPoint + 4] = 1.0f; // tex u
            vertices[kBottomPoint + 5] = 0.0f; // tex v
            // Origin point (dark)
            vertices[kDarkOffset+kBottomPoint + 0] = 0.0f; // x
            vertices[kDarkOffset+kBottomPoint + 1] = 0.0f; // y
            vertices[kDarkOffset+kBottomPoint + 2] = offset_z; // z
            vertices[kDarkOffset+kBottomPoint + 3] = g1; // shading
            vertices[kDarkOffset+kBottomPoint + 4] = 1.0f; // tex u
            vertices[kDarkOffset+kBottomPoint + 5] = 0.0f; // tex v
            // Front point (light, has no dark version (lol))
            vertices[kFrontPoint + 0] = 0.0f; // x
            vertices[kFrontPoint + 1] = 0.0f; // y
            vertices[kFrontPoint + 2] = size_z + offset_z; // z
            vertices[kFrontPoint + 3] = g2; // shading
            vertices[kFrontPoint + 4] = 1.0f; // tex u
            vertices[kFrontPoint + 5] = 1.0f; // tex v

            // Fill indices
            volatile int ind = 0;

            // Back side
            indices[ind++] = kDarkOffset/6; // 4*N+5
            for(unsigned int i = kDarkOffset/6 + 1; i < kDarkOffset/6 + 2*NUM_BANDS+2; ++i) // 2N+1
            {
                indices[ind++] = 2*NUM_BANDS+2 + i;
                indices[ind++] = i;
            }
            indices[ind++] = kDarkOffset/6 + 2*NUM_BANDS+2;

            // Degenerates
            indices[ind] = indices[ind-1]; ++ind;
            indices[ind++] = 4*NUM_BANDS+4;

            // Top sides
            for(unsigned int i = 0; i <= 2*NUM_BANDS+2; ++i) // 2N+3
            {
                indices[ind++] = 4*NUM_BANDS+4;
                indices[ind++] = i;
            }

            // Bottom side
            indices[ind++] = 0;
        }
        void VehicleRenderer::FillAttributes()
        {
            // Specify attributes
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // vertex
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 1)); // color modifier
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 2)); // texture coordinate
        }
        static int InterpolateColor(int c1, int c2, float a)
        {
            int b1 = (c1 & 0xff0000) >> 16;
            int g1 = (c1 & 0x00ff00) >> 8;
            int r1 = (c1 & 0x0000ff);
            int b2 = (c2 & 0xff0000) >> 16;
            int g2 = (c2 & 0x00ff00) >> 8;
            int r2 = (c2 & 0x0000ff);
            int r = r1 + (int)((r2-r1)*a);
            int g = g1 + (int)((g2-g1)*a);
            int b = b1 + (int)((b2-b1)*a);
            return (0xff << 24) | (b << 16) | (g << 8) | r;
        }
        void VehicleRenderer::CreateTexture()
        {
            const int w = 64;
            const int h = 64;

            unsigned char* data = new unsigned char[w * h * sizeof(int)];
            int *pixels = reinterpret_cast<int*>(data);

            for(int y = 0; y < h; ++y)
            {
                float v = float(y)/float(h-1);
                for(int x = 0; x < w; ++x)
                {
                    float alpha = 0.0f;
                    float u = float(x)/float(w-1);
                    if (x > y)
                        alpha = (u-v)/(1.0f-v);
                    pixels[y*w+x] = InterpolateColor(mColor1, mColor2, alpha);
                }
            }
            renderer_->CreateTextureFromData(mTexture, w, h,
                graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, data);
            delete[] data;
        }

    } // namespace terrain
} // namespace mgn