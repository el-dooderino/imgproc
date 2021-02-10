/*
 * Copyright (c) 2019+ Gene Savchuk as an unpublished work.
 * All rights reserved.
 *
 * The information contained herein is confidential property of
 * Gene Savchuk. The use, copying, transfer or disclosure of such
 * information is prohibited except by express written agreement with
 * Gene Savchuk.
 */

#include <catch2/catch.hpp>

#include "point.hh"

TEST_CASE( "point_hash", "hash uniqueness" ) {
    std::unordered_map<size_t, int> hashes;

    for (u_int x{}; x < 10'000; ++x) {
        for (u_int y{}; y < 10'000; ++y) {
            CHECK(++hashes[point_hash{}({x,y})] < 3);
        }
    }
}
