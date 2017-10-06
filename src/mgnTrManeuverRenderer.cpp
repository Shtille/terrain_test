#include "mgnTrManeuverRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnMdWorldPoint.h"
#include "mgnMdIUserDataDrawContext.h"

#include <cmath>

#include <vector>
#include <algorithm>

#include <boost/assign/std/vector.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>
#include <boost/range/as_array.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tuple/tuple.hpp>

#include "StripeAlgorithm.h"

class TerrainMapPathContext : public mgnMdIUserDataDrawContext {
public:
    explicit TerrainMapPathContext(mgnMdTerrainView * terrain_view, const mgnMdWorldPosition * gps_pos)
    : gps_pos_(gps_pos)
    {
        world_rect_ = terrain_view->getLowDetailWorldRect();
    }
    mgnMdWorldRect GetWorldRect() const { return world_rect_; }
    int GetMapScaleIndex() const { return 1; }
    float GetMapScaleFactor() const { return 30.0f; }
    mgnMdWorldPosition GetGPSPosition() const { return *gps_pos_; }
    bool ShouldDrawPOIs() const { return false; }
    bool ShouldDrawLabels() const { return false; }

private:
    const mgnMdWorldPosition * gps_pos_;
    mgnMdWorldRect world_rect_;
};

namespace gmu
{
  template <>
  struct point_traits<vec3>
  {
    typedef float coordinate_type;
  };

  template <>
  struct access<vec3, 0>
  {
    static float get(const vec3& p)
    {
      return p.x;
    }

    static void set(vec3& p, const float& v)
    {
      p.x = v;
    }
  };

  template <>
  struct access<vec3, 1>
  {
    static float get(const vec3& p)
    {
      return p.z;
    }

    static void set(vec3& p, const float& v)
    {
      p.z = v;
    }
  };
}

template <typename Container>
class point_store : public gmu::inner::point_store<Container>
{
public:
  typedef typename gmu::inner::point_store<Container>::part_type part_type;

  point_store(Container& container)
    : gmu::inner::point_store<Container>(container)
  {}

  Container& get_points()
  {
    return this->points;
  }

  std::list<part_type>& get_parts()
  {
    return this->parts;
  }

  std::vector<typename gmu::inner::point_store<Container>::size_type>& get_left_edge()
  {
    return this->left_edge;
  }

  std::vector<typename gmu::inner::point_store<Container>::size_type>& get_right_edge()
  {
    return this->right_edge;
  }
};

typedef point_store<std::vector<vec3> > vec_point_store;

class GeoRoutePoint2Local
{
  mgnMdTerrainView * terrain_view_;
public:
  GeoRoutePoint2Local(mgnMdTerrainView * terrain_view)
    : terrain_view_(terrain_view)
  {}

  vec3 operator()(const GeoRoutePoint& rp) const
  {
    double local_x, local_y;
    terrain_view_->WorldToLocal(rp.point, local_x, local_y);
    return vec3((float)local_x, 0.0f, (float)local_y);
  }
};

static void update_store_z(mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider, vec_point_store& store)
{
  std::vector<vec3>& points = store.get_points();
  mgnMdWorldPoint wp;
  BOOST_FOREACH(point_store<std::vector<vec3> >::part_type& part, store.get_parts() | boost::adaptors::filtered(boost::lambda::bind(&point_store<std::vector<vec3> >::part_type::first, boost::lambda::_1) == gmu::inner::primitive::TriangleFun))
  {
    vec3& lp = points[part.second.front()];
    terrain_view->LocalToWorld(lp.x, lp.z, wp);
    float altitude = (float)provider->getAltitude(wp.mLatitude, wp.mLongitude);
    BOOST_FOREACH(std::size_t p_idx, part.second)
    {
      points[p_idx].y = altitude;
    }
  }
  vec3& fp = points.front(), &lp = points.back();
  terrain_view->LocalToWorld(fp.x, fp.z, wp);
  float altitude = (float)provider->getAltitude(wp.mLatitude, wp.mLongitude);
  points[0].y = altitude;
  points[1].y = altitude;
  terrain_view->LocalToWorld(lp.x, lp.z, wp);
  altitude = (float)provider->getAltitude(wp.mLatitude, wp.mLongitude);
  points[points.size() - 2].y = altitude;
  points[points.size() - 1].y = altitude;
}

namespace mgn {
    namespace terrain {

