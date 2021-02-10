
#pragma once

#include <gvs_exception.hh>
#include <gvs_json.hh>

#include <sys/types.h>

struct px_t {
    friend struct px_hash;
    using color_type = int;
    static constexpr const int num_fields{3};

    px_t(const uint8_t *p, u_int sz) {
        switch (sz) {
            case 3:
                m_r = p[0];
                m_g = p[1];
                m_b = p[2];
                break;

            case 1:
                m_r = m_g = m_b = p[0];
                break;

            default:
                throw gvs::exception{"invalid pixel dimention %d", sz};
        }
    }

    px_t(const gvs::json::val &v);

    px_t(int r, int g, int b): m_r{r}, m_g{g}, m_b{b} {};

    [[nodiscard]] int lum() const noexcept { return (m_r + m_r + m_r + m_b + m_g + m_g + m_g + m_g) >> 3; }
    [[nodiscard]] int avg() const noexcept { return (m_r + m_g + m_b) / 3; }

    friend inline bool constexpr operator==(const px_t &a, const px_t &b) noexcept {
        static_assert(px_t::num_fields == 3);
        return a.m_r == b.m_r && a.m_g == b.m_g && a.m_b == b.m_b;
    }

    [[nodiscard]] gvs::json::val to_json() const;

    [[nodiscard]] auto r() const noexcept {return m_r;}
    [[nodiscard]] auto g() const noexcept {return m_g;}
    [[nodiscard]] auto b() const noexcept {return m_b;}

    template<int O, int N, int D>
    static px_t mult(const px_t &px) noexcept {
        return {(px.m_r + O) * N / D, (px.m_g + O) * N / D, (px.m_b + O) * N / D};
    }

private:
    color_type m_r, m_g, m_b;
};

struct px_hash {
    static_assert(px_t::num_fields == 3);
    inline constexpr size_t operator()(const px_t &g) const noexcept {
        return (g.m_r << 16) | (g.m_g << 8) | g.m_b;
    }
};


