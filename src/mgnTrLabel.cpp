#include "mgnTrLabel.h"
#include "mgnTrConstants.h"

#include "mgnMdTerrainProvider.h"
#include "mgnMdTerrainView.h"

#include "mgnMdBitmap.h"

namespace mgn {
    namespace terrain {

        Label::Label(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const vec3* tile_position, const LabelPositionInfo &lpi, unsigned short magIndex)
        : Billboard(renderer, terrain_view, shader, tile_position, 0.002f)
        , mText(lpi.text)
        {
            Create(lpi, magIndex);
            MakeRenderable();
        }
        Label::~Label()
        {
        }
        const std::wstring& Label::text() const
        {
            return mText;
        }
        void Label::Create(const LabelPositionInfo &lpi, unsigned short magIndex)
        {
            const int kTileResolution = GetTileResolution();

            float metersInPixelLat = (float)mTerrainView->getCellSizeLat(magIndex)/kTileResolution;
            float metersInPixelLon = (float)mTerrainView->getCellSizeLon(magIndex)/kTileResolution;
            int magIndexDelta = magIndex - mTerrainView->getMagIndex();
            if (magIndexDelta < 0) magIndexDelta = 0;
            float scale_tex = (float)mTerrainView->getPixelScale()/1.5f * (float)(1 << (magIndexDelta+1));

            // Label size (in meters)
            float w = metersInPixelLon*lpi.w/scale_tex;
            float h = metersInPixelLat*lpi.h/scale_tex;

            mOrigin = (lpi.centered) ? Billboard::kBottomMiddle : Billboard::kBottomLeft;
            mWidth = w;
            mHeight = h;

            // Coordinates of the middle-bottom point of label
            mPosition.x = metersInPixelLon*lpi.lon;
            mPosition.y = (float)          lpi.alt;
            mPosition.z = metersInPixelLat*lpi.lat;

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
                vertices[ind++] = lpi.centered ? (-w/2.0f) : 0;
                vertices[ind++] = lpi.centered ? (-h/2.0f) : 0;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 0.0f;
                vertices[ind++] = 1.0f;
            }
            // bottom-right
            {
                // position
                vertices[ind++] = lpi.centered ? (w/2.0f) : w;
                vertices[ind++] = lpi.centered ? (-h/2.0f) : 0.0f;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 1.0f;
                vertices[ind++] = 1.0f;
            }
            // upper-left
            {
                // position
                vertices[ind++] = lpi.centered ? (-w/2.0f) : 0.0f;
                vertices[ind++] = lpi.centered ? (h/2.0f) : h;
                vertices[ind++] = 0.0f;
                // texture coordinates (0..1)
                vertices[ind++] = 0.0f;
                vertices[ind++] = 0.0f;
            }

            // upper-right
            {
                // position
                vertices[ind++] = lpi.centered ? (w/2.0f) : w;
                vertices[ind++] = lpi.centered ? (h/2.0f) : h;
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
