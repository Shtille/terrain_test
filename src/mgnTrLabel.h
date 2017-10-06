#pragma once
#ifndef __MGN_TERRAIN_LABEL_H__
#define __MGN_TERRAIN_LABEL_H__

#include "mgnTrBillboard.h"
#include "mgnBaseType.h"

struct LabelPositionInfo;

namespace mgn {
    namespace terrain {

        //! class for rendering labels
        class Label : public Billboard {
        public:
            explicit Label(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const vec3* tile_position, const LabelPositionInfo &lpi, unsigned short magIndex);
            virtual ~Label();

            const std::wstring& text() const;

        protected:
            std::wstring mText; //!< text of label

        private:
            void Create(const LabelPositionInfo &lpi, unsigned short magIndex);
        };

    } // namespace terrain
} // namespace mgn

#endif