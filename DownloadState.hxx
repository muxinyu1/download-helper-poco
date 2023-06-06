#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>

struct DownloadState {
  std::string url;
  std::string output;
  int concurrency;
  std::vector<size_t> progresses;
  std::vector<std::ofstream*> streams;

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
