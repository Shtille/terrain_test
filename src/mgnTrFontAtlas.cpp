#include "mgnTrFontAtlas.h"

#include "mgnLog.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include "ftbitmap.h" // for embolden operations

#include <algorithm>
#include <cstring>
#include <cassert>

namespace {
    void Bitmap_Allocate(mgnMdBitmap * bitmap, int width, int height, int bytes_per_pixel)
    {
        bitmap->mWidth = width;
        bitmap->mHeight = height;
        bitmap->mBitsPerPix = bytes_per_pixel << 3;
        bitmap->mpData = new unsigned char[width * height * bytes_per_pixel];
    }
    void Bitmap_FillWithZeroes(mgnMdBitmap * bitmap)
    {
        const size_t width = static_cast<size_t>(bitmap->mWidth);
        const size_t height = static_cast<size_t>(bitmap->mHeight);
        const size_t bpp = static_cast<size_t>(bitmap->mBitsPerPix >> 3);
        size_t size = width * height * bpp * sizeof(unsigned char);
        memset(bitmap->mpData, 0, size);
    }
    void Bitmap_SubData(mgnMdBitmap * bitmap, int offset_x, int offset_y, int w, int h, const unsigned char* data)
    {
        const int width = static_cast<int>(bitmap->mWidth);
        const int height = static_cast<int>(bitmap->mHeight);
        const int bpp = static_cast<int>(bitmap->mBitsPerPix >> 3);
        unsigned char* pixels = bitmap->mpData;

        int max_x = offset_x + w;
        int max_y = offset_y + h;
        assert(max_x <= width);
        assert(max_y <= height);

        // Data should have the same bpp as source
        for (int y = 0; y < h; ++y)
        {
            unsigned char* dst = &pixels[((y + offset_y)*width + offset_x)*bpp];
            const unsigned char* src = &data[(y*w)*bpp];
            memcpy(dst, src, w * bpp);
        }
    }
    void Bitmap_SubDataColored(mgnMdBitmap * bitmap, int offset_x, int offset_y, int w, int h,
        const unsigned char* data, const Color& color)
    {
        const int width = static_cast<int>(bitmap->mWidth);
        const int height = static_cast<int>(bitmap->mHeight);
        const int bpp = static_cast<int>(bitmap->mBitsPerPix >> 3);

        int max_x = offset_x + w;
        int max_y = offset_y + h;
        assert(max_x <= width);
        assert(max_y <= height);
        assert(bpp == 4);

        unsigned char r = (unsigned char)(color.mR * 255.0f);
        unsigned char g = (unsigned char)(color.mG * 255.0f);
        unsigned char b = (unsigned char)(color.mB * 255.0f);

        const unsigned char* src = data;

        // Destination should have 4 bytes per pixel while source - only 1
        for (int y = offset_y; y < max_y; ++y)
        {
            for (int x = offset_x; x < max_x; ++x)
            {
                unsigned char* dst = &bitmap->mpData[(y*width + x)*bpp];
                *dst++ = r;
                *dst++ = g;
                *dst++ = b;
                *dst = *src++;
            }
        }
    }
    void Bitmap_SubDataAlphaBlend(mgnMdBitmap * bitmap, int offset_x, int offset_y, int w, int h,
        const unsigned char* data, const Color& color)
    {
        // Same as above, but using alpha blend for data modification
        const int width = static_cast<int>(bitmap->mWidth);
        const int height = static_cast<int>(bitmap->mHeight);
        const int bpp = static_cast<int>(bitmap->mBitsPerPix >> 3);

        int max_x = offset_x + w;
        int max_y = offset_y + h;
        assert(max_x <= width);
        assert(max_y <= height);
        assert(bpp == 4);

        const unsigned char* src = data;

        // Destination should have 4 bytes per pixel while source - only 1
        for (int y = offset_y; y < max_y; ++y)
        {
            for (int x = offset_x; x < max_x; ++x)
            {
                unsigned char* dst = &bitmap->mpData[(y*width + x)*bpp];

                float base_r = (float)(*(dst  ))/255.0f;
                float base_g = (float)(*(dst+1))/255.0f;
                float base_b = (float)(*(dst+2))/255.0f;
                float base_a = (float)(*(dst+3))/255.0f;
                float blend_r = color.mR;
                float blend_g = color.mG;
                float blend_b = color.mB;
                float blend_a = (float)(*src)/255.0f;
                float out_r = (1.0f - blend_a) * base_r + blend_a * blend_r;
                float out_g = (1.0f - blend_a) * base_g + blend_a * blend_g;
                float out_b = (1.0f - blend_a) * base_b + blend_a * blend_b;
                float out_a = (1.0f - blend_a) * base_a + blend_a * blend_a;
                *dst++ = (unsigned char)(out_r * 255.0f);
                *dst++ = (unsigned char)(out_g * 255.0f);
                *dst++ = (unsigned char)(out_b * 255.0f);
                *dst   = (unsigned char)(out_a * 255.0f);
                ++src;
            }
        }
    }
    void Bitmap_ChangeChannelsToFour(mgnMdBitmap * bitmap)
    {
        assert(bitmap->mBitsPerPix == 8);
        const int width = static_cast<int>(bitmap->mWidth);
        const int height = static_cast<int>(bitmap->mHeight);
        const int bpp = 4;
        unsigned char* pixels = new unsigned char[width * height * bpp];

        unsigned char* src = bitmap->mpData;
        unsigned char* dst = pixels;

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                *dst++ = 0xFF;
                *dst++ = 0xFF;
                *dst++ = 0xFF;
                *dst++ = *src++;
            }
        
