#pragma once
#ifndef __MGN_TERRAIN_FAKE_TERRAIN_H__
#define __MGN_TERRAIN_FAKE_TERRAIN_H__

#include "mgnTrMesh.h"

namespace mgn {
    namespace terrain {

        class FakeTerrain : private Mesh {
        public:
            explicit FakeTerrain(mgn::graphics::Renderer * renderer,
                graphics::Shader * shader);
            virtual ~FakeTerrain();

            void Render(float height, float size_x, float size_y);

        private:
            void Create();
            void FillAttributes();

            graphics::Shader * shader_;
        };

    } // namespace terrain
} // namespace mgn

#endif