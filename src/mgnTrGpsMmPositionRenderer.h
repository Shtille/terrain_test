#ifndef __MGN_TERRAIN_GPS_MM_POSITION_RENDERER_H__
#define __MGN_TERRAIN_GPS_MM_POSITION_RENDERER_H__

#include "mgnTrMesh.h"

class mgnMdTerrainView;
class mgnMdTerrainProvider;

namespace mgn {
    namespace terrain {

        //! GPS map matching position renderer class
        class GpsMmPositionRenderer : private Mesh
        {
        public:
            explicit GpsMmPositionRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader);
            virtual ~GpsMmPositionRenderer();

            void Update(mgnMdTerrainProvider * provider);
            void Render();

        private:
            void Create();
            void FillAttributes();
            void CreateTexture();

            mgnMdTerrainView * mTerrainView;
            vec3 mGpsPosition;
            vec3 mMmPosition;
            float mScale;
            vec3 mGpsColor;
            vec3 mMmColor;
            bool mExists;

            graphics::Shader * mShader;
        };

    } // namespace terrain
} // namespace mgn

#endif