        delete[] bitmap->mpData;
        bitmap->mpData = pixels;
        bitmap->mBitsPerPix = 32;
    }
    void Bitmap_ChangeChannelsToFourAndColorize(mgnMdBitmap * bitmap, const Color& color)
    {
        assert(bitmap->mBitsPerPix == 8);
        const int width = static_cast<int>(bitmap->mWidth);
        const int height = static_cast<int>(bitmap->mHeight);
        const int bpp = 4;
        unsigned char* pixels = new unsigned char[width * height * bpp];

        unsigned char* src = bitmap->mpData;
        unsigned char* dst = pixels;

        unsigned char r = (unsigned char)(color.mR * 255.0f);
        unsigned char g = (unsigned char)(color.mG * 255.0f);
        unsigned char b = (unsigned char)(color.mB * 255.0f);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                *dst++ = r;
                *dst++ = g;
                *dst++ = b;
                *dst++ = *src++;
            }

        delete[] bitmap->mpData;
        bitmap->mpData = pixels;
        bitmap->mBitsPerPix = 32;
    }
    template <typename T>
    static T RoundToPowerOfTwo(T x)
    {
        T n = 1;
        while (n < x)
            n <<= 1;
        return n;
    }
    static int InterpolateColor(int c1, int c2, float alpha)
    {
        int a1 = (c1 & 0xff000000) >> 24;
        int b1 = (c1 & 0xff0000) >> 16;
        int g1 = (c1 & 0x00ff00) >> 8;
        int r1 = (c1 & 0x0000ff);
        int a2 = (c2 & 0xff000000) >> 24;
        int b2 = (c2 & 0xff0000) >> 16;
        int g2 = (c2 & 0x00ff00) >> 8;
        int r2 = (c2 & 0x0000ff);
        int r = r1 + (int)((r2-r1)*alpha);
        int g = g1 + (int)((g2-g1)*alpha);
        int b = b1 + (int)((b2-b1)*alpha);
        int a = a1 + (int)((a2-a1)*alpha);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }
    void Bitmap_MakePotTextureFromNpot(mgnMdBitmap * bitmap, float* scale_x, float* scale_y)
    {
        const int width = static_cast<int>(bitmap->mWidth);
        const int height = static_cast<int>(bitmap->mHeight);
        const int bpp = 4;

        if (((width & (width-1)) != 0) || ((height & (height-1)) != 0))
        {
            // Need to make POT
            unsigned int * data = reinterpret_cast<unsigned int*>(bitmap->mpData);
            int w2 = RoundToPowerOfTwo(width);
            int h2 = RoundToPowerOfTwo(height);

            // Rescale image to power of 2
            int new_size = w2 * h2;
            unsigned int * new_data = new unsigned int[new_size];
            for (int dh2 = 0; dh2 < h2; ++dh2)
            {
                float rh = (float)dh2 / (float)h2;
                float y = rh * (float)height;
                int dh = (int)y;
                int dh1 = std::min<int>(dh+1, height-1);
                float ry = y - (float)dh; // fract part of y
                for (int dw2 = 0; dw2 < w2; ++dw2)
                {
                    float rw = (float)dw2 / (float)w2;
                    float x = rw * (float)width;
                    int dw = (int)x;
                    int dw1 = std::min<int>(dw+1, width-1);
                    float rx = x - (float)dw; // fract part of x

                    // We will use bilinear interpolation
                    int sample1 = (int) data[dw +width*dh ];
                    int sample2 = (int) data[dw1+width*dh ];
                    int sample3 = (int) data[dw +width*dh1];
                    int sample4 = (int) data[dw1+width*dh1];

                    int color1 = InterpolateColor(sample1, sample2, rx);
                    int color2 = InterpolateColor(sample3, sample4, rx);;
                    int color3 = InterpolateColor(color1, color2, ry);
                    new_data[dw2+w2*dh2] = (unsigned int)color3;
                }
            }
            // Finally
            delete[] bitmap->mpData;
            bitmap->mpData = reinterpret_cast<unsigned char*>(new_data);
            bitmap->mWidth = static_cast<unsigned short>(w2);
            bitmap->mHeight = static_cast<unsigned short>(h2);
            if (scale_x)
                *scale_x = (float)w2/(float)width;
            if (scale_y)
                *scale_y = (float)h2/(float)height;
        }
        else
        {
            if (scale_x)
                *scale_x = 1.0f;
            if (scale_y)
                *scale_y = 1.0f;
        }
    }
}

