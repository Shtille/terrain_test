#include "ArrowBuilder.h"

#include <cmath>
#include <utility>
#include <algorithm>

#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/assign/std/vector.hpp>

#include "LineGeometry.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gmu
{
  template<>
  struct point_traits<vec3>
  {
    typedef float coordinate_type;
  };

  template<>
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

  template<>
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

namespace BufferOps
{
  void fill_buffers(const std::vector<vec3>& points, const std::list<std::pair<gmu::inner::primitive::kind, std::vector<std::size_t> > > parts, const std::vector<std::size_t>& left_edge, const std::vector<std::size_t>& right_edge, float h, bool countur_enabled, arrow_data& result)
  {
    using namespace boost::assign;
    typedef std::pair<gmu::inner::primitive::kind, std::vector<std::size_t> > part_type;

    // first N points placed on ground
    std::copy(points.begin(), points.end(), std::back_inserter(result.points));
    std::size_t N = points.size();
    // next N points higher on h
    std::copy(points.begin(), points.end(), std::back_inserter(result.points));
    std::for_each(result.points.begin() + N, result.points.end(), boost::lambda::bind(&vec3::y, boost::lambda::_1) += h);

    // for each part indexes shifts to upper points
    BOOST_FOREACH(const part_type& part, parts)
    {
      arrow_data::arrow_part& n_part = result.add_part(part.first);
      boost::transform(part.second, std::back_inserter(boost::get<1>(n_part)), boost::lambda::_1 + N);
    }

    // building the "wall"
    arrow_data::arrow_part& wall_part = result.add_part(gmu::inner::primitive::TriangleStrip);
    BOOST_FOREACH(std::size_t idx, right_edge)
    {
      boost::get<1>(wall_part) += idx + N, idx;
    }
    BOOST_FOREACH(std::size_t idx, left_edge | boost::adaptors::reversed)
    {
      boost::get<1>(wall_part) += idx + N, idx;
    }
    boost::get<1>(wall_part) += right_edge.front() + N, right_edge.front();

    if (countur_enabled)
    {
      // countur for upper points
      arrow_data::arrow_part& upper_contour_part = result.add_part(gmu::inner::primitive::LineStrip);
      boost::copy(left_edge | boost::adaptors::reversed | boost::adaptors::transformed(boost::lambda::_1 + N), std::back_inserter(boost::get<1>(upper_contour_part)));
      boost::copy(right_edge | boost::adaptors::transformed(boost::lambda::_1 + N), std::back_inserter(boost::get<1>(upper_contour_part)));

      // countur for lower points
      arrow_data::arrow_part& lower_contour_part = result.add_part(gmu::inner::primitive::LineStrip);
      boost::copy(left_edge | boost::adaptors::reversed, std::back_inserter(boost::get<1>(lower_contour_part)));
      boost::copy(right_edge, std::back_inserter(boost::get<1>(lower_contour_part)));

      // vertical egges
      boost::get<1>(result.add_part(gmu::inner::primitive::Lines)) += left_edge.front(), left_edge.front() + N, left_edge.back(), left_edge.back() + N, right_edge.back(), right_edge.back() + N, right_edge.front(), right_edge.front() + N;
    }

    // building arrow
    vec3 arrow_base;
    arrow_base.x = (points[left_edge.back()].x + points[right_edge.back()].x)/2.0;
    arrow_base.y = (points[left_edge.back()].y + points[right_edge.back()].y)/2.0;
    arrow_base.z = (points[left_edge.back()].z + points[right_edge.back()].z)/2.0;
    vec3 left_wing = gmu::inner::mix(arrow_base, points[left_edge.back()], 1.6);
    left_wing.y = arrow_base.y;
    vec3 right_wing = gmu::inner::mix(arrow_base, points[right_edge.back()], 1.6);
    right_wing.y = arrow_base.y;

    double arrow_angle = std::atan2(left_wing.z - right_wing.z, left_wing.x - right_wing.x) - M_PI/2;
    double arrow_length = std::sqrt(gmu::inner::q_distance(left_wing, right_wing));
    vec3 arrow_head = vec3(arrow_base.x + std::cos(arrow_angle)*arrow_length, arrow_base.y, arrow_base.z + std::sin(arrow_angle)*arrow_length);
    // arrowhead points
    result.points += left_wing, arrow_head, right_wing, left_wing, arrow_head, right_wing;
    // for upper points increase y
    std::for_each(result.points.begin() + N*2 + 3, result.points.begin() + N*2 + 6, boost::lambda::bind(&vec3::y, boost::lambda::_1) += h);

    // arrowhead triangle
    boost::get<1>(result.add_part(gmu::inner::primitive::Triangles)) += N*2 + 5, N*2 + 4, N*2 + 3;

    // arrowhead wall
    boost::get<1>(result.add_part(gmu::inner::primitive::TriangleStrip)) += N*2 + 5, N*2 + 2, N*2 + 4, N*2 + 1, N*2 + 3, N*2, N*2 + 5, N*2 + 2;

    // arrowhead countur
    if (countur_enabled)
    {
      boost::get<1>(result.add_part(gmu::inner::primitive::LineStrip)) += N*2 - 1, N*2 + 5, N*2 + 4, N*2 + 3, N*2 - 2;
      boost::get<1>(result.add_part(gmu::inner::primitive::LineStrip)) += N - 1, N*2 + 2, N*2 + 1, N*2, N - 2;
      boost::get<1>(result.add_part(gmu::inner::primitive::Lines)) += N*2 + 3, N*2, N*2 + 4, N*2 + 1, N*2 + 5, N*2 + 2;
    }
  }
}

