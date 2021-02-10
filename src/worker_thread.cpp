
#include "worker_thread.hh"

#include "bmp_averager.hh"
#include "globals.hh"
#include "grid.hh"
#include "point.hh"
#include "tags.hh"
#include "utils.hh"

#include <gvs_hash.hh>
#include <gvs_json_time.hh>
#include <gvs_timer.hh>
#include <gvs_utils.hh>

#include <cmath>
#include <set>
#include <unordered_map>

namespace {

auto from_ts(struct timespec ts) noexcept {
    using namespace std::chrono;
    return seconds{ts.tv_sec} + nanoseconds{ts.tv_nsec} + system_clock::from_time_t(0);
}

void hash_loop() {
    using namespace std::chrono_literals;
    int recalculated{};

    while (true) {
        try {
            if (const auto fn = g::file_queue.get(.1s); !fn) {
                if (!g::do_hash) break;
            } else {
                try {
                    gvs::timer timer;
                    // check modification time first
                    auto mtime = gvs::json_time::tp2jv(from_ts(gvs::utl::statx(**fn).st_mtim));
                    if (!g::database.r([&fn, &mtime](const auto &z) {
                        const auto jvt = z[tag::files][**fn][tag::timestamp];
                        return (jvt && jvt.isStr() && jvt == mtime);
                    })) { // if problems - run hash
                        auto hash = gvs::file_sha256h(**fn);
                        g::database.w([&mtime, &recalculated, &fn, &hash](auto &z) {
                            auto &jv = z[tag::files][**fn];
                            auto &jvh = jv[tag::hash];
                            if (!jvh || !jvh.isStr() || jvh.asStr() != hash) {
                                jv.clear();
                                jv[tag::hash] = std::move(hash);
                                ++recalculated;
                                ++g::db_recalcs;
                            }

                            if (auto &jvt = jv[tag::timestamp]; jvt != mtime) {
                                jvt = std::move(mtime);
                                ++g::db_recalcs;
                            }
                        });
                        g::hash_avgs.w([&timer](auto &z) {
                            ++z.cnt;
                            z.dur += timer.dur<std::chrono::milliseconds>();
                        });
                    }
                } catch (const std::exception &ex) {
                    printf("%s\n", ex.what());
                    g::database.w([&fn](auto &z) {
                        z[tag::files].remove(**fn);
                    });
                }
            }
        } catch (const std::exception &ex) {
            printf("%s\n", ex.what());
        }
    }
    g::hash_avgs.w([&recalculated](auto &z) {
        z.recalc += recalculated;
    });
}

void process_loop() {
    using namespace std::chrono_literals;
    while (true) { // make sure we only look at do_process if queue is empty
        try {
            if (const auto fn = g::file_queue.get(.1s); !fn) {
                if (!g::do_process) break;
            } else {
                // see if we can use a record from the db
                if (!g::database.r([&fn](const auto &z) { return z[tag::files][**fn][tag::extract].isArr(); })) {
                    recalc:
                    ++g::db_recalcs;
                    gvs::timer timer;
                    if (const auto bmp = read_img(**fn); bmp) {
                        g::bmp_avgs.w([&bmp, &timer](auto &z) {
                            ++z.cnt;
                            z.sz += bmp->bytes();
                            z.dur += timer.dur<std::chrono::microseconds>();
                        });

                        bmp_averager_t bmp_info;
                        timer.start();

                        const auto [b_w, b_h] = bmp->dims();
                        grid_t<g::GRID_W, g::GRID_H> gridder{b_w, b_h};
                        for (u_int x{}; x < b_w; ++x) {
                            for (u_int y{}; y < b_h; ++y) {
                                bmp_info.val(gridder({x, y})) += bmp->px(x, y);
                            }
                        }

                        gvs::json::val extract_jv;
                        g::pointwithavg_t fileavgs;
                        for (const auto &[p, val]: bmp_info.vals()) {
                            const auto avgs = val(); // save the avgs as-is
                            extract_jv.append(gvs::json::val{gvs::tval{tag::point, p.to_json()}, gvs::tval{tag::vals, avgs.to_json()}});
                            // reduce the palette for lookups.
                            const auto mavgs = px_t::mult<0, g::PX_N, g::PX_D>(avgs);
                            g::grid2files.w([&p, &mavgs, &fn](auto &z) { z[p][mavgs].emplace(*fn); });
                            fileavgs.emplace(p, mavgs);
                        }
                        g::database.w([&fn, &extract_jv](auto &z) {
                            z[tag::files][**fn][tag::extract] = std::move(extract_jv);
                        });
                        g::file2grids.w([&fn, &fileavgs](auto &z) {
                            z.emplace_back(std::make_unique<g::fileandgrid_t::element_type>(*fn, std::move(fileavgs)));
                        });

                        g::proc_avgs.w([&bmp, &timer](auto &z) {
                            ++z.recalc;
                            ++z.cnt;
                            z.sz += bmp->bytes();
                            z.dur += timer.dur<std::chrono::milliseconds>();
                        });
                    } else {
                        g::bad_files.w([&fn](auto &z) { z.emplace_back(*fn); });
                        g::database.w([&fn](auto &z) { z[tag::files].remove(**fn); });
                    }
                } else {
                    try {
                        gvs::timer timer;
                        g::database.r([&fn](const auto &z) {
                            g::pointwithavg_t fileavgs;
                            for (const auto &jv: z[tag::files][**fn][tag::extract].asArr()) {
                                point_t p{jv[tag::point]};
                                auto avgs = px_t::mult<0, g::PX_N, g::PX_D>(jv[tag::vals]);
                                g::grid2files.w([&p, &avgs, &fn](auto &z) { z[p][avgs].emplace(*fn); });
                                fileavgs.emplace(p, avgs);
                            }
                            g::file2grids.w([&fn, &fileavgs](auto &z) {
                                z.emplace_back(std::make_unique<g::fileandgrid_t::element_type>(*fn, std::move(fileavgs)));
                            });
                        });
                        g::proc_avgs.w([&timer](auto &z) {
                            ++z.cnt;
                            z.dur += timer.dur<std::chrono::milliseconds>();
                        });
                    } catch (...) {
                        goto recalc;
                    }
                }
            }
        } catch (const std::exception &ex) {
            printf("%s\n", ex.what());
        }
    }
}

void match_loop() {
    using namespace std::chrono_literals;
    const auto &grid2files = g::grid2files.r([](const auto &z) -> decltype(auto) {return z;}); // direct access - its read-only now

    while (true) {
        if (const auto fng = g::fileandgrid_queue.get(.1s); !fng) {
            if (!g::do_match) break;
        } else {
            avg_t<int> avg_lum;
            const auto &[fn, grid] = **fng;
            std::unordered_map<g::string_ptr, int> ftree; // to count grid natches per file that matches this particular grid piece
            for (const auto &[point, avgs]: grid) { // iterate on grid per file
                avg_lum += avgs.lum();
//                printf("iter: [%d,%d]: %d,%d,%d\n", point.x, point.y, avgs.r, avgs.g, avgs.b);
                if (const auto point_it = grid2files.find(point); point_it != grid2files.end()) {
                    const auto &avgs_map = point_it->second;
                    if (const auto avg_it = avgs_map.find(avgs); avg_it != avgs_map.cend()) {
                        for (const auto &mfn: avg_it->second) {
                            if (fn == mfn) continue;
                            ++ftree[mfn];
                        }
                    }
                }
            }
            if (avg_lum() > 2) g::duplicates.w([&fn](auto &list){list.front().emplace(fn);});

            std::set<g::string_ptr> matching_files;
            for (const auto &[mfn, cnt]: ftree) {
                if (cnt >= g::GRID_W * g::GRID_H * g::PASSABLE_RATE::num / g::PASSABLE_RATE::den) {
                    matching_files.emplace(mfn);
                }
            }
            if (!matching_files.empty()) {
                matching_files.emplace(fn);
                g::duplicates.w([&matching_files] (auto &list) {
                    list.emplace_back(std::move(matching_files));
                });
            }
        }
    }
}

}

void worker() {
    using namespace std::chrono_literals;

    g::worker_state.w([](auto &z) { ++z.hashing; });
    hash_loop();
    g::worker_state.w([](auto &z) {
        --z.hashing;
        ++z.processing;
    });
    process_loop();
    g::worker_state.w([](auto &z) {
        --z.processing;
        ++z.matching;
    });
    match_loop();
    g::worker_state.w([](auto &z) { --z.matching; });
}
