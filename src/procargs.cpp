
#include "procargs.hh"

#include <gvs_exception.hh>

#include <getopt.h>

namespace {
void usage() {
    throw gvs::exception(
            R"(
Usage:
   imgproc folder [folder]
)"
    );
}

}

opts procargs(int argc, char **argv) {
    opts ret;

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
                {nullptr, 0,              nullptr, 0}
        };

        int c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
//            case 'D':
//                ret.delete_bad_files = true;
//                break;

            case '?': break;

            default: throw gvs::exception{"?? getopt returned character code 0%o ??\n", c};
        }
    }

    if (optind < argc) {
        while (optind < argc) ret.dirs.emplace_back(argv[optind++]);
    }

    if (ret.dirs.empty()) usage();

    return ret;
}
