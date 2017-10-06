#ifndef ARROW_BUILDER_H
#define ARROW_BUILDER_H

#include <vector>
#include <list>

#include <boost/tuple/tuple.hpp>
#include <boost/array.hpp>

#include "LineGeometry.h"
#include "StripeAlgorithm.h"
#include "MapDrawing/Graphics/mgnVector.h"

namespace BufferOps
{
  struct arrow_data
  {
    typedef boost::tuple<gmu::inner::primitive::kind, std::vector<short int>, boost::array<float, 4> > arrow_part;

    std::vector<vec3> points;
    std::vector<arrow_part> parts;

    void clear()
    {
      parts.clear();
      points.clear();
    }

    arrow_part& add_part(gmu::inner::primitive::kind kind)
    {
      parts.resize(parts.size() + 1);
      arrow_part& n_part = parts.back();
      boost::get<0>(n_part) = kind;
      return n_part;
    }
  };

  void fill_buffers(const std::vector<vec3>& points, const std::list<std::pair<gmu::inner::primitive::kind, std::vector<std::size_t> > > parts, const std::vector<std::size_t>& left_edge, const std::vector<std::size_t>& right_edge, float h, bool countur_enabled, arrow_data& result);
}

#endif
