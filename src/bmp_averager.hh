
#pragma once

#include "point.hh"
#include "px.hh"

#include <cmath>
#include <unordered_map>

template <typename T>
struct avg_t {
    template<typename U>
    avg_t &operator+=(const U& in) {
        m_val += static_cast<decltype(m_val)>(in);
        ++m_cnt;
        return *this;
    }

//    operator T() const {
//        if (!m_cnt) throw std::runtime_error{"divide by 0"};
//        if constexpr (std::is_integral_v<T>) {
//            return std::nearbyint(m_val / m_cnt);
//        } else {
//            return static_cast<T>(m_val / m_cnt);
//        }
//    }

    T operator()() const {
        if (!m_cnt) throw std::runtime_error{"divide by 0"};
        if constexpr (std::is_integral_v<T>) {
            return std::round(m_val / m_cnt);
        } else {
            return static_cast<T>(std::round(m_val / m_cnt));
        }
    }

private:
    double m_val{};
    size_t m_cnt{};
};

struct bmp_averager_t {
    struct avgs {
        avg_t<px_t::color_type> r, g, b;
        px_t operator()() const { return {r(), g(), b()}; }

        auto &operator+=(const px_t &ex) {
            r += ex.r();
            g += ex.g();
            b += ex.b();
            return *this;
        }
    };

//    auto &vals() { return m_vals; }

    const auto &vals() const { return m_vals; }

    auto &val(const point_t &p) { return m_vals[p]; }

private:

    std::unordered_map<point_t, avgs, point_hash> m_vals; //values in the grid, [x][y]
};