namespace mgn {
    namespace terrain {

        Font * Font::Create(graphics::Renderer * renderer,
            const char* filename, int font_height, const Color& color, float pixel_size)
        {
            Font * font = new Font(renderer, pixel_size);
            font->Make(filename, font_height, color);
            return font;
        }
        Font * Font::CreateWithBorder(graphics::Renderer * renderer,
            const char* filename, int font_height, int border, const Color& color, const Color& border_color, float pixel_size)
        {
            Font * font = new Font(renderer, pixel_size);
            font->MakeWithBorder(filename, font_height, border, color, border_color);
            return font;
        }
        Font::Font(graphics::Renderer * renderer, float pixel_size)
            : renderer_(renderer)
            , texture_(NULL)
            , scale_(pixel_size)
            , scale_x_(1.0f)
            , scale_y_(1.0f)
        {

        }
        Font::~Font()
        {
            if (texture_)
                renderer_->DeleteTexture(texture_);
        }
        const FontCharInfo* Font::info(unsigned int charcode) const
        {
            InfoMap::const_iterator it = info_map_.find(charcode);
            if (it != info_map_.end())
                return &(it->second);
            else
                return NULL;
        }
        const float Font::atlas_width() const
        {
            return (float)texture_->width();
        }
        const float Font::atlas_height() const
        {
            return (float)texture_->height();
        }
        graphics::Texture * Font::texture() const
        {
            return texture_;
        }
        const float Font::scale() const
        {
            return scale_;
        }
        const float Font::scale_x() const
        {
            return scale_x_;
        }
        const float Font::scale_y() const
        {
            return scale_y_;
        }
        void Font::Make(const char* filename, int font_height, const Color& color)
        {
            // Font scale is an actual scale multiplier for labels rendered with selected font
            // Use 2x increased quality to get finer fonts
            font_height <<= 1;
            scale_ *= 0.5f;

            mgnMdBitmap bitmap;
            bitmap.mpData = 0;

            if (MakeAtlas(filename, font_height, color, &bitmap))
            {
                assert(bitmap.mBitsPerPix == 32);
                // Create texture from data
                renderer_->CreateTextureFromData(texture_, bitmap.mWidth, bitmap.mHeight,
                    graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, bitmap.mpData);
            }
            if (bitmap.mpData)
                delete[] bitmap.mpData;
        }
        void Font::MakeWithBorder(const char* filename, int font_height, int border, const Color& color, const Color& border_color)
        {
            // Font scale is an actual scale multiplier for labels rendered with selected font
            // Use 2x increased quality to get finer fonts
            font_height <<= 1;
            border <<= 1;
            scale_ *= 0.5f;

            mgnMdBitmap bitmap;
            bitmap.mpData = 0;

            if (MakeAtlasWithBorder(filename, font_height, border, color, border_color, &bitmap))
            {
                assert(bitmap.mBitsPerPix == 32);
                LOG_INFO(0, ("creating font %hu x %hu", bitmap.mWidth, bitmap.mHeight));
                // Create texture from data
                renderer_->CreateTextureFromData(texture_, bitmap.mWidth, bitmap.mHeight,
                    graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, bitmap.mpData);
            }
            if (bitmap.mpData)
                delete[] bitmap.mpData;
        }
        bool Font::MakeAtlas(const char* filename, int font_height, const Color& color, mgnMdBitmap * bitmap)
        {
            FT_Library ft;
            FT_Face face;

            // Initialize library
            if (FT_Init_FreeType(&ft))
            {
                fprintf(stderr, "FreeType library initialization failed");
                return false;
            }

            // Load a font
            if (FT_New_Face(ft, filename, 0, &face))
            {
                fprintf(stderr, "Failed to load a font %s!\n", filename);
                FT_Done_FreeType(ft);
                return false;
            }

            // Set encoding
            if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
            {
                fprintf(stderr, "Failed to set encoding\n");
                FT_Done_FreeType(ft);
                return false;
            }

            // Set font height in pixels
            FT_Set_Pixel_Sizes(face, 0, font_height);

            FT_GlyphSlot g = face->glyph;

            const unsigned int kMaxTextureWidth = 1024U;

            int w,h;
            int roww = 0;
            int rowh = 0;
            w = 0;
            h = 0;

            FT_ULong charcode;
            FT_UInt gindex;

            // Find minimum size for a texture holding all visible ASCII characters
            charcode = FT_Get_First_Char(face, &gindex);
            while (gindex != 0)
            {
                // Load glyph
                if (FT_Load_Char(face, charcode, FT_LOAD_RENDER)) {
                    fprintf(stderr, "Loading character %lu failed!\n", charcode);
                    continue;
                }
                if (roww + g->bitmap.width + 1 >= kMaxTextureWidth) {
                    w = std::max(w, roww);
                    h += rowh;
                    roww = 0;
                    rowh = 0;
                }
                roww += g->bitmap.width + 1;
                rowh = std::max(rowh, g->bitmap.rows);
                // Goto next charcode
                charcode = FT_Get_Next_Char(face, charcode, &gindex);
            }

            w = std::max(w, roww);
            h += rowh;

            if (w == 0 || h == 0)
            {
                FT_Done_FreeType(ft);
                return false;
            }

            // Create an image that will be used to hold all ASCII glyphs
            Bitmap_Allocate(bitmap, w, h, 4);
            Bitmap_FillWithZeroes(bitmap);

            // Paste all glyph bitmaps into the image, remembering the offset
            int ox = 0;
            int oy = 0;

            rowh = 0;

            charcode = FT_Get_First_Char(face, &gindex);
            while (gindex != 0)
            {
                if (FT_Load_Char(face, charcode, FT_LOAD_RENDER)) {
                    fprintf(stderr, "Loading character %lu failed!\n", charcode);
                    continue;
                }

                if (ox + g->bitmap.width + 1 >= kMaxTextureWidth) {
                    oy += rowh;
                    rowh = 0;
                    ox = 0;
                }

                Bitmap_SubDataColored(bitmap, ox, oy, g->bitmap.width, g->bitmap.rows, g->bitmap.buffer, color);

                unsigned int i = static_cast<unsigned int>(charcode);

                FontCharInfo & info = info_map_[i];

                info.advance_x = static_cast<float>(g->advance.x >> 6);
                info.advance_y = static_cast<float>(g->advance.y >> 6);

                info.bitmap_width = static_cast<float>(g->bitmap.width);
                info.bitmap_height = static_cast<float>(g->bitmap.rows);

                info.bitmap_left = static_cast<float>(g->bitmap_left);
                info.bitmap_top = static_cast<float>(g->bitmap_top);

                info.texcoord_x = ox / (float)w;
                info.texcoord_y = oy / (float)h;

                rowh = std::max(rowh, g->bitmap.rows);
                ox += g->bitmap.width + 1;

                // Goto next charcode
                charcode = FT_Get_Next_Char(face, charcode, &gindex);
            }

            FT_Done_Face(face);

            FT_Done_FreeType(ft);

            // Rescale texture to power of two
            Bitmap_MakePotTextureFromNpot(bitmap, &scale_x_, &scale_y_);

            return true;
        }
        bool Font::MakeAtlasWithBorder(const char* filename, int font_height, int border, 
            const Color& color, const Color& border_color, mgnMdBitmap * bitmap)
        {
            FT_Library ft;
            FT_Face face;

            // Initialize library
            if (FT_Init_FreeType(&ft))
            {
                fprintf(stderr, "FreeType library initialization failed");
                return false;
            }

            // Load a font
            if (FT_New_Face(ft, filename, 0, &face))
            {
                fprintf(stderr, "Failed to load a font %s!\n", filename);
                FT_Done_FreeType(ft);
                return false;
            }

            // Set encoding
            if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
            {
                fprintf(stderr, "Failed to set encoding\n");
                FT_Done_FreeType(ft);
                return false;
            }

            // Set font height in pixels
            FT_Set_Pixel_Sizes(face, 0, font_height);

            FT_GlyphSlot g = face->glyph;

            const unsigned int kMaxTextureWidth = 1024U;

            int w,h;
            int roww = 0;
            int rowh = 0;
            w = 0;
            h = 0;

            FT_Bitmap border_bitmap;
            FT_Bitmap_New(&border_bitmap);

            FT_ULong charcode;
            FT_UInt gindex;

            // Find minimum size for a texture holding all visible ASCII characters
            charcode = FT_Get_First_Char(face, &gindex);
            while (gindex != 0)
            {
                // Load glyph
                if (FT_Load_Char(face, charcode, FT_LOAD_RENDER)) {
                    fprintf(stderr, "Loading character %lu failed!\n", charcode);
                    continue;
                }
                // Copy glyph bitmap to our border bitmap and embolden it
                FT_Bitmap_Copy(ft, &g->bitmap, &border_bitmap);
                FT_Bitmap_Embolden(
                    ft,
                    &border_bitmap,
                    FT_Pos(border * 32) * 2, //multiply by 32 because FreeType expects 26.6 values;
                    FT_Pos(border * 32) * 2);
                if (roww + border_bitmap.width + 1 >= kMaxTextureWidth) {
                    w = std::max(w, roww);
                    h += rowh;
                    roww = 0;
                    rowh = 0;
                }
                roww += border_bitmap.width + 1;
                rowh = std::max(rowh, border_bitmap.rows);
                // Goto next charcode
                charcode = FT_Get_Next_Char(face, charcode, &gindex);
            }

            w = std::max(w, roww);
            h += rowh;

            if (w == 0 || h == 0)
            {
                FT_Done_FreeType(ft);
                return false;
            }

            // Create an image that will be used to hold all ASCII glyphs
            Bitmap_Allocate(bitmap, w, h, 4);
            Bitmap_FillWithZeroes(bitmap);

            // Paste all glyph bitmaps into the image, remembering the offset
            int ox = 0;
            int oy = 0;

            rowh = 0;

            charcode = FT_Get_First_Char(face, &gindex);
            while (gindex != 0)
            {
                if (FT_Load_Char(face, charcode, FT_LOAD_RENDER)) {
                    fprintf(stderr, "Loading character %lu failed!\n", charcode);
                    continue;
                }

                // Copy glyph bitmap to our border bitmap and embolden it
                FT_Bitmap_Copy(ft, &g->bitmap, &border_bitmap);
                FT_Bitmap_Embolden(
                    ft,
                    &border_bitmap,
                    FT_Pos(border * 32) * 2, //multiply by 32 because FreeType expects 26.6 values;
                    FT_Pos(border * 32) * 2);//multiply by 32 because we want it bigger on both sides, not just one

                if (ox + border_bitmap.width + 1 >= kMaxTextureWidth) {
                    oy += rowh;
                    rowh = 0;
                    ox = 0;
                }

                int offset_x = (border_bitmap.width - g->bitmap.width) >> 1;
                int offset_y = (border_bitmap.rows - g->bitmap.rows) >> 1;

                Bitmap_SubDataColored(bitmap, ox, oy, 
                    border_bitmap.width, border_bitmap.rows, border_bitmap.buffer,
                    border_color);
                Bitmap_SubDataAlphaBlend(bitmap, ox + offset_x, oy + offset_y,
                    g->bitmap.width, g->bitmap.rows, g->bitmap.buffer,
                    color);

                unsigned int i = static_cast<unsigned int>(charcode);

                FontCharInfo & info = info_map_[i];

                info.advance_x = static_cast<float>(g->advance.x >> 6);
                info.advance_y = static_cast<float>(g->advance.y >> 6);

                info.bitmap_width = static_cast<float>(border_bitmap.width);
                info.bitmap_height = static_cast<float>(border_bitmap.rows);

                info.bitmap_left = static_cast<float>(g->bitmap_left - offset_x);
                info.bitmap_top = static_cast<float>(g->bitmap_top - offset_y);

                info.texcoord_x = ox / (float)w;
                info.texcoord_y = oy / (float)h;

                rowh = std::max(rowh, border_bitmap.rows);
                ox += border_bitmap.width + 1;

                // Goto next charcode
                charcode = FT_Get_Next_Char(face, charcode, &gindex);
            }

            FT_Bitmap_Done(ft, &border_bitmap);

            FT_Done_Face(face);

            FT_Done_FreeType(ft);

            // Rescale texture to power of two
            Bitmap_MakePotTextureFromNpot(bitmap, &scale_x_, &scale_y_);

            return true;
        }

    } // namespace terrain
} // namespace mgn
