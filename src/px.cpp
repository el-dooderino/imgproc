
#include "px.hh"

#include "tags.hh"

#include <cmath>

namespace {
// sRGB luminance(Y) values
constexpr const double rY = 0.212655;
constexpr const double gY = 0.715158;
constexpr const double bY = 0.072187;

// Inverse of sRGB "gamma" function. (approx 2.2)
double inv_gam_sRGB(int ic) {
    double c = ic / 255.0;
    if (c <= 0.04045) return c / 12.92;
    return pow(((c + 0.055) / (1.055)), 2.4);
}

// sRGB "gamma" function (approx 2.2)
int gam_sRGB(double v) {
    if (v <= 0.0031308) v *= 12.92;
    else v = 1.055 * pow(v, 1.0 / 2.4) - 0.055;
    return static_cast<int>(round(v * 255));
}

// GRAY VALUE ("brightness")
int gray(int r, int g, int b) {
    return gam_sRGB(
            rY * inv_gam_sRGB(r) +
            gY * inv_gam_sRGB(g) +
            bY * inv_gam_sRGB(b)
    );
}

}

px_t::px_t(const gvs::json::val &v): m_r(v[tag::r].asInt()), m_g(v[tag::g].asInt()), m_b(v[tag::b].asInt()) {}

gvs::json::val px_t::to_json() const {
    return gvs::json::val{gvs::tval{tag::r, m_r}, gvs::tval{tag::g, m_g}, gvs::tval{tag::b, m_b} };
}

