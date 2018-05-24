#pragma once
#ifndef __MGN_TERRAIN_ATLAS_LABEL_H__
#define __MGN_TERRAIN_ATLAS_LABEL_H__

#include "mgnTrBillboard.h"
#include "mgnBaseType.h"

namespace math {
    struct Matrix4;
    struct Rect;
}

namespace mgn {
    namespace terrain {

        class Font;
        struct LabelData;

        //! class for rendering atlas based labels
        class AtlasLabel : public Billboard {
        public:
            explicit AtlasLabel(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const Font * font, const LabelData &data, int lod);
            virtual ~AtlasLabel();

            const std::wstring& text() const;

            //! Returns 4 vertices in eye space
            void GetBoundingBox(const math::Matrix4& view, math::Rect& rect);

        protected:
            std::wstring mText; //!< text of label

        private:
            void Create(const Font * font, const LabelData &data, int lod);
        };

    } // namespace terrain
} // namespace mgn

#endif