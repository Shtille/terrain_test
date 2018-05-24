#include "mgnTrLabel.h"

#include "mgnTrConstants.h"
#include "mgnTrMercatorDataInfo.h"

#include "mgnMdTerrainView.h"

#include "mgnMdBitmap.h"

namespace mgn {
    namespace terrain {

        Label::Label(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const LabelData &data, int lod)
        : Billboard(renderer, terrain_view, shader, 0.002f)
        , mText(data.text)
        {
            Create(data, lod);
            MakeRenderable();
        }
        Label::~Label()
        {
        }
        const std::wstring& Label::text() const
        {
            return mText;
        }
        void Label::Create(const LabelData &data, int lod)
        {
            const float kMSM = static_cast<float>(GetMapSizeMax());
            const float kCellSize = static_cast<float>(1 << (GetMaxLod() - lod));

            int lod_delta = mTerrainView->GetLod() - lod;
            if (lod_delta < 0) lod_delta = 0;
            float scale_tex = (float)mTerrainView->getPixelScale()/1.5f * (float)(1 << (lod_delta+1));

            // Label size (in meters)
            float w = kCellSize * data.width / scale_tex;
            float h = kCellSize * data.height / scale_tex;

            mOrigin = (data.centered) ? Billboard::kBottomMiddle : Billboard::kBottomLeft;
            mWidth = w;
            mHeight = h;

            // Coordinates of the middle-bottom point of label
            mTerrainView->WorldToPixel(data.latitude, data.longitude, data.altitude,
                mPosition, kMSM);

            unsigned int vertex_size = 5 * sizeof(float);
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            num_vertices_ = 4;
            num_indices_ = 4;
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);

            volatile int ind = 0;

            // bottom-left
            {
                // position
                vertices[ind++] = data.centered ? (-w/2.0f) : 0;
                vertices[ind++] = data.centered ? (-h/2.0f) : 0;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 0.0f;
                vertices[ind++] = 1.0f;
            }
            // bottom-right
            {
                // position
                vertices[ind++] = data.centered ? (w/2.0f) : w;
                vertices[ind++] = data.centered ? (-h/2.0f) : 0.0f;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 1.0f;
                vertices[ind++] = 1.0f;
            }
            // upper-left
            {
                // position
                vertices[ind++] = data.centered ? (-w/2.0f) : 0.0f;
                vertices[ind++] = data.centered ? (h/2.0f) : h;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 0.0f;
                vertices[ind++] = 0.0f;
            }

            // upper-right
            {
                // position
                vertices[ind++] = data.centered ? (w/2.0f) : w;
                vertices[ind++] = data.centered ? (h/2.0f) : h;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 1.0f;
                vertices[ind++] = 0.0f;
            }

            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 3;
        }

    } // namespace terrain
} // namespace mgn
