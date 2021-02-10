
#pragma once

#include <list>
#include <string>

struct opts {
    std::list<std::string> dirs;
};

opts procargs(int argc, char **argv);
