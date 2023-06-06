#pragma once

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <string>
#include <vector>

struct DownloadState {
  std::string url;
  std::string output;
  int concurrency;
  std::vector<size_t> progresses;

  DownloadState() = default;
  ~DownloadState() = default;

  template <typename Archieve>
  void serialize(Archieve& ar, const unsigned int v) {
    ar& url;
    ar& output;
    ar& concurrency;
    ar& progresses;
  }
};
