#include <fmt/format.h>
#include <fmt/color.h>

#include <csignal>

#include "include/indicators/indicators.hpp"
#include "include/cxxopts/cxxopts.hpp"

extern void download(const std::string& url, const std::string& output, const int concurrency);

extern void suspend(int);

int main(int argc, char *argv[]) {
  signal(SIGINT, suspend);

  cxxopts::Options options{"Download Helper", "A Download Helper"};
  options.add_options()
  ("u,url", "File Url", cxxopts::value<std::string>(), "File Url")
  ("o,output", "Output File Name", cxxopts::value<std::string>()->default_value(""), "Output File Name")
  ("h,help", "Show Helps")
  ("n,concurrency", "Thread Number", cxxopts::value<int>()->default_value("8"), "Thread Num");
	try {
		auto result = options.parse(argc, argv);
		const auto url = result["url"].as<std::string>();
		auto output = result["output"].as<std::string>();
		const auto concurrency = result["concurrency"].as<int>();

		// Show Help
		if (result.count("help")) {
			fmt::println("{}", options.help());
		}

		if (output.empty()) {
			output = url.substr(url.find_last_of('/') + 1);
		}
		try {
		// fmt::println("url={}, output={}, concurrency={}", url, output, concurrency);
		download(url, output, concurrency);
		} catch (const std::exception& e) {
			fmt::println(": {}", e.what());
			fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "Error");
			exit(EXIT_FAILURE);
		}
	} catch (const std::exception& e) {
		fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "Error");
		fmt::println(": {}", e.what());
		
		fmt::println("{}", options.help());
		exit(EXIT_FAILURE);
	}
	return 0;
}
