#include "mgnTrMercatorTileMesh.h"

namespace mgn {
    namespace terrain {

    	MercatorTileMesh::MercatorTileMesh(graphics::Renderer * renderer, int grid_size)
			: Mesh(renderer)
			, grid_size_(grid_size)
		{
			primitive_mode_ = graphics::PrimitiveType::kTriangles;
		}
		MercatorTileMesh::~MercatorTileMesh()
		{

		}
        void MercatorTileMesh::FillAttributes()
        {
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kVertex, 3)); // vertex
        }
		void MercatorTileMesh::Create()
		{
			const unsigned int uGridSize = static_cast<unsigned int>(grid_size_);
			const int iGridSizeMinusOne = grid_size_ - 1;
			const float fGridSizeMinusOne = (float) iGridSizeMinusOne;

            num_vertices_ = uGridSize * uGridSize + uGridSize * 4;
            num_indices_ = ((uGridSize - 1) * (uGridSize - 1) + 4 * (uGridSize - 1)) * 6;

            unsigned int vertex_size = 3 * sizeof(float);
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];

            index_size_ = sizeof(unsigned int);
            index_data_type_ = graphics::DataType::kUnsignedInt;
            indices_array_ = new unsigned char[num_indices_ * index_size_];

            math::Vector3 * vertices = reinterpret_cast<math::Vector3 *>(vertices_array_);
            unsigned int * indices = reinterpret_cast<unsigned int *>(indices_array_);

			volatile unsigned int index = 0;
			// Output vertex data for regular grid.
			for (int j = 0; j < grid_size_; ++j)
			{
				for (int i = 0; i < grid_size_; ++i)
				{
					float x = (float) i / fGridSizeMinusOne;
					float y = (float) j / fGridSizeMinusOne;

					vertices[index].x = x;
					vertices[index].y = y;
					vertices[index].z = 0.0f;
					++index;
				}
			}
			// Output vertex data for x skirts.
			for (int j = 0; j < grid_size_; j += iGridSizeMinusOne)
			{
				for (int i = 0; i < grid_size_; ++i)
				{
					float x = (float) i / fGridSizeMinusOne;
					float y = (float) j / fGridSizeMinusOne;

					vertices[index].x = x;
					vertices[index].y = y;
					vertices[index].z = -1.0f;
					++index;
				}
			}
			// Output vertex data for y skirts.
			for (int i = 0; i < grid_size_; i += iGridSizeMinusOne)
			{
				for (int j = 0; j < grid_size_; ++j)
				{
					float x = (float) i / fGridSizeMinusOne;
					float y = (float) j / fGridSizeMinusOne;

					vertices[index].x = x;
					vertices[index].y = y;
					vertices[index].z = -1.0f;
					++index;
				}
			}

			unsigned int * pIndex = indices;

			index = 0;
			volatile unsigned int skirt_index = 0;
			// Output indices for regular surface.
			for (int j = 0; j < iGridSizeMinusOne; ++j)
			{
				for (int i = 0; i < iGridSizeMinusOne; ++i)
				{
					*pIndex++ = index;
					*pIndex++ = index + 1;
					*pIndex++ = index + uGridSize;

					*pIndex++ = index + uGridSize;
					*pIndex++ = index + 1;
					*pIndex++ = index + uGridSize + 1;

					++index;
				}
				++index;
			}
			// Output indices for x skirts.
			index = 0;
			skirt_index = uGridSize * uGridSize;
			for (int i = 0; i < iGridSizeMinusOne; ++i)
			{
				*pIndex++ = index;
				*pIndex++ = skirt_index;
				*pIndex++ = index + 1;

				*pIndex++ = skirt_index;
				*pIndex++ = skirt_index + 1;
				*pIndex++ = index + 1;

				index++;
				skirt_index++;
			}
			index = uGridSize * (uGridSize - 1);
			skirt_index = uGridSize * (uGridSize + 1);
			for (int i = 0; i < iGridSizeMinusOne; ++i)
			{
				*pIndex++ = index;
				*pIndex++ = index + 1;
				*pIndex++ = skirt_index;

				*pIndex++ = skirt_index;
				*pIndex++ = index + 1;
				*pIndex++ = skirt_index + 1;

				index++;
				skirt_index++;
			}
			// Output indices for y skirts.
			index = 0;
			skirt_index = uGridSize * (uGridSize + 2);
			for (int i = 0; i < iGridSizeMinusOne; ++i)
			{
				*pIndex++ = index;
				*pIndex++ = index + uGridSize;
				*pIndex++ = skirt_index;

				*pIndex++ = skirt_index;
				*pIndex++ = index + uGridSize;
				*pIndex++ = skirt_index + 1;

				index += uGridSize;
				skirt_index++;
			}
			index = (uGridSize - 1);
			skirt_index = uGridSize * (uGridSize + 3);
			for (int i = 0; i < iGridSizeMinusOne; ++i)
			{
				*pIndex++ = index;
				*pIndex++ = skirt_index;
				*pIndex++ = index + uGridSize;

				*pIndex++ = skirt_index;
				*pIndex++ = skirt_index + 1;
				*pIndex++ = index + uGridSize;

				index += uGridSize;
				skirt_index++;
			}
		}

    } // namespace terrain
} // namespace mgn