
#pragma once

#include <gvs_json.hh>

#include <sys/types.h>

struct point_t {
    point_t(u_int x, u_int y): x{x}, y{y} {}
    point_t(const gvs::json::val &v);
    [[nodiscard]] gvs::json::val to_json() const;
    u_int x, y;
};

inline constexpr bool operator==(const point_t &a, const point_t &b) noexcept {
    return a.x == b.x & a.y == b.y;
};

struct point_hash {
    constexpr size_t operator()(const point_t &e) const noexcept {
        return (e.x << 16) | e.y;
    };
};
