
#include "user_interactive.hh"

#include "globals.hh"
#include "point.hh"
#include "utils.hh"

#include <gvs_exec_task.hh>
#include <gvs_str.hh>
#include <gvs_utils.hh>

#include <cstring>
#include <functional>
#include <iostream>

#include <unistd.h>

namespace {

struct finfo_t {
    finfo_t(g::string_ptr fn, size_t fs, point_t point): name{std::move(fn)}, size{fs}, point{point} {}
    g::string_ptr name;
    size_t size;
    point_t point;
};

using dupfiles_t = std::vector<finfo_t>;
using fields_t = std::vector<std::string>;

// to have a std::function returning std::function we need this...
template<typename... T>
struct recursive_func_t {
    typedef std::function<recursive_func_t(T...)> type;

    template<typename F>
    recursive_func_t(F &&f) : func(std::forward<F>(f)) {}

    // with forwarding main constructor there's a claim we don't need those...
    recursive_func_t(const recursive_func_t&) = delete;
    recursive_func_t(recursive_func_t&&) = delete;

    operator type() { return func; }
    type func;
};

// so we have this defined...
using action_t = recursive_func_t<gvs::exec &, dupfiles_t &>::type;

fields_t split_input(const std::string &input) {
   return gvs::str::split<std::vector<std::string>>(input, {' ', '\n'}, gvs::str::skip_empty);
}

void user_action_r(std::string input, dupfiles_t &dupfiles) {
    using namespace std::string_literals;
    const auto fields = split_input(input);
    if (fields.size() < 3) return;
    if (gvs::utl::same_file(fields[1], fields[2])) return;
    const auto from = std::stoi(fields[1]);
    const auto to = std::stoi(fields[2]);
    if (from < 0 || from >= dupfiles.size() || to < 0 || to >= dupfiles.size()) return;
    const auto &ff = *dupfiles[from].name;
    const auto &tf = *dupfiles[to].name;
    if (fields.size() < 4 || fields[3] != "y"s) {
        printf("rename '%s' to '%s'?\ny/n?: ", ff.c_str(), tf.c_str());
        std::getline(std::cin, input);
        if (input.size() != 1 || input[0] != 'y') return;
    }
    gvs::utl::copy_file(100, ff, tf);
    ::unlink(ff.c_str());
    dupfiles[to].size = dupfiles[from].size;
    dupfiles[to].point = dupfiles[from].point;
    dupfiles.erase(dupfiles.begin() + from);
}

action_t user_action_ads(std::string input, fields_t &fields, dupfiles_t &dupfiles) {
    const auto dirno = std::stoi(fields[1]);
    const auto max_cluster_size = (fields.size() > 2) ? std::stoi(fields[2]) : 100000;
    if (dirno < 0 || dirno >= dupfiles.size()) return nullptr;
    auto dirname = gvs::utl::dirname(*dupfiles[dirno].name);
    printf("autodelete outside '%s' same size max cluster sz %d?\ny/n?: ", dirname.c_str(), max_cluster_size);
    std::getline(std::cin, input);
    if (input.size() == 1 && input[0] == 'y') {
        return [msiz = max_cluster_size, dn = std::move(dirname)](auto &executor, auto &dupfiles) -> action_t {
            if (dupfiles.size() > msiz) return nullptr;

            size_t sz{}; // find size if it's unique
            for (const auto &f: dupfiles) {
                if (gvs::utl::dirname(*f.name) == dn) {
                    if (sz != 0) return nullptr;
                    sz = f.size;
                }
            }

            const double dsz = sz;
            auto [min_sz, max_sz] = std::make_tuple(.0, dsz + dsz * .1);

            for (const auto &f: dupfiles) {
                if (gvs::utl::dirname(*f.name) != dn && min_sz < f.size && f.size < max_sz) {
                    if (0 == ::unlink(f.name->c_str())) {
                        printf("delete '%s'\n", f.name->c_str());
                    }
                }
            }

            return nullptr;
        };
    }
    return nullptr;
}

void user_action_v(gvs::exec &executor, fields_t &fields, dupfiles_t &dupfiles) {
    using namespace std::string_literals;
    std::list<std::string> eargs{"/usr/bin/eom"s};

    if (fields.size() > 1) {
        for (const auto &f: fields) {
            try {
                int i = std::stoi(f);
                if (i >= 0 && i < dupfiles.size()) eargs.emplace_back(*dupfiles[i].name);
            } catch (...) {}
        }
    } else for (const auto &fn: dupfiles) eargs.emplace_back(*fn.name);
    const auto[st, o, e] = gvs::exec_task{executor(eargs)}();
}

const std::string help_line{R"(
x: exit
d # [#]: delete files in specified positions
da: delete all in this cluster
v: view group
ads folder# [max cluster sz#]: auto-delete outside of folder#, similar or smaller size
r from# to#: rename
c: continue
?: )"};

action_t user_action(gvs::exec &executor, dupfiles_t &dupfiles) {
    using namespace std::string_literals;

    while (true) {
        if (dupfiles.size() <= 1) return nullptr;

        std::cout << "- - - - - - - - - - - - -";
        int i{};
        for (const auto &fi: dupfiles) {
            printf("\n%d: '%s' %dx%d %.2f MiB", i++, fi.name->c_str(), fi.point.x, fi.point.y, static_cast<double>(fi.size) / 1024. / 1024.);
        }
        std::cout << "\n- - - - - - - - - - - - -" << help_line;

        std::string input;
        std::getline(std::cin, input);
        if (input.empty()) continue;
        auto fields = split_input(input);
        if (fields.empty()) continue;

        switch (fields[0][0]) {
            case 'x':
                throw 0;

            case 'c':
                return nullptr;

            case 'v':
                user_action_v(executor, fields, dupfiles);
                break;

            case 'r': {
                try {
                    user_action_r(input, dupfiles);
                } catch (...) {}
                break;
            }

            case 'a': {
                if (fields.size() >= 2 && fields[0] == "ads"s) {
                    try {
                        auto func = user_action_ads(input, fields, dupfiles);
                        if (func) {
                            // run on current set
                            func(executor, dupfiles);
                            return func;
                        }
                    } catch (...) {}
                }
                break;
            }

            case 'd': {
                if (fields.size() == 1 && fields[0] == "da"s) {
                    for (auto it = dupfiles.begin(); it != dupfiles.end();) {
                        if (0 == ::unlink(it->name->c_str())) it = dupfiles.erase(it);
                        else ++it;
                    }
                } else if (fields.size() > 1 && fields[0] == "d"s) {
                    std::set<g::string_ptr> to_delete; // collect all the files into this so duipfiles indeces don't change
                    for (auto fit = fields.cbegin() + 1; fit != fields.cend(); ++fit) {
                        try {
                            if (int num = std::stoi(*fit); num >= 0 && num < dupfiles.size()) to_delete.emplace(dupfiles[num].name);
                        } catch (...) {}
                    }
                    for (auto it = dupfiles.begin(); it != dupfiles.end(); ) {
                        if (gvs::utl::container_contains(to_delete, it->name) && 0 == ::unlink(it->name->c_str())) it = dupfiles.erase(it);
                        else ++it;
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    return nullptr;
};

};

int deal_with_duplicates(gvs::exec &executor, std::list<std::shared_ptr<std::string>> &&bad_files, std::list<std::set<std::shared_ptr<std::string>>> &&clusters) {
    using namespace std::string_literals;

    if (!bad_files.empty()) {
        bool work{true};
        while (work && !bad_files.empty()) {
            std::cout << "\n" << bad_files.size() << " bad files found\nl: list\nd: delete\nc: continue\n?: "s;
            std::string input;
            std::getline(std::cin, input);
            if (input.empty()) continue;
            switch (input[0]) {
                case 'l':
                    std::cout << "\n-------------\n";
                    for (const auto &f: bad_files) {
                        std::cout << "'" << *f << "'" << std::endl;
                    }
                    std::cout << "-------------\n";
                    break;
                case 'd': {
                    std::cout << "\n";
                    for (auto it = bad_files.begin(); it != bad_files.end();) {
                        const auto &fn = **it;
                        if (0 == ::unlink(fn.c_str())) {
                            std::cout << "Deleted '" << fn << "'\n";
                        } else {
                            std::cerr << "Error deleting '" << fn << "': " << strerror(errno) << "\n";
                        }
                        it = bad_files.erase(it);
                    }
                    break;
                }
                case 'c':
                    work = false;
                    break;
            }
        }
    }

    if (!clusters.empty()) {
        action_t curr_action = user_action;
        action_t next_action{};

        for (auto clit = clusters.begin(); clit != clusters.end();) {
            try {
                printf("\n%ld clusters(s) to go...\n", clusters.size());
                std::vector<finfo_t> dupfiles;
                for (const auto &fn: *clit) {
                    try {
                        auto point = read_img_header(*fn);
                        if (!point) dupfiles.emplace_back(fn, gvs::utl::statx(*fn).st_size, point_t{0, 0});
                        else dupfiles.emplace_back(fn, gvs::utl::statx(*fn).st_size, *point);
                    } catch (const std::exception &ex) {
                        std::cout << ex.what() << std::endl;
                    }
                }
                clit = clusters.erase(clit);

                next_action = curr_action(executor, dupfiles);
                if (next_action) {
                    curr_action = std::move(next_action);
                    next_action = nullptr;
                }

            } catch (const std::exception &ex) {
                std::cerr << ex.what() << "\n";
            } catch (const int &code) {
                return code;
            }
        }
    }

    std::cout << "\nAll done\n"s;

    return 0;
}
