#pragma once
#ifndef __MGN_TERRAIN_FONT_ATLAS_H__
#define __MGN_TERRAIN_FONT_ATLAS_H__

#include "MapDrawing/Graphics/Renderer.h"

#include "Color.h"
#include "mgnMdBitmap.h"

#include <boost/unordered_map.hpp>

namespace mgn {
    namespace terrain {

        //! Structure containing all offsets data
        struct FontCharInfo {
            float advance_x;
            float advance_y;
            float bitmap_width;
            float bitmap_height;
            float bitmap_left;
            float bitmap_top;
            float texcoord_x;
            float texcoord_y;
        };

        struct FontGlyphPoint {
            float position_x;
            float position_y;
            float texcoord_x;
            float texcoord_y;
        };

        //! Font class
        class Font {
        public:

            static Font * Create(graphics::Renderer * renderer,
                const char* filename, int font_height, const Color& color, float pixel_size);
            static Font * CreateWithBorder(graphics::Renderer * renderer,
                const char* filename, int font_height, int border, const Color& color, const Color& border_color, float pixel_size);
            ~Font();

            const FontCharInfo* info(unsigned int charcode) const;
            const float atlas_width() const;
            const float atlas_height() const;
            graphics::Texture * texture() const;
            const float scale() const;
            const float scale_x() const;
            const float scale_y() const;

        protected:
            explicit Font(graphics::Renderer * renderer, float pixel_size);

            void Make(const char* filename, int font_height, const Color& color);
            void MakeWithBorder(const char* filename, int font_height, int border, const Color& color, const Color& border_color);
            bool MakeAtlas(const char* filename, int font_height, const Color& color, mgnMdBitmap * bitmap);
            bool MakeAtlasWithBorder(const char* filename, int font_height, int border, 
                const Color& color, const Color& border_color, mgnMdBitmap * bitmap);

            // Don't allow to copy and assign
            Font(const Font&);
            void operator = (const Font&);

        private:
            graphics::Renderer * renderer_;
            graphics::Texture * texture_;
            typedef boost::unordered_map<unsigned int, FontCharInfo> InfoMap;
            InfoMap info_map_;
            float scale_; //!< for atlas
            float scale_x_;
            float scale_y_;
        };

    } // namespace terrain
} // namespace mgn

#endif