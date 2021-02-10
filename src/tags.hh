
#pragma once

#include <string>

#define TAG_LIST \
_(x)             \
_(y)             \
_(r)             \
_(g)             \
_(b)             \
_(extract)       \
_(hash)          \
_(files)         \
_(point)         \
_(vals)          \
_(grid)          \
_(timestamp)     \


namespace tag {

#define _(X) const extern std::string X;
TAG_LIST
#undef _

}
