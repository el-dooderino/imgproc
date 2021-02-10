
#pragma once

#include "bmp.hh"
#include "point.hh"

#include <optional>
#include <string>

std::optional<bmp_t> read_img(const std::string &fn);

std::optional<point_t> read_img_header(const std::string &fn);

