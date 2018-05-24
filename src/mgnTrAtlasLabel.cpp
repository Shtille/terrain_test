#include "mgnTrAtlasLabel.h"

#include "mgnTrConstants.h"
#include "mgnTrFontAtlas.h"
#include "mgnTrMercatorDataInfo.h"
#include "mgnMdTerrainView.h"

#include "mgnMdBitmap.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

namespace mgn {
    namespace terrain {

        AtlasLabel::AtlasLabel(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const Font * font, const LabelData &data, int lod)
        : Billboard(renderer, terrain_view, shader, 0.002f)
        , mText(data.text)
        {
            Create(font, data, lod);
            MakeRenderable();
            setTexture(font->texture(), false); // font owns texture object
        }
        AtlasLabel::~AtlasLabel()
        {
        }
        const std::wstring& AtlasLabel::text() const
        {
            return mText;
        }
        void AtlasLabel::GetBoundingBox(const math::Matrix4& view, math::Rect& rect)
        {
            vec2 size;
            GetIconSize(size);

            // World space position
            vec4 icon_pos_world(mPosition, 1.0f);
            vec4 icon_pos_eye = view * icon_pos_world;

            switch (getOrigin())
            {
            case Billboard::kBottomLeft:
                rect.vertices[0].x = icon_pos_eye.x;
                rect.vertices[0].y = icon_pos_eye.y;
                break;
            case Billboard::kBottomMiddle:
                rect.vertices[0].x = icon_pos_eye.x - 0.5f * size.x;
                rect.vertices[0].y = icon_pos_eye.y;
                break;
            default:
                assert(false);
                break;
            }
            rect.vertices[1] = rect.vertices[0];
            rect.vertices[1].x += size.x;
            rect.vertices[2] = rect.vertices[0];
            rect.vertices[2].x += size.x;
            rect.vertices[2].y -= size.y; // sign "-" because of retarded view matrix (scale(1,-1,1))
            rect.vertices[3] = rect.vertices[0];
            rect.vertices[3].y -= size.y;
        }
        void AtlasLabel::Create(const Font * font, const LabelData &data, int lod)
        {
            const float kMSM = static_cast<float>(GetMapSizeMax());
            const float kCellSize = static_cast<float>(1 << (GetMaxLod() - lod)); // MSM / 2^lod / TileRes

            float scale_tex = (float)mTerrainView->getPixelScale()/1.5f * 2.0f;

            // Coefficients to convert from bitmap to tile coordinates (meters)
            const float kB2WX = kCellSize / scale_tex * font->scale();
            const float kB2WY = kCellSize / scale_tex * font->scale();

            // Coordinates of the middle-bottom point of label
            mTerrainView->WorldToPixel(data.latitude, data.longitude, data.altitude,
                mPosition, kMSM);

            // Compute vertex size of label
            float offset_x = 0.0f;
            float offset_y = 0.0f;
            float min_x = kMSM;
            float min_y = kMSM;
            float max_x = -kMSM;
            float max_y = -kMSM;
            for (const wchar_t* p = mText.c_str(); *p != L'\0'; ++p)
            {
                const FontCharInfo* info = font->info(*p);
                if (info == NULL)
                    continue;

                float glyph_size_x = info->bitmap_width;
                float glyph_size_y = info->bitmap_height;

                float left = info->bitmap_left;
                float top  = info->bitmap_top;

                float glyph_x = offset_x + left;
                float glyph_y = offset_y + top - glyph_size_y;

                if (glyph_x < min_x)
                    min_x = glyph_x;

                if (glyph_y < min_y)
                    min_y = glyph_y;

                if (glyph_x + glyph_size_x > max_x)
                    max_x = glyph_x + glyph_size_x;

                if (glyph_y + glyph_size_y > max_y)
                    max_y = glyph_y + glyph_size_y;

                offset_x += info->advance_x;
                offset_y += info->advance_y;
            }
            float size_x = max_x - min_x;
            float size_y = max_y - min_y;

            mOrigin = (data.centered) ? Billboard::kBottomMiddle : Billboard::kBottomLeft;
            mWidth = size_x * kB2WX;
            mHeight = size_y * kB2WY;

            if (data.centered)
            {
                offset_x = -0.5f * size_x;
                offset_y = -0.5f * size_y;
            }
            else
            {
                offset_x = 0.0f;
                offset_y = 0.0f;
            }

            unsigned int vertex_size = 5 * sizeof(float);
            index_size_ = sizeof(unsigned short);
            index_data_type_ = graphics::DataType::kUnsignedShort;
            num_vertices_ = 4 * mText.length();
            num_indices_ = 6 * mText.length() - 2;
            vertices_array_ = new unsigned char[num_vertices_ * vertex_size];
            float * vertices = reinterpret_cast<float*>(vertices_array_);
            indices_array_ = new unsigned char[num_indices_ * index_size_];
            unsigned short *indices = reinterpret_cast<unsigned short*>(indices_array_);

            volatile int vertices_index = 0;
            volatile int indices_index = 0;

            unsigned short index = 0;
            for (const wchar_t* p = mText.c_str(); *p != L'\0'; ++p)
            {
                // Character is already in UTF, so we don't need any translation
                const FontCharInfo* info = font->info(*p);

                // Our font doesn't present this character
                if (info == NULL)
                    continue;

                float glyph_size_x = info->bitmap_width;
                float glyph_size_y = info->bitmap_height;

                float glyph_x = offset_x + info->bitmap_left;
                float glyph_y = offset_y + info->bitmap_top - glyph_size_y;

                float lower, upper, left, right;
                left = (glyph_x) * kB2WX;
                right = (glyph_x + glyph_size_x) * kB2WX;
                lower = (glyph_y) * kB2WY;
                upper = (glyph_y + glyph_size_y) * kB2WY;

                offset_x += info->advance_x;
                offset_y += info->advance_y;

                float texcoord_right = info->texcoord_x + (info->bitmap_width / font->atlas_width() * font->scale_x());
                float texcoord_top = info->texcoord_y + (info->bitmap_height / font->atlas_height() * font->scale_y());

                // bottom-left
                {
                    // position
                    vertices[vertices_index++] = left;
                    vertices[vertices_index++] = lower;
                    vertices[vertices_index++] = 0.0f;
                    // texture coordinates (0..1)
                    vertices[vertices_index++] = info->texcoord_x;
                    vertices[vertices_index++] = texcoord_top;
                }
                // bottom-right
                {
                    // position
                    vertices[vertices_index++] = right;
                    vertices[vertices_index++] = lower;
                    vertices[vertices_index++] = 0.0f;
                    // texture coordinates (0..1)
                    vertices[vertices_index++] = texcoord_right;
                    vertices[vertices_index++] = texcoord_top;
                }
                // upper-left
                {
                    // position
                    vertices[vertices_index++] = left;
                    vertices[vertices_index++] = upper;
                    vertices[vertices_index++] = 0.0f;
                    // texture coordinates (0..1)
                    vertices[vertices_index++] = info->texcoord_x;
                    vertices[vertices_index++] = info->texcoord_y;
                }

                // upper-right
                {
                    // position
                    vertices[vertices_index++] = right;
                    vertices[vertices_index++] = upper;
                    vertices[vertices_index++] = 0.0f;
                    // texture coordinates (0..1)
                    vertices[vertices_index++] = texcoord_right;
                    vertices[vertices_index++] = info->texcoord_y;
                }

                if (p != mText.c_str()) // not beginning of the string
                {
                    // Add two degenerates
                    indices[indices_index++] = index - 1;
                    indices[indices_index++] = index;
                }

                indices[indices_index++] = index++;
                indices[indices_index++] = index++;
                indices[indices_index++] = index++;
                indices[indices_index++] = index++;
            }
        }

    } // namespace terrain
} // namespace mgn