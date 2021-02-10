/*
 * Copyright (c) 2020 Gene Savchuk as an unpublished work.
 * All rights reserved.
 *
 * The information contained herein is confidential property of
 * Gene Savchuk. The use, copying, transfer or disclosure of such
 * information is prohibited except by express written agreement with
 * Gene Savchuk.
 */

#include "globals.hh"
#include "procargs.hh"
#include "tags.hh"
#include "user_interactive.hh"
#include "worker_thread.hh"

#include <gvs_exec.hh>
#include <gvs_json.hh>
#include <gvs_scandir.hh>
#include <gvs_timer.hh>
#include <gvs_utils.hh>
#include <jsonio/from_file.hh>
#include <jsonio/to_file.hh>

#include <cstdio>
#include <deque>
#include <iostream>
#include <thread>

#include <unistd.h>

namespace {

template <size_t N>
struct progress_print {
    progress_print() = default;
    progress_print(size_t sz): sz{sz} {}

    void operator()() {
        if (sz == 0 || sz > N) {
            if (++current % N == 0) std::cerr << current << " ";
        }
    }

    void done() const {
        if (sz == 0) {
            std::cerr << current << "\n";
        } else {
            if (sz > N) std::cerr << current << "\n";
        }
    }

    [[nodiscard]] auto size() const { return sz; }

private:
    const size_t sz{};
    size_t current{};
};

typedef std::list<g::string_ptr> file_list_t;

template <typename F>
void scan_file_tree(std::string path, const F &cb) {
    try {
        switch (gvs::utl::statx(path).st_mode & S_IFMT) {
            case S_IFREG:
                cb(std::move(path));
                break;
            case S_IFDIR:
                if (path.back() != '/') path.push_back('/');
                for (const auto &dx: gvs::dir{path}) {
                    if (dx.is_those_dots()) continue;
                    auto fn = path + dx.d_name;
                    if (dx.is_reg()) {
                        reg_again:
                        cb(std::move(fn));
                    } else if (dx.is_dir()) {
                        dir_again:
                        scan_file_tree(std::move(fn), cb);
                    } else if (dx.is_unk()) {
                        try {
                            switch (gvs::utl::statx(fn).st_mode & S_IFMT) {
                                case S_IFREG: goto reg_again;
                                case S_IFDIR: goto dir_again;
                            }
                        } catch (const std::exception &ex) {
                            printf("%s: %s", fn.c_str(), ex.what());
                        }
                    } else {
                        printf("?%s?, %d\n", dx.d_name, dx.d_type);
                    }
                }
                break;
        }
    } catch (const std::exception &ex) {
        printf("%s: %s", path.c_str(), ex.what());
    }
}

void rem_duplicate_input_files(file_list_t &list) {
    std::map<int, std::set<decltype(std::declval<stat>().st_ino)>> inodes;
    int duplicates{}, errors{};
    progress_print<1000> pp{list.size()};
    for (auto it = list.begin(); it != list.end();) {
        pp();
        try {
            auto st = gvs::utl::statx(**it);
            if (!inodes[st.st_dev].emplace(st.st_ino).second) {
                ++duplicates;
                it = list.erase(it);
            } else {
                std::advance(it, 1);
            }
        } catch (...) {
            ++errors;
            it = list.erase(it);
        }
    }
    pp.done();

    if (duplicates || errors) {
        std::cout << "Removed " << duplicates << " duplicates with " << errors << " errors" << std::endl;
    }
}

void calc_hashes(const file_list_t &list) {
    using namespace std::chrono_literals;
    printf("Calculating hashes...\n");

    progress_print<100> pp{list.size()};
    for (const auto &f: list) {
        g::file_queue.push(f);
        pp();
    }
    pp.done();
    g::do_hash = false;
    while (g::worker_state.r([](const auto &z) { return z.hashing != 0; })) std::this_thread::sleep_for(0.1s);

    g::hash_avgs.r([](const auto &z) {
        const auto ddur = std::chrono::duration<double>(z.dur).count();
        printf("Hashed %ld file(s) in %.1fs, %.1fps, (re)calculated %ld\n", z.cnt, ddur, (z.cnt / ddur), z.recalc);
    });
}

void process(const file_list_t &list) {
    using namespace std::chrono_literals;
    printf("Processing...\n");

    progress_print<100> pp{list.size()};
    for (const auto &f: list) {
        g::file_queue.push(f);
        pp();
    }
    pp.done();
    g::do_process = false;
    while (g::worker_state.r([](const auto &z) { return z.processing != 0; })) std::this_thread::sleep_for(0.1s);

    g::read_avgs.r([](const auto &z) {
        const auto ddur = std::chrono::duration<double>(z.dur).count();
        const auto sz = static_cast<double>(z.sz);
        printf("Read %ld file(s) in %.1fs, %.1fGiBs, %.1fMiBps, %.1fps\n", z.cnt, ddur, (sz / 1024. / 1024. / 1024.), (sz / ddur / 1024. / 1024.), (z.cnt / ddur));
    });
    g::bmp_avgs.r([](const auto &z) {
        const auto ddur = std::chrono::duration<double>(z.dur).count();
        const auto sz = static_cast<double>(z.sz);
        printf("Bmp'd %ld file(s) in %.1fs, %.1fGiBs, %.1fMiBps, %.1fps\n", z.cnt, ddur, (sz / 1024. / 1024. / 1024.), (sz / ddur / 1024. / 1024.), (z.cnt / ddur));
    });
    g::proc_avgs.r([](const auto &z) {
        const auto ddur = std::chrono::duration<double>(z.dur).count();
        const auto sz = static_cast<double>(z.sz);
        printf("Proc'd %ld file(s), (re)calculated %ld file(s) in %.1fs, %.1fGiBs, %.1fMiBps, %.1fps\n", z.cnt, z.recalc, ddur, (sz / 1024. / 1024. / 1024.), (sz / ddur / 1024. / 1024.), (z.cnt / ddur));
    });
}

void lookups() {
    using namespace std::chrono_literals;
    printf("Matching...\n");
    auto gridlist = g::file2grids.w([](auto &z) { return std::move(z); });
    progress_print<1000> pp{gridlist.size()};
    gvs::timer timer;
    while (!gridlist.empty()) {
        g::fileandgrid_queue.push(std::move(gridlist.front()));
        gridlist.pop_front();
        pp();
    }
    pp.done();

    g::do_match = false;
    while (g::worker_state.r([](const auto &z) { return z.matching != 0; })) std::this_thread::sleep_for(.1s);
    printf("Done running lookups in %.2fs, %.1fps\n", timer.measure<float>(), pp.size() / timer.measure<double>());
}

void maybe_load_db(const char *filename) {
    g::database.w([filename](auto &z) {
        try {
            printf("Loading database...\n");
            gvs::timer timer;
            auto zz = gvs::jsonio::from_file(filename);
            if (!zz[tag::grid].let([](const auto &z) {
                return g::GRID_H == z[tag::y].asInt() && g::GRID_W == z[tag::x].asInt();
            })) {
                printf("Stale database ignored\n");
                return;
            }
            z = std::move(zz);
            printf("Database loaded in %.02fs\n", timer.measure<double>());
        } catch (const std::exception &ex) {
            printf("%s\n", ex.what());
        }
    });
}

auto re_cluster() {
    auto duplist = g::duplicates.w([](auto &list) { return std::move(list); });
    std::cout << "re-cluster\ny/n?: ";
    std::string input;
    std::getline(std::cin, input);
    if (input.size() == 1 and input[0] == 'y') {
        std::list<std::set<g::string_ptr>> ret;
        // we don't need protection anymore - steal the duplist;
        printf("re-clustering\n");
        for (auto &dlg: duplist) { // per duplist group
            std::set<std::shared_ptr<std::string>> *cl_p{};
            for (auto &fn: dlg) { // per file in duplist group
                for (auto &cl: ret) { // See if this file already in some cluster
                    if (gvs::utl::container_contains(cl, fn)) {
                        if (!cl_p) cl_p = &cl; // if one file is already there - all files from this duplist group should get there.
                        goto next_fn; // see if this file is already in the cluster
                    }
                }
                // create a cluster if necessary
                if (!cl_p) cl_p = &ret.emplace_back(std::set<std::shared_ptr<std::string>>{});
                cl_p->emplace(fn); // add to a cluster

                next_fn:;
            }
        }
        printf("duplists: %ld, clusters: %ld\n", duplist.size(), ret.size());
        return ret;
    }

    return duplist;
}

}

