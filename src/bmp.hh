
#pragma once

#include "px.hh"

#include <gvs_dynbuf.hh>

#include <cstdint>
#include <optional>
#include <tuple>

#include <sys/types.h>

struct bmp_t {
    bmp_t() = default;

    bmp_t(u_int w, u_int h, u_int pxs = 3) { set(w, h, pxs); }

    [[nodiscard]] size_t bytes() const noexcept { return m_w * m_h * m_pxs; }

    void set(u_int w, u_int h, u_int pxs = 3) {
        m_w = w;
        m_h = h;
        m_pxs = pxs;
        m_row_stride = w * pxs;
        m_buf.setsize(0).reserve(w * h * pxs);
//        printf("%dx%d %db/p \n", w, h, pxs);
    }

    auto *data() noexcept { return m_buf.data(); }

    [[nodiscard]] auto px(u_int x, u_int y) const {
        if (x >= m_w) throw gvs::exception{"bad X %d", x};
        if (y >= m_h) throw gvs::exception{"bad Y %d", y};

        return px_t{m_buf.data() + y * m_row_stride + x * m_pxs, m_pxs};
    }

    std::optional<px_t> up(const uint8_t *p) const {
        const auto ret = p - m_row_stride;
        if (ret >= m_buf.data()) return px_t{ret, m_pxs};
        return {};
    }

    std::optional<px_t> down(const uint8_t *p) const {
        const auto ret = p + m_row_stride;
        if (ret <= &m_buf.back()) return px_t{ret, m_pxs};
        return {};
    }

    std::optional<px_t> left(const uint8_t *p) const {
        const auto ret = p - m_pxs;
        if (ret >= m_buf.data()) return px_t{ret, m_pxs};
        return {};
    }

    std::optional<px_t> right(const uint8_t *p) const {
        const auto ret = p + m_pxs;
        if (ret <= &m_buf.back()) return px_t{ret, m_pxs};
        return {};
    }

    // a row
    auto *row(int y) noexcept { return m_buf.data() + y * m_row_stride; }

    [[nodiscard]] auto dims() const { return std::make_tuple(m_w, m_h); }

private:
    u_int m_h{}, m_w{}, m_pxs{};
    u_int m_row_stride{};
    gvs::dynbuf<uint8_t, 1> m_buf;
};
