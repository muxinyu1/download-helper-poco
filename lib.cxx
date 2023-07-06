#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPSStreamFactory.h>
#include <Poco/Path.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <cstdlib>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <memory>
#include <sqlite3.h>

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "DownloadState.hxx"
#include "include/indicators/indicators.hpp"

static DownloadState state;

static void update_progress(size_t current, size_t total, int thread_id,
                            indicators::ProgressBar *bar) {
  using namespace indicators;

  if (current < total) {
    // fmt::println("percentage = {}", percentage);
    bar->set_progress(100.0 * current / total);
    // Update download state
    state.progresses[thread_id] = current;
    // fmt::println("progress = {}", 100.0 * current / total);
    // fmt::println("{} / {}", current, total);
  } else {
    // Update download state
    state.progresses[thread_id] = total;
    bar->set_option(option::ForegroundColor{Color::green});
    bar->set_progress(100);
    bar->mark_as_completed();
  }
}

static std::unique_ptr<Poco::Net::HTTPClientSession>
create_client(const std::string &url) {
  Poco::URI uri{url};
  if (uri.getScheme() == "https") {
    Poco::Net::Context::Ptr context = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
    auto session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(), context);
    session->setKeepAlive(true);
    return session;
  } else if (uri.getScheme() == "http") {
    auto session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
    session->setKeepAlive(true);
    return session;
  }
  // TODO: Other Protocols
  return std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
}

static void download_part(const size_t start_bytes, const size_t end_bytes,
                          const std::string &url, const std::string &output,
                          const int thread_id, indicators::ProgressBar *bar,
                          std::mutex *mtx, size_t *main_thread_downloaded, bool single_thread) {
  auto client = create_client(url);
  Poco::Net::HTTPRequest request{};
  request.set("Range", fmt::format("bytes={}-{}", start_bytes, end_bytes));

  client->sendRequest(request);
  Poco::Net::HTTPResponse response{};
  auto &stream = client->receiveResponse(response);

  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_PARTIAL_CONTENT || single_thread) {
    try {
      const auto path = std::filesystem::temp_directory_path() /
                        fmt::format("{}.part{}", output, thread_id);
      std::ofstream out{path,
                        std::ios::out | std::ios::trunc | std::ios::binary};
      
      state.streams.emplace_back(&out);
      char buffer[1024];
      const auto total = end_bytes - start_bytes + 1;
      size_t downloaded = 0;
      // Poco::StreamCopier::copyStream(stream, out);
      while (stream.read(buffer, sizeof(buffer))) {
        const auto n = stream.gcount();
        // fmt::println("Thread {} read {} bytes", thread_id, n);
        // if (thread_id == 7) {
        //   fmt::println("percentage={}", (float)(100.0 * downloaded / total));
        // }
        mtx->lock();
        *main_thread_downloaded += n;
        mtx->unlock();
        downloaded += n;
        out.write(buffer, n);
        update_progress(downloaded, total, thread_id, bar);
      }
      if (stream.gcount() > 0) {
        downloaded += stream.gcount();
        mtx->lock();
        *main_thread_downloaded += stream.gcount();
        mtx->unlock();
        update_progress(downloaded, total, thread_id, bar);
        out.write(buffer, stream.gcount());
      }
      out.close();
    } catch (const std::exception &e) {
      fmt::println("Error: {}", e.what());
      return;
    }

  } else {
    fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
               "Error: Url Does NOT Support Multithreading\n");
    exit(EXIT_FAILURE);
  }
}

static std::pair<size_t, bool> get_content_length(const std::string &url) {
  // fmt::println("Getting content length...");
  try {
    Poco::URI uri{url};

    auto client = create_client(url);
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_HEAD,
                                   uri.getPath()};
    Poco::Net::HTTPResponse response{};

    client->sendRequest(request);
    client->receiveResponse(response);

    if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
      const auto content_length = response.getContentLength64();
      // Try Sending Partial File Request
      Poco::Net::HTTPRequest request{};
      request.set("Range", fmt::format("bytes={}-{}", 0, 0));
      client->sendRequest(request);
      Poco::Net::HTTPResponse response{};
      client->receiveResponse(response);
      // According Response, Return If Server Support Multithreading
      if (response.getStatus() ==
          Poco::Net::HTTPResponse::HTTP_PARTIAL_CONTENT) {
        return {content_length, true};
      } else {
        return {content_length, false};
      }
    } else {
      fmt::println("Error: Response Status is not 200");
      exit(EXIT_FAILURE);
    }
  } catch (const std::exception &e) {
    fmt::println("Error: {}", e.what());
    exit(EXIT_FAILURE);
  }
  return {0, false};
}