        boost::array<float, 4> ManeuverRenderer::cFaceColor = {1.0f, 0.713f, 0.223f, 1.0f};
        boost::array<float, 4> ManeuverRenderer::cFrameColor = {0.0f, 0.0f, 0.0f, 1.0f};

        ManeuverRenderer::ManeuverRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
        : renderer_(renderer)
        , terrain_view_(terrain_view)
        , shader_(shader)
        , gps_pos_(gps_pos)
        , vertex_format_(NULL)
        , mArrowCenter(0, 0)
        , mArrowSize(0)
        , mScaleKf(1.0f)
        {
            mPTypeMap[gmu::inner::primitive::Triangles] = graphics::PrimitiveType::kTriangles;
            mPTypeMap[gmu::inner::primitive::TriangleFun] = graphics::PrimitiveType::kTriangleFan;
            mPTypeMap[gmu::inner::primitive::TriangleStrip] = graphics::PrimitiveType::kTriangleStrip;
            mPTypeMap[gmu::inner::primitive::LineLoop] = graphics::PrimitiveType::kLineStrip;
            mPTypeMap[gmu::inner::primitive::LineStrip] = graphics::PrimitiveType::kLineStrip;
            mPTypeMap[gmu::inner::primitive::Lines] = graphics::PrimitiveType::kLines;

            graphics::VertexAttribute attribs[] = {
                graphics::VertexAttribute(graphics::VertexAttribute::kGeneric, 3)
            };
            renderer_->AddVertexFormat(vertex_format_, &attribs[0], 1);
        }
        ManeuverRenderer::~ManeuverRenderer()
        {
            if (vertex_format_)
                renderer_->DeleteVertexFormat(vertex_format_);
        }
        void ManeuverRenderer::Update(mgnMdTerrainProvider * provider)
        {
            std::vector<GeoRoutePoint> routePoints;
            mScaleKf = std::max(static_cast<float>(terrain_view_->getCamDistance()*0.01), 1.0f);
            if (provider->fetchManeuverAroundPath(TerrainMapPathContext(terrain_view_, gps_pos_), 2.5*mScaleKf, routePoints) && 
                terrain_view_->getMagIndex() < 5)
            {
                FillBuffers(provider, routePoints);
                if (mArrowData.points.size() > 0)
                {
                    gmu::inner::rect<vec3> bounds = std::for_each(mArrowData.points.begin() + 1, mArrowData.points.end(), gmu::inner::rect<vec3>(mArrowData.points[0]));
                    mArrowCenter = vec2((bounds.left + bounds.right)/2.0, (bounds.top + bounds.bottom)/2.0);
                    mArrowSize = std::max(bounds.top - bounds.bottom, bounds.right - bounds.left)/2.0;
                }
            }
            else
            {
                mArrowData.clear();
                mExtrusionData.clear();
                mExtrusionBlackData.clear();
            }
        }
        void ManeuverRenderer::Render(boost::optional<std::pair<vec2, float> > shadow)
        {
            if (!mArrowData.points.empty())
            {
                // vehicle intersection checking
                if (shadow && (mArrowCenter - shadow->first).Length() < mArrowSize + shadow->second)
                    return;

                renderer_->ChangeVertexFormat(vertex_format_);
                renderer_->ChangeVertexBuffer(NULL);
                renderer_->ChangeIndexBuffer(NULL);

                shader_->Bind();
                shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());

                renderer_->ClearStencil(3); // 1 & 2 arrow's extrusions
                renderer_->ClearStencilBuffer();

                renderer_->EnableStencilTest();
                renderer_->StencilBaseFunction();

                // arrow
                RenderArrow(mArrowData);

                // black extrusion
                renderer_->StencilHighlightFunction(1);
                RenderArrow(mExtrusionBlackData);

                // white extrusion
                renderer_->StencilHighlightFunction(2);
                RenderArrow(mExtrusionData);

                renderer_->DisableStencilTest();

                shader_->Unbind();
            }
        }
        bool ManeuverRenderer::exists() const
        {
            return mArrowData.points.size() > 0;
        }
        boost::optional<std::pair<vec2, float> > ManeuverRenderer::shadow() const
        {
            if (exists())
            {
              return std::make_pair(mArrowCenter, mArrowSize);
            }
            else
            {
              return boost::none;
            }
        }
        void ManeuverRenderer::FillBuffers(mgnMdTerrainProvider * provider, const std::vector<GeoRoutePoint> routePoints)
        {
            mArrowData.clear();
            mExtrusionData.clear();
            mExtrusionBlackData.clear();

            if (terrain_view_->getMagIndex() >= 5)
                return;

            gmu::inner::primitive::kind const& (*part_kind)(boost::tuples::cons<BufferOps::arrow_data::arrow_part::head_type, BufferOps::arrow_data::arrow_part::tail_type> const&) = boost::get<0, BufferOps::arrow_data::arrow_part::head_type, BufferOps::arrow_data::arrow_part::tail_type>;

            std::vector<vec3> local_points;
            boost::transform(routePoints, std::back_inserter(local_points), GeoRoutePoint2Local(terrain_view_));
            float arrowWidth = mScaleKf*1.2, arrowHeight = 0.6*mScaleKf, extrBlackKf = 0.1, extrWhiteKf = 0.15;

            {
                std::vector<vec3> points;
                vec_point_store store(points);
                gmu::stripe(local_points.begin(), local_points.end(), arrowWidth, store);
                update_store_z(terrain_view_, provider, store);

                BufferOps::fill_buffers(points, store.get_parts(), store.get_left_edge(), store.get_right_edge(), arrowHeight, true, mArrowData);
                BOOST_FOREACH(BufferOps::arrow_data::arrow_part& part, mArrowData.parts)
                {
                  if (boost::get<0>(part) == gmu::inner::primitive::Lines || boost::get<0>(part) == gmu::inner::primitive::LineLoop || boost::get<0>(part) == gmu::inner::primitive::LineStrip)
                  {
                    boost::copy(cFrameColor, boost::get<2>(part).begin());
                  }
                  else
                  {
                    boost::copy(cFaceColor, boost::get<2>(part).begin());
                  }
                }
            }

            // first interval prolongation
            vec3 endpoint = gmu::continued(local_points[1], local_points.front(), mScaleKf*extrBlackKf);
            endpoint.y = local_points.front().y;
            local_points.front() = endpoint;

            /*
            vec3 endpoint = gmu::continued(local_points[local_points.size() - 2], local_points.back(), mScaleKf*extrBlackKf);
            endpoint.y = local_points.back().y;
            local_points.back() = endpoint;
            */

            {
                std::vector<vec3> points;
                vec_point_store store(points);
                gmu::stripe(local_points.begin(), local_points.end(), arrowWidth*(1.0 + extrBlackKf), store);
                update_store_z(terrain_view_, provider, store);

                BufferOps::fill_buffers(points, store.get_parts(), store.get_left_edge(), store.get_right_edge(), arrowHeight*(1.0 + extrBlackKf), true, mExtrusionBlackData);
                BOOST_FOREACH(BufferOps::arrow_data::arrow_part& part, mExtrusionData.parts)
                {
                  boost::copy(cFrameColor, boost::get<2>(part).begin());
                }
                }

                {
                std::vector<vec3> points;
                vec_point_store store(points);
                gmu::stripe(local_points.begin(), local_points.end(), arrowWidth*(1.0f + extrBlackKf  + extrWhiteKf), store);
                update_store_z(terrain_view_, provider, store);

                BufferOps::fill_buffers(points, store.get_parts(), store.get_left_edge(), store.get_right_edge(), arrowHeight*(1.0f + extrBlackKf  + extrWhiteKf), true, mExtrusionData);
                BOOST_FOREACH(BufferOps::arrow_data::arrow_part& part, mExtrusionData.parts)
                {
                  boost::get<2>(part).fill(1.0);
                }
            }
        }
        void ManeuverRenderer::RenderArrow(const BufferOps::arrow_data& arrowData)
        {
            BOOST_FOREACH(const BufferOps::arrow_data::arrow_part& part, arrowData.parts)
            {
                boost::unordered_map<gmu::inner::primitive::kind, graphics::PrimitiveType::T>::const_iterator part_iter = mPTypeMap.find(boost::get<0>(part));
                if (part_iter == mPTypeMap.end())
                    continue;

                shader_->Uniform4f("u_color", boost::get<2>(part)[0], boost::get<2>(part)[1], boost::get<2>(part)[2], 1.0f);
                const graphics::PrimitiveType::T mode = part_iter->second;
                const std::vector<short int>& indexes = boost::get<1>(part);
                renderer_->context()->VertexAttribPointer(0, 3, graphics::DataType::kFloat, 3 * sizeof(float), &arrowData.points[0]);
                renderer_->context()->DrawElements(mode, (unsigned int)indexes.size(), graphics::DataType::kUnsignedShort, &indexes[0]);
            }
        }

    } // namespace terrain
} // namespace mgn