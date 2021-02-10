
#pragma once

#include "bmp_averager.hh"

#include <gvs_json.hh>
#include <gvs_mutexed.hh>
#include <gvs_tqueue.hh>

#include <chrono>
#include <deque>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace g {

constexpr const int GRID_W{3};
constexpr const int GRID_H{3};

constexpr const int PX_N{1};
constexpr const int PX_D{32};

typedef std::ratio<90, 100> PASSABLE_RATE;

typedef std::shared_ptr<std::string> string_ptr;

//map of point:average;
typedef std::unordered_map<point_t, px_t, point_hash> pointwithavg_t;

// file -> point:average
typedef std::unique_ptr<std::tuple<string_ptr, pointwithavg_t>> fileandgrid_t;

struct avgs_t {
    size_t sz{};
    size_t cnt{};
    size_t recalc{};
    std::chrono::microseconds dur{};
};

extern gvs::mutexed<avgs_t> hash_avgs;
extern gvs::mutexed<avgs_t> read_avgs;
extern gvs::mutexed<avgs_t> bmp_avgs;
extern gvs::mutexed<avgs_t> proc_avgs;
extern std::atomic_int db_recalcs;

extern gvs::tqueue<std::deque<string_ptr>, 111, true> file_queue;

// flags for the workers
extern std::atomic_bool do_hash;
extern std::atomic_bool do_process;
extern std::atomic_bool do_match;

// json hashes
extern gvs::mutexed<gvs::json::val> database;

extern gvs::tqueue<std::deque<fileandgrid_t>, 111, true> fileandgrid_queue;
extern gvs::mutexed<std::list<fileandgrid_t>> file2grids; // file -> grid

// point -> averages -> filenames
extern gvs::mutexed<std::unordered_map<point_t, std::unordered_map<px_t, std::set<string_ptr>, px_hash>, point_hash>> grid2files;

// main lookup table, grid elt -> set of files that have same grid elt
//extern gvs::mutexed<std::unordered_map<grid_elt_t, std::set<string_ptr>, grid_elt_hash>> grid2files; // grid to files

// list of (set of matching files) groups
extern gvs::mutexed<std::list<std::set<string_ptr>>> duplicates;

extern gvs::mutexed<std::list<std::shared_ptr<std::string>>> bad_files;

// --------------------------------------
struct worker_state_t {
    int hashing{};
    int processing{};
    int matching{};
};
extern gvs::mutexed<worker_state_t> worker_state;

}
