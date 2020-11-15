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
  options.add_options()("u,universes", "number of universes",
                        cxxopts::value<int>())("o,output",
                                               "path of output FFV1 MKV file",
                                               cxxopts::value<std::string>())(
      "i,input", "path of input showfile", cxxopts::value<std::string>())(
      "h,help", "show help");
  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << '\n';
    return 0;
  }

  int num_universe{result["universes"].as<int>()};
  if (num_universe <= 0)
    throw std::runtime_error{"non-positive universe count"};

  DMXVideoEncoder::DMXVideoEncoder encoder{num_universe,
                                           result["output"].as<std::string>()};

  std::ifstream show{result["input"].as<std::string>()};
  if (!show) throw std::runtime_error{"could not open showfile"};

  io::UniverseStates universe_states{};
  io::OLAFrame d_frame{};

  while (read_frame(show, d_frame) || (d_frame.duration_ms == -1)) {
    universe_states[d_frame.universe] = d_frame.data;
    if (universe_states.size() > num_universe)
      throw std::runtime_error{"too many universes in showfile"};

    if (!d_frame.duration_ms) continue;

    if (universe_states.size() != num_universe)
      throw std::runtime_error{"universe state(s) undefined at encode"};

    encoder.write_universe(universe_states, std::max(d_frame.duration_ms,
                                                     static_cast<int64_t>(1)));
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