int main(int argc, char **argv) {
    using namespace std::literals;
    std::list<std::thread> threads;

    file_list_t all_files;

    gvs::exec executor{};

    try {
        const auto args = procargs(argc, argv);

        maybe_load_db("/home/gvs/database");

        // the first "duplicate" set is for luminocity stuff
        g::duplicates.w([](auto &list) { list.emplace_back(); });
        for (int i{}; i < 8; ++i) threads.emplace_back(worker);

        {
            progress_print<1000> pp;
            for (const auto &dir: args.dirs) {
                printf("Scanning '%s'\n", dir.c_str());
                scan_file_tree(dir, [&pp, &all_files](std::string &&fn) {
                    all_files.push_back(std::make_shared<std::string>(std::move(fn)));
                    pp();
                });
                pp.done();
            }
        }
        printf("Checking for input uniqueness\n");
        rem_duplicate_input_files(all_files);
        printf("Found %ld files\n", all_files.size());

        calc_hashes(all_files);

        process(all_files);

        if (g::db_recalcs > 0) {
            printf("saving database...\n");
            gvs::timer timer;
            g::database.w([](auto &z) {
                z[tag::grid] = gvs::json::val{gvs::tval{tag::y, g::GRID_H}, gvs::tval{tag::x, g::GRID_W}};
                gvs::jsonio::to_file("/home/gvs/database", 0644, z);
                z.clear();
            });
            printf("database saved in %0.2fs\n", timer.measure<double>());
        }
        all_files.clear();

        lookups();

        for (auto &t: threads) if (t.joinable()) t.join();

        g::grid2files.w([](auto &z) { z.clear(); });

        deal_with_duplicates(executor, g::bad_files.w([](auto &z) { return std::move(z); }), re_cluster());
    }

    catch (const std::exception &ex) {
        fprintf(stderr, "Error %s\n", ex.what());
    }

    return 0;
}
