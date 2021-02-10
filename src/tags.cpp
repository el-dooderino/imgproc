
#include "tags.hh"

namespace tag {
#define _(X) const std::string X{#X};
TAG_LIST
#undef _
}
