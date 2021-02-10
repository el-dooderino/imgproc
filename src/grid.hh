
#pragma once

#include "point.hh"

#include <tuple>

template<u_int W, u_int H>
struct grid_t {
    grid_t(u_int w, u_int h): m_w{w}, m_h{h} {}
    point_t operator()(const point_t &p) const noexcept { return {(p.x * W) / m_w, (p.y * H) / m_h}; }
    const u_int m_w, m_h;
};