void combine(const std::string &output, const int concurrency) {
  try {
    std::ofstream out{output,
                      std::ios::out | std::ios::trunc | std::ios::binary};
    if (out.is_open()) {
      for (int i = 0; i < concurrency; ++i) {
        const auto part_file = (std::filesystem::temp_directory_path() /
                                fmt::format("{}.part{}", output, i));
        std::ifstream in{part_file, std::ios::binary};
        if (in.is_open()) {
          out << in.rdbuf();
        } else {
          fmt::println("Error: Could not open file: {}", part_file.string());
          exit(EXIT_FAILURE);
        }
        in.close();

        // Delete temp file
        std::remove(part_file.c_str());
      }
      out.close();
    } else {
      fmt::println("Error: Could not open file: {}", output);
      exit(EXIT_FAILURE);
    }
  } catch (const std::exception &e) {
    fmt::println("Error: {}", e.what());
    exit(EXIT_FAILURE);
  }
}

void download(const std::string &url, const std::string &output,
              int concurrency) {
  using namespace indicators;

  state.url = url;
  state.output = output;
  state.concurrency = concurrency;
  for (int i = 0; i < concurrency; ++i) {
    state.progresses.push_back(0);
  }
  // Poco::Net::HTTPSStreamFactory::registerFactory();

  Poco::URI uri{url};
  fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
             "Try Downloading ");
  fmt::print(fmt::fg(fmt::color::purple) | fmt::emphasis::underline |
                 fmt::emphasis::bold,
             "{}", output);
  fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold, " from {}...\n",
             uri.getHost());
  const auto [content_length, support_multithreading] = get_content_length(url);

  // Single Thread Download
  if (!support_multithreading) {
    concurrency = 1;
  }
  // fmt::println("file-size={}B", content_length);

  const auto bytes_per_thread = content_length / (size_t)concurrency;
  const auto remaining_bytes = content_length % bytes_per_thread;

  DynamicProgress<ProgressBar> bars{};
  bars.set_option(option::HideBarWhenComplete{false});
  // std::mutex mtx{};

  ProgressBar main_thread_bar{
      option::PrefixText{"Main Thread"},
      option::ForegroundColor{Color::cyan},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
      option::Fill{"="},
      option::Remainder{" "},
      option::Lead{">"},
      option::ShowPercentage{true},
      option::BarWidth{20},
      option::PostfixText{""}};
  bars.push_back(main_thread_bar);
  // ProgressBar** child_thread_bars = new ProgressBar*[concurrency];
  // for (int i = 0; i < concurrency; ++i) {
  //   child_thread_bars[i] = new ProgressBar(
  //     option::PrefixText{fmt::format("Thread: {}", i)},
  //     option::ForegroundColor{Color::cyan},
  //     option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
  //     option::Fill{"😅"},
  //     option::Remainder{"💧"},
  //     option::Lead{"😄"}
  //   );
  // }
  std::unique_ptr<std::unique_ptr<ProgressBar>[]> child_thread_bars{
      new std::unique_ptr<ProgressBar>[concurrency]};
  for (int i = 0; i < concurrency; ++i) {
    child_thread_bars[i].reset(new ProgressBar(
        option::PrefixText{fmt::format("Thread: {}", i)},
        option::ForegroundColor{Color::cyan},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
        option::ShowPercentage{true}, option::Fill{"="}, option::Remainder{" "},
        option::Lead{">"}, option::BarWidth{20}, option::PostfixText{""}));
  }

  for (int i = 0; i < concurrency; ++i) {
    bars.push_back(*child_thread_bars[i]);
  }

  // std::thread update_bars([&bars, &mtx] {
  //   while (!bars[0].is_completed()) {
  //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
  //     std::lock_guard<std::mutex> lock{mtx};

  //   }
  // });

  // Main Thread Progress
  std::mutex mtx{};
  size_t main_thread_downloaded = 0;

  std::vector<std::thread> threads{};
  for (int i = 0; i < concurrency; ++i) {
    const auto start_bytes = i * bytes_per_thread;
    auto end_bytes = start_bytes + bytes_per_thread - 1;
    if (i == concurrency - 1) {
      end_bytes += remaining_bytes;
    }
    threads.emplace_back(download_part, start_bytes, end_bytes, url, output, i,
                         &(*child_thread_bars[i]), &mtx,
                         &main_thread_downloaded, !support_multithreading);
  }

  bool finished = false;
  while (!finished) {
    finished = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bars[0].set_progress(100.0 * main_thread_downloaded / content_length);
    bars.print_progress();
    for (int i = 0; i < concurrency; ++i) {
      if (!child_thread_bars[i]->is_completed()) {
        finished = false;
        break;
      }
    }
  }

  bars[0].set_progress(100);
  bars[0].set_option(option::ForegroundColor{Color::green});

  // Wait for finished
  for (auto &thread : threads) {
    thread.join();
  }
  // std::lock_guard<std::mutex> lock{mtx};
  bars[0].mark_as_completed();

  fmt::print(fmt::fg(fmt::color::green) | fmt::emphasis::bold |
                 fmt::emphasis::italic,
             "✔ Downloaded\n");

  // Poco::Net::HTTPSStreamFactory::unregisterFactory();
  // gc
  // for (int i = 0; i < concurrency; ++i) {
  //   delete child_thread_bars[i];
  // }
  // delete[] child_thread_bars;

  // Combine files
  combine(output, concurrency);
}

