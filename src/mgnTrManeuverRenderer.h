#pragma once
#ifndef __MGN_TERRAIN_MANEUVER_RENDERER_H__
#define __MGN_TERRAIN_MANEUVER_RENDERER_H__

#include "ArrowBuilder.h"

#include "MapDrawing/Graphics/Renderer.h"

#include <vector>
#include <utility>
#include <boost/unordered_map.hpp>
#include <boost/optional.hpp>
#include <boost/array.hpp>

class mgnMdTerrainView;
class mgnMdTerrainProvider;
class mgnMdWorldPosition;
struct GeoRoutePoint;

namespace mgn {
    namespace terrain {

        class ManeuverRenderer {
        public:
            enum color_name {face, dark, frame};

            ManeuverRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            ~ManeuverRenderer();

            void Update(mgnMdTerrainProvider * provider);
            void Render(boost::optional<std::pair<vec2, float> > shadow);

            bool exists() const;
            boost::optional<std::pair<vec2, float> > shadow() const;

        private:
            void FillBuffers(mgnMdTerrainProvider * provider, const std::vector<GeoRoutePoint> routePoints);
            void RenderArrow(const BufferOps::arrow_data& arrowData);

            graphics::Renderer * renderer_;
            mgnMdTerrainView * terrain_view_;
            graphics::Shader * shader_;
            const mgnMdWorldPosition * gps_pos_;
            graphics::VertexFormat * vertex_format_;

            static boost::array<float, 4> cFaceColor;
            static boost::array<float, 4> cFrameColor;
            boost::unordered_map<gmu::inner::primitive::kind, graphics::PrimitiveType::T> mPTypeMap;
            BufferOps::arrow_data mArrowData;
            BufferOps::arrow_data mExtrusionData;
            BufferOps::arrow_data mExtrusionBlackData;
            vec2 mArrowCenter;
            float mArrowSize;
            float mScaleKf;
        };

    } // namespace terrain
} // namespace mgn

#endif