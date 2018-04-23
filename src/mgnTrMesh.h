#pragma once
#ifndef __MGN_TERRAIN_MODEL_H__
#define __MGN_TERRAIN_MODEL_H__

#include "MapDrawing/Graphics/Renderer.h"

#include <vector>

namespace mgn {
    namespace terrain {
        
        //! Standard model class
        class Mesh {
        public:
            Mesh(graphics::Renderer * renderer);
            virtual ~Mesh();
            
            bool MakeRenderable();
            
            void Render();

        protected:
            virtual void FillAttributes() = 0;
            void AddFormat(const graphics::VertexAttribute& attrib);
            
            graphics::Renderer * renderer_;
            graphics::PrimitiveType::T primitive_mode_;
            
            unsigned int num_vertices_;
            unsigned char * vertices_array_;
            unsigned int num_indices_;
            unsigned int index_size_;
            unsigned char * indices_array_;
            graphics::DataType::T index_data_type_;
            
        private:
            void FreeArrays();

            graphics::VertexFormat * vertex_format_;
            graphics::VertexBuffer * vertex_buffer_;
            graphics::IndexBuffer * index_buffer_;

            std::vector<graphics::VertexAttribute> attribs_;

            bool can_render_;
        };
        
    } // namespace terrain
} // namespace mgn

#endif