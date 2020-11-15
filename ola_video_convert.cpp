#include <algorithm>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <media.hpp>
#include <stdexcept>
#include <string>

int prog(int argc, char **argv) {
  using namespace olavc;

  cxxopts::Options options{"ola_video_convert",
                           "converts an OLA showfile to a video"};
  // clang-format off
  options.add_options()
    ("u,universes", "number of universes", cxxopts::value<int>())
    ("o,output", "path of output FFV1 MKV file", cxxopts::value<std::string>())
    ("i,input", "path of input showfile", cxxopts::value<std::string>())
    ("l,last-duration", "duration of last frame (ms)", 
      cxxopts::value<int>()->default_value("1"))
    ("h,help", "show help")
    ("extra-positional", "extra positional arguments", 
      cxxopts::value<std::vector<std::string>>());

  options.positional_help("OUTPUT INPUT");
  options.show_positional_help();
  // clang-format on
  options.parse_positional({"output", "input", "extra-positional"});
  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cerr << options.help() << '\n';
    return 0;
  }

  if (!result.count("universes")) {
    std::cerr << "Error: no universe count specified." << '\n';
    return 1;
  }

  if (!result.count("output")) {
    std::cerr << "Error: no output path specified." << '\n';
    return 1;
  }

  if (!result.count("input")) {
    std::cerr << "Error: no input path specified." << '\n';
    return 1;
  }

  int num_universe{result["universes"].as<int>()};
  if (num_universe <= 0)
    throw std::runtime_error{"non-positive universe count"};

  DMXVideoEncoder::DMXVideoEncoder encoder{num_universe,
                                           result["output"].as<std::string>()};

  std::ifstream show{result["input"].as<std::string>()};
  if (!show) throw std::runtime_error{"could not open showfile"};

  int last_frame_time{result["last-duration"].as<int>()};
  io::UniverseStates universe_states{};
  io::OLAFrame d_frame{};

  while (read_frame(show, d_frame) || (d_frame.duration_ms == -1)) {
    universe_states[d_frame.universe] = d_frame.data;
    if (universe_states.size() > num_universe)
      throw std::runtime_error{"too many universes in showfile"};

    if (!d_frame.duration_ms) continue;

    if (d_frame.duration_ms == -1) d_frame.duration_ms = last_frame_time;

    if (universe_states.size() != num_universe)
      throw std::runtime_error{"universe state(s) undefined at encode"};

    encoder.write_universe(universe_states, d_frame.duration_ms);
  };

  if (!show.eof()) throw std::runtime_error{"reading showfile"};

  encoder.close();

  return 0;
}

int main(int argc, char **argv) {
  try {
    return prog(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "Exiting with error: " << e.what() << '\n';
    return 1;
  }
}
