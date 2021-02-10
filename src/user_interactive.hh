
#pragma once

#include <gvs_exec.hh>

#include <list>
#include <memory>
#include <set>
#include <string>

int deal_with_duplicates(gvs::exec &executor, std::list<std::shared_ptr<std::string>> &&bad_files, std::list<std::set<std::shared_ptr<std::string>>> &&clusters);
