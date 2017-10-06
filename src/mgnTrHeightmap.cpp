#include "mgnTrHeightmap.h"

#include <assert.h>

namespace mgn {
    namespace terrain {

        Heightmap::Heightmap(mgn::graphics::Renderer * renderer,
                int width, int height, float *heights, float size_x, float size_y)
        : Mesh(renderer)
        , mWidth(width)
        , mHeight(height)
        , mSizeX(size_x)
        , mSizeY(size_y)
        {
            assert(heights);
            mTileSizeX = size_x / float(width-1);
            mTileSizeY = size_y / float(height-1);
            create(heights);
        }
        Heightmap::~Heightmap()
        {
        }
        int Heightmap::width() const
        {
            return mWidth;
        }
        int Heightmap::height() const
        {
            return mHeight;
        }
        float Heightmap::minHeight() const
        {
            return mMinHeight;
        }
        float Heightmap::maxHeight() const
        {
            return mMaxHeight;
        }
        void Heightmap::FillAttributes()
        {
            // Specify attributes
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)); // vertex
            AddFormat(graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 2)); // texture coordinate
        }
        void Heightmap::create(float *heights)
        {
            const int kVertexOffset = 5;

            unsigned int vertex_size = kVertexOffset * sizeof(float);
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            num_vertices_ = mWidth * mHeight * kVertexOffset;
            num_indices_ = 2 * mWidth * (mHeight-1) + 2 * (mHeight-2);
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);

            float half_w = float(mWidth-1)*0.5f;
            float half_h = float(mHeight-1)*0.5f;

            // Calc minimum and maximum heights
            mMinHeight = heights[0];
            mMaxHeight = heights[0];

            int hcount = 0;
            int index = 0;
            for(int y = 0; y < mHeight; ++y)
            {
                for(int x = 0; x < mWidth; ++x)
                {
                    const float& height_sample = heights[hcount];

                    if (height_sample < mMinHeight)
                        mMinHeight = height_sample;
                    if (height_sample > mMaxHeight)
                        mMaxHeight = height_sample;

                    // Point coordinates (local)
                    vertices[index++] = -(float(x) - half_w) * mTileSizeX; // longitude
                    vertices[index++] = height_sample;
                    vertices[index++] = -(float(y) - half_h) * mTileSizeY; // latitude

                    // Texture coordinates (0..1)
                    vertices[index++] = 1.0f-(float(x)/float(mWidth-1));
                    vertices[index++] =      (float(y)/float(mHeight-1));

                    ++hcount;
                }
            }

            /*  3    2
            x <-----+
                |  /|
                | / |
                |/  |
                +---| 1
               0    |
                    V y
            */

            index = 0;
            for(int y = 0; y < mHeight-1; ++y)
            {
                for(int x = 0; x < mWidth; ++x)
                {
                    unsigned short ind0 = static_cast<unsigned short>(y * mWidth + x);
                    unsigned short ind1 = static_cast<unsigned short>(ind0 + mWidth);

                    indices[index++] = ind1;
                    indices[index++] = ind0;
                }
                // Add 2 degenerates
                if (y+2 != mHeight) // except the last strip
                {
                    unsigned short deg0 = static_cast<unsigned short>(y * mWidth + mWidth - 1);
                    unsigned short deg1 = static_cast<unsigned short>(deg0 + mWidth + 1);

                    indices[index++] = deg0;
                    indices[index++] = deg1;
                }
            }
        }

    } // namespace terrain
} // namespace mgn