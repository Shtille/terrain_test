#include "mgnTrRoutePointRenderer.h"

#include "Frustum.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

#include <cmath>

namespace mgn {
    namespace terrain {

        RoutePointRenderer::RoutePointRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
            graphics::Shader * shader)
            : Mesh(renderer)
            , mTerrainView(terrain_view)
            , mScale(1.0f)
            , mExists(false)
            , mShader(shader)
        {
            Create();
            MakeRenderable();
        }
        RoutePointRenderer::~RoutePointRenderer()
        {

        }
        void RoutePointRenderer::Render(const math::Frustum& frustum, const vec3& vehicle_pos, float vehicle_size)
        {
            if (!mExists) return;

            mShader->Bind();

            for (std::vector<vec3>::const_iterator it = mPositions.begin(); it != mPositions.end(); ++it)
            {
                const vec3& position = *it;

                if (math::DistanceLess(position, vehicle_pos, mScale + vehicle_size))
                {
                    // Route point collides with vehicle
                    continue;
                }

                if (!frustum.IsSphereIn(position, mScale)) continue;

                renderer_->PushMatrix();
                renderer_->Translate(position);
                renderer_->Scale(mScale);
                mShader->UniformMatrix4fv("u_model", renderer_->model_matrix());
                mShader->Uniform3fv("u_color", mColor);
                Mesh::Render();
                renderer_->PopMatrix();
            } // for

            mShader->Unbind();
        }
        void RoutePointRenderer::Create()
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
        void RoutePointRenderer::FillAttributes()
        {
            // Specify attributes
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // vertex
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // normal
        }

    } // namespace terrain
} // namespace mgn