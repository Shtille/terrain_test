#pragma once
#ifndef __MGN_TERRAIN_VEHICLE_RENDERER_H__
#define __MGN_TERRAIN_VEHICLE_RENDERER_H__

#include "mgnTrMesh.h"
#include "mgnMdWorldPosition.h"

class mgnMdTerrainView;

namespace mgn {
    namespace terrain {

        class VehicleRenderer : private Mesh {
        public:
            explicit VehicleRenderer(mgn::graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader);
            virtual ~VehicleRenderer();

            mgnMdWorldPosition GetPos() const;
            const mgnMdWorldPosition * GetPosPtr() const;
            vec3 GetCenter() const;
            float GetSize() const; // circumcircle radius

            void Update();
            void Render();

            void UpdateLocationIndicator(double lat, double lon, double altitude,
                double heading, double tilt, int color1, int color2);

        private:
            void Create();
            void FillAttributes();
            void CreateTexture();

            mgnMdTerrainView * mTerrainView;
            graphics::Shader * mShader;
            graphics::Texture * mTexture;

            mgnMdWorldPosition mGpsPosition;
            double mAltitude;
            int mColor1, mColor2;
            vec3 mPosition; //!< position in 3D 
            float mScale;
            float mHeading;
            float mTilt;
        };

    } // namespace terrain
} // namespace mgn

#endif