namespace database {
bool download_state_table_exists(sqlite3 *db) {
  const char sql[] =
      "select from sqlite_master where type = \'table\' and name = "
      "\'DownloadState\'";
  bool exist = false;
  sqlite3_exec(
      db, sql,
      [](void *exist, int, char **, char **) {
        *(bool *)exist = true;
        return 0;
      },
      &exist, nullptr);
  return exist;
}

void create_download_state_table(sqlite3 *db) {
  const char sql[] =
      "create table DownloadState(url text primary key, state text)";
  sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

bool primary_key_exists(sqlite3 *db, const auto &primary_key) {
  bool exist = false;
  const auto sql = fmt::format(
      "select exists (select 1 from DownloadState where url = \'{}\')",
      primary_key);
  sqlite3_exec(
      db, sql.c_str(),
      [](void *exist, int, char **, char **) {
        *(bool *)exist = true;
        return 0;
      },
      &exist, nullptr);
  return exist;
}

void update(sqlite3 *db, const auto &primary_key, const auto &data) {
  const auto sql =
      fmt::format("update DownloadState set state = \'{}\' where url = \'{}\'",
                  data, primary_key);
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void insert(sqlite3 *db, const auto &primary_key, const auto &data) {
  const auto sql =
      fmt::format("insert into DownloadState (url, state) values(\'{}\', \'{}\')",
                  primary_key, data);
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}
} // namespace database

namespace boost::archive {
template <typename T>
concept serializable = requires(T t, std::ostream &os) {
  { (t.serialize(os, 0)) } -> std::same_as<void>;
};
}

template <typename T>
  requires boost::archive::serializable<T>
auto to_data(const T &data) {
  std::ostringstream oss{};
  boost::archive::text_oarchive oa{oss};
  oa << data;
  return oss.str();
}

void suspend(int) {
  using namespace database;
  //TODO: Stop downloading


  if (!std::filesystem::exists("data.db")) {
    // Create database
    std::ofstream ofs{"data.db"};
    ofs.close();
  }
  // Save download state
  sqlite3 *db;
  if (sqlite3_open("data.db", &db) != SQLITE_OK) {
    std::cerr << "Could not open \'data.db\'" << std::endl;
    sqlite3_close(db);
    return;
  }
  if (!download_state_table_exists(db)) {
    create_download_state_table(db);
  }
  const auto primary_key = state.url;
  const auto data = to_data(state);
  if (primary_key_exists(db, primary_key)) {
    update(db, primary_key, data);
  } else {
    insert(db, primary_key, data);
  }
  fmt::print(fmt::fg(fmt::color::blue) | fmt::emphasis::bold, "Suspend\n");
  exit(EXIT_SUCCESS);
}