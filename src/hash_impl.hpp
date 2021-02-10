
#pragma once

namespace {

template<typename T>
constexpr T rot(T x, T k) noexcept {
    return (x << k) | (x >> (sizeof(T) * 8 - k));
}

template<typename T>
constexpr void mix(T &a, T &b, T &c) noexcept {
    a -= c;
    a ^= rot(c, 4U);
    c += b;
    b -= a;
    b ^= rot(a, 6U);
    a += c;
    c -= b;
    c ^= rot(b, 8U);
    b += a;
    a -= c;
    a ^= rot(c, 16U);
    c += b;
    b -= a;
    b ^= rot(a, 19U);
    a += c;
    c -= b;
    c ^= rot(b, 4U);
    b += a;
}

template<typename T>
constexpr void final(T &a, T &b, T &c) noexcept {
    c ^= b;
    c -= rot(b, 14U);
    a ^= c;
    a -= rot(c, 11U);
    b ^= a;
    b -= rot(a, 25U);
    c ^= b;
    c -= rot(b, 16U);
    a ^= c;
    a -= rot(c, 4U);
    b ^= a;
    b -= rot(a, 14U);
    c ^= b;
    c -= rot(b, 24U);
}

/// k: the key, an array of uint32_t values
/// length: the length of the key, in uint32_ts
/// the previous hash, or an arbitrary value
uint32_t hashword(const uint32_t *k,  size_t length,  uint32_t initval) {
    uint32_t a, b, c;

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (((uint32_t) length) << 2) + initval;

    /*------------------------------------------------- handle most of the key */
    while (length > 3) {
        a += k[0];
        b += k[1];
        c += k[2];
        mix(a, b, c);
        length -= 3;
        k += 3;
    }

    /*------------------------------------------- handle the last 3 uint32_t's */
    /* all the case statements fall through */
    switch (length) {
        case 3 :
            c += k[2];
        case 2 :
            b += k[1];
        case 1 :
            a += k[0];
            final(a, b, c);
        case 0:     /* case 0: nothing left to add */
            break;
    }
    /*------------------------------------------------------ report the result */
    return c;
}

}
