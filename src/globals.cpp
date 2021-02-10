
#include "globals.hh"

namespace g {

gvs::mutexed<avgs_t> hash_avgs;
gvs::mutexed<avgs_t> read_avgs;
gvs::mutexed<avgs_t> bmp_avgs;
gvs::mutexed<avgs_t> proc_avgs;
std::atomic_int db_recalcs{};

gvs::tqueue<std::deque<string_ptr>, 111, true> file_queue;

std::atomic_bool do_hash{true};
std::atomic_bool do_process{true};
std::atomic_bool do_match{true};

// filename -> its hash
gvs::mutexed<gvs::json::val> database;

// file name -> info
gvs::tqueue<std::deque<fileandgrid_t>, 111, true> fileandgrid_queue;
gvs::mutexed<std::list<fileandgrid_t>> file2grids; //

gvs::mutexed<std::unordered_map<point_t, std::unordered_map<px_t, std::set<string_ptr>, px_hash>, point_hash>> grid2files;

// main lookup table, grid elt -> set of files that have same grid elt
//gvs::mutexed<std::unordered_map<grid_elt_t, std::set<string_ptr>, grid_elt_hash>> grid2files; // grid to filename

// list of (set of matching files) groups
gvs::mutexed<std::list<std::set<string_ptr>>> duplicates;

gvs::mutexed<std::list<std::shared_ptr<std::string>>> bad_files;

gvs::mutexed<worker_state_t> worker_state;

}


