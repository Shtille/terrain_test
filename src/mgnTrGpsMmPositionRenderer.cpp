#include "mgnTrGpsMmPositionRenderer.h"

#include "mgnTrConstants.h"
#include "mgnTrMercatorProvider.h"

#include "mgnMdTerrainView.h"

#include "mgnMdWorldPoint.h"

namespace mgn {
    namespace terrain {

        GpsMmPositionRenderer::GpsMmPositionRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
            graphics::Shader * shader)
            : Mesh(renderer)
            , mTerrainView(terrain_view)
            , mShader(shader)
            , mScale(1.0f)
            , mGpsColor(1.0f, 0.0f, 0.0f)
            , mMmColor(0.0f, 0.5f, 0.0f)
            , mExists(false)
        {
            Create();
            MakeRenderable();
        }
        GpsMmPositionRenderer::~GpsMmPositionRenderer()
        {

        }
        void GpsMmPositionRenderer::Update(MercatorProvider * provider)
        {
            // TODO: do 1 fetch per second, not every frame
            mgnMdWorldPoint gps_point, mm_point;
            if (provider->FetchGpsMmPoint(&gps_point, &mm_point))
            {
                const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
                mExists = true;
                float cam_dist = (float)mTerrainView->getCamDistance();
                mScale = std::max(cam_dist * 0.01f, 1.0f);
                mTerrainView->LocalToPixelDistance(mScale, mScale, kMSM);
                double latitude, longitude, altitude;

                latitude = gps_point.mLatitude;
                longitude = gps_point.mLongitude;
                altitude = provider->GetAltitude(latitude, longitude);
                mTerrainView->WorldToPixel(latitude, longitude, altitude, mGpsPosition, kMSM);

                latitude = mm_point.mLatitude;
                longitude = mm_point.mLongitude;
                altitude = provider->GetAltitude(latitude, longitude);
                mTerrainView->WorldToPixel(latitude, longitude, altitude, mMmPosition, kMSM);
            }
            else
            {
                mExists = false;
            }
        }
        void GpsMmPositionRenderer::Render()
        {
            if (!mExists) return;

            mShader->Bind();

            renderer_->PushMatrix();
            renderer_->Translate(mGpsPosition);
            renderer_->Scale(mScale);
            mShader->UniformMatrix4fv("u_model", renderer_->model_matrix());
            mShader->Uniform3fv("u_color", mGpsColor);
            Mesh::Render();
            renderer_->PopMatrix();

            renderer_->PushMatrix();
            renderer_->Translate(mMmPosition);
            renderer_->Scale(mScale);
            mShader->UniformMatrix4fv("u_model", renderer_->model_matrix());
            mShader->Uniform3fv("u_color", mMmColor);
            Mesh::Render();
            renderer_->PopMatrix();

            mShader->Unbind();
        }
        void GpsMmPositionRenderer::Create()
        {
            // 1. Make some data
            const float kTwoPi = 6.2831853071f;
            const float kPi    = 3.1415926535f;
            const float kRadius = 1.0f;
            const int kSlices = 20; // number of vertices in sphere's longitude direction
            const int kLoops  = 10; // number of vertices in sphere's latitude direction

            // Fill vertices
            num_vertices_ = 2 + kSlices*(kLoops-2);
            unsigned int vertex_size = 6 * sizeof(float);
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            volatile int ind = 0;
            vertices[ind++] = 0.0f; // x
            vertices[ind++] = -kRadius; // y
            vertices[ind++] = 0.0f; // z
            vertices[ind++] = 0.0f; // nx
            vertices[ind++] = -1.0f; // ny
            vertices[ind++] = 0.0f; // nz
            vertices[ind++] = 0.0f; // x
            vertices[ind++] = kRadius; // y
            vertices[ind++] = 0.0f; // z
            vertices[ind++] = 0.0f; // nx
            vertices[ind++] = 1.0f; // ny
            vertices[ind++] = 0.0f; // nz
            for (int j = 1; j < kLoops-1; ++j)
            {
                float aj = kPi / (float)(kLoops-1) * (float)j;
                float sin_aj = sin(aj);
                float cos_aj = cos(aj);
                for (int i = 0; i < kSlices; ++i)
                {
                    float ai = kTwoPi / (float)kSlices * (float)i;

                    vertices[ind+3] = sin_aj * cos(ai); // nx
                    vertices[ind+4] = -cos_aj; // ny
                    vertices[ind+5] = sin_aj * sin(ai); // nz
                    vertices[ind+0] = kRadius * vertices[ind+3]; // x
                    vertices[ind+1] = kRadius * vertices[ind+4]; // y
                    vertices[ind+2] = kRadius * vertices[ind+5]; // z
                    ind += 6;
                }
            }
            // Fill indices
            num_indices_ = (2+2*kSlices)*(kLoops-1) + 2*(kLoops-2);
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);
            ind = 0;
            for (unsigned int j = 0; j < kLoops-1; ++j)
            {
                const bool lat_end = (j+2 == kLoops);
                const bool lat_beg = (j == 0);
                indices[ind++] = lat_end ? (1) : (2 + (j  )*kSlices);
                indices[ind++] = lat_beg ? (0) : (2 + (j-1)*kSlices);
                for (int i = 0; i < kSlices; ++i)
                {
                    int next_i = (i + 1 == kSlices) ? (0) : (i+1);
                    
                    indices[ind++] = lat_end ? (1) : (2 + (next_i) + (j  )*kSlices);
                    indices[ind++] = lat_beg ? (0) : (2 + (next_i) + (j-1)*kSlices);
                }
                // Degenerates
                if (!lat_end)
                {
                    if (j+3 == kLoops)
                    {
                        indices[ind] = indices[ind - 1], ++ind;
                        indices[ind++] = 1;
                    }
                    else
                    {
                        indices[ind] = indices[ind - 1], ++ind;
                        indices[ind++] = 2 + (j+1)*kSlices;
                    }
                }
            }
        }
        void GpsMmPositionRenderer::FillAttributes()
        {
            // Specify attributes
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // vertex
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // normal
        }

    } // namespace terrain
} // namespace mgn