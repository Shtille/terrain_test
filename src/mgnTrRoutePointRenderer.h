#ifndef __MGN_TERRAIN_ROUTE_POINT_RENDERER_H__
#define __MGN_TERRAIN_ROUTE_POINT_RENDERER_H__

#include "mgnTrMesh.h"

#include <vector>

class mgnMdTerrainView;

namespace math {
    class Frustum;
}

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        //! Base class for rendering route point
        class RoutePointRenderer : private Mesh
        {
        public:
            explicit RoutePointRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader);
            virtual ~RoutePointRenderer();

            virtual void Update(MercatorProvider * provider) = 0;
            void Render(const math::Frustum& frustum, const vec3& vehicle_pos, float vehicle_size);

        protected:
            mgnMdTerrainView * mTerrainView;
            std::vector<vec3> mPositions;
            float mScale;
            vec3 mColor;
            bool mExists;

        private:
            void Create();
            void FillAttributes();
            void CreateTexture();

            graphics::Shader * mShader;
        };

    } // namespace terrain
} // namespace mgn

#endif