
#include "point.hh"
#include "tags.hh"

point_t::point_t(const gvs::json::val &v): x(v[tag::x].asInt()), y(v[tag::y].asInt()) {}

gvs::json::val point_t::to_json() const {
    return {gvs::tval{tag::x, x}, gvs::tval{tag::y, y}};
}
