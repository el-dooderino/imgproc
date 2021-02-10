/*
 * Copyright (c) 2020 Gene Savchuk as an unpublished work.
 * All rights reserved.
 *
 * The information contained herein is confidential property of
 * Gene Savchuk. The use, copying, transfer or disclosure of such
 * information is prohibited except by express written agreement with
 * Gene Savchuk.
 */

#pragma once

#include "bmp.hh"
#include "point.hh"

#include <optional>
#include <string>

std::optional<bmp_t> read_img(const std::string &fn);

std::optional<point_t> read_img_header(const std::string &fn);

