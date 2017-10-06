#pragma once
#ifndef __MGN_TERRAIN_BILLBOARD_H__
#define __MGN_TERRAIN_BILLBOARD_H__

#include "mgnTrMesh.h"

class mgnMdTerrainView;

namespace mgn {
    namespace terrain {

        class Billboard : protected Mesh {
        public:
            explicit Billboard(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const vec3* tile_position, float scale);
            virtual ~Billboard();

            enum OriginType {
                kBottomLeft,
                kBottomMiddle
            };

            void render();

            void setTexture(graphics::Texture * texture, bool owns_texture = true);
            virtual void GetIconSize(vec2& size);

            const vec3& position() const;
            const vec3& tile_position() const;
            OriginType getOrigin() const; // need for selection icon id

        protected:
            mgnMdTerrainView * mTerrainView;
            graphics::Shader * mShader;
            graphics::Texture * mTexture;

            vec3 mPosition;     //!< 3D position in tile coord system
            const vec3* mTilePosition; //!< tile position (icons offset in world space)
            float mScale;       //!< view dependent scale
            OriginType mOrigin; //!< type of icon origin
            float mWidth;
            float mHeight;
            bool mOwnsTexture;

        private:
            virtual void FillAttributes();
        };

    } // namespace terrain
} // namespace mgn

#endif