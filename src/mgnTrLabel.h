#pragma once
#ifndef __MGN_TERRAIN_LABEL_H__
#define __MGN_TERRAIN_LABEL_H__

#include "mgnTrBillboard.h"
#include "mgnBaseType.h"

namespace mgn {
    namespace terrain {

        struct LabelData;

        //! class for rendering labels
        class Label : public Billboard {
        public:
            explicit Label(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const LabelData &data, int lod);
            virtual ~Label();

            const std::wstring& text() const;

        protected:
            std::wstring mText; //!< text of label

        private:
            void Create(const LabelData &data, int lod);
        };

    } // namespace terrain
} // namespace mgn

#endif