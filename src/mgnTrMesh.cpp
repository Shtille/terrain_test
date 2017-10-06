#include "mgnTrMesh.h"

#include <assert.h>

namespace mgn {
    namespace terrain {
        
        Mesh::Mesh(graphics::Renderer * renderer)
        : renderer_(renderer)
        , primitive_mode_(graphics::PrimitiveType::kTriangleStrip)
        , num_vertices_(0)
        , vertices_array_(NULL)
        , num_indices_(0)
        , index_size_(0)
        , indices_array_(NULL)
        , vertex_format_(NULL)
        , vertex_buffer_(NULL)
        , index_buffer_(NULL)
        {
            
        }
        Mesh::~Mesh()
        {
            if (vertex_format_)
                renderer_->DeleteVertexFormat(vertex_format_);
            if (vertex_buffer_)
                renderer_->DeleteVertexBuffer(vertex_buffer_);
            if (index_buffer_)
                renderer_->DeleteIndexBuffer(index_buffer_);
            FreeArrays();
        }
        void Mesh::AddFormat(const graphics::VertexAttribute& attrib)
        {
            attribs_.push_back(attrib);
        }
        void Mesh::FreeArrays()
        {
            if (vertices_array_)
            {
                delete [] vertices_array_;
                vertices_array_ = NULL;
            }
            if (indices_array_)
            {
                delete [] indices_array_;
                indices_array_ = NULL;
            }
        }
        bool Mesh::MakeRenderable()
        {
            FillAttributes();
            renderer_->AddVertexFormat(vertex_format_, &attribs_[0], (unsigned int)attribs_.size());
            
            renderer_->AddVertexBuffer(vertex_buffer_, num_vertices_ * vertex_format_->vertex_size(), vertices_array_, graphics::BufferUsage::kStaticDraw);
            if (vertex_buffer_ == NULL) return false;
            
            renderer_->AddIndexBuffer(index_buffer_, num_indices_, index_size_, indices_array_, graphics::BufferUsage::kStaticDraw);
            if (index_buffer_ == NULL) return false;
            
            FreeArrays();
            
            return true;
        }
        void Mesh::Render()
        {
            renderer_->ChangeVertexFormat(vertex_format_);
            renderer_->ChangeVertexBuffer(vertex_buffer_);
            renderer_->ChangeIndexBuffer(index_buffer_);
            renderer_->context()->DrawElements(primitive_mode_, num_indices_, index_data_type_);
        }

    } // namespace terrain
} // namespace mgn