#pragma once
#ifndef __MGN_TERRAIN_GUIDANCE_ARROW_RENDERER_H__
#define __MGN_TERRAIN_GUIDANCE_ARROW_RENDERER_H__

#include "MapDrawing/Graphics/Renderer.h"

#include <utility>
#include <boost/optional.hpp>

class mgnMdTerrainView;
class mgnMdWorldPosition;

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        class GuidanceArrowRenderer {
        public:
            GuidanceArrowRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            ~GuidanceArrowRenderer();

            void UpdateLineWidth(float fovx, float distance_to_camera);

            void Update(MercatorProvider * provider);
            void Render(boost::optional<std::pair<vec2, float> > shadow);

            float size() const;
            bool exists() const;
            const vec3& position() const;

        private:
            void Create();
            void CreateArrow();
            void CreateExtrusion();
            void CalcRotation(MercatorProvider * provider);

            graphics::Renderer * renderer_;
            mgnMdTerrainView * terrain_view_;
            graphics::Shader * shader_;
            const mgnMdWorldPosition * gps_pos_;

            vec3 position_;
            double latitude_; //!< to not do additional conversions for rotation matrix calculation
            double longitude_;
            float heading_;
            float scale_;
            math::Matrix4 rotation_matrix_;
            float line_width_;
            bool exists_;
            bool need_rotation_matrix_;

            graphics::VertexFormat * vertex_format_;
            graphics::VertexBuffer * vertex_buffers_[3];
            graphics::IndexBuffer * index_buffers_[2];
        };

    } // namespace terrain
} // namespace mgn

#endif