#include "mgnTrFakeTerrain.h"

namespace mgn {
    namespace terrain {

        FakeTerrain::FakeTerrain(mgn::graphics::Renderer * renderer,
            graphics::Shader * shader)
            : Mesh(renderer)
            , shader_(shader)
        {
            Create();
            MakeRenderable();
        }
        FakeTerrain::~FakeTerrain()
        {

        }
        void FakeTerrain::Render(float height, float size_x, float size_y)
        {
            shader_->Bind();
            shader_->Uniform4f("u_color", 0.666f, 0.666f, 0.666f, 1.0f);

            renderer_->DisableDepthTest();
            renderer_->PushMatrix();
            renderer_->Translate(0.0f, height, 0.0f);
            renderer_->Scale(size_x, 1.0f, size_y);
            shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
            Mesh::Render();
            renderer_->PopMatrix();
            renderer_->EnableDepthTest();

            shader_->Unbind();
        }
        void FakeTerrain::Create()
        {
            // Fill vertices
            num_vertices_ = 4;
            vertices_array_ = new unsigned char[num_vertices_ * 3 * sizeof(float)];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            vertices[0] = -0.5f;
            vertices[1] = 0.0f;
            vertices[2] = -0.5f;

            vertices[3] = 0.5f;
            vertices[4] = 0.0f;
            vertices[5] = -0.5f;

            vertices[6] = -0.5f;
            vertices[7] = 0.0f;
            vertices[8] = 0.5f;

            vertices[9] = 0.5f;
            vertices[10] = 0.0f;
            vertices[11] = 0.5f;

            // Fill indices
            num_indices_ = 4;
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 3;
        }
        void FakeTerrain::FillAttributes()
        {
            // Specify attributes
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); 
        }

    } // namespace terrain
} // namespace mgn