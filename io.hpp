#ifndef _IO_HPP_INCLUDED
#define _IO_HPP_INCLUDED

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace olavc {
namespace io {
/**
 * Header for the V1 OLA show file.
 */
static constexpr const char *show_header{"OLA Show"};
/**
 * Frame width / line width in bytes for video conversion.
 *
 * One line represents one universe.
 *
 * - The first two bytes represent a unsigned integer encoded in straight
 *   binary and stored in the little-endian format. It contains the
 *   universe number of that line.
 *   Seems a little inappropriate to use a uint16 for this task, since
 *   we use uint32s for all universe numbers in the program, but we can always
 *   expand this in the future.
 * - The last 514 bytes contain the DMX channel data, starting from
 *   channel zero.
 */
static constexpr const auto frame_width{2 + 512};

/**
 * Type representing universe channel data.
 */
using UniverseData = std::array<std::uint8_t, 512>;
/**
 * Type representing channel data for multiple universes.
 */
using UniverseStates = std::map<std::uint32_t, UniverseData>;

/**
 * Structure describing a single OLA recorder frame.
 */
struct OLAFrame {
  /**
   * Duration of the frame in milliseconds.
   */
  std::int64_t duration_ms;
  /**
   * DMX universe this frame is to be emitted on.
   */
  std::uint32_t universe;
  /**
   * Data contained within the frame.
   */
  UniverseData data;

  OLAFrame() { clear(); }

  /**
   * Clears the frame so it returns to its original state after
   * construction.
   */
  void clear() noexcept {
    data.fill(0);
    duration_ms = 0;
    universe = 0;
  }
};

/**
 * Writes a line (a single universe worth of data) to a buffer.
 *
 * \param l buffer to write to.
 * \param universe universe data is meant for.
 * \param data universe channel data.
 */
inline static uint8_t *write_line(uint8_t *l, std::uint32_t universe,
                                  const UniverseData &data) noexcept {
  l[0] = universe & static_cast<uint32_t>(0xff);
  l[1] = (universe & static_cast<uint32_t>(0xff00)) >> 8;

  return std::copy(data.begin(), data.end(), l + 2);
}

/**
 * Writes all universe states to a buffer.
 *
 * \param l buffer to write to.
 * \param stride number of bytes actually allocated for each line.
 * \param states universe states.
 */
inline static void write_lines(uint8_t *l, size_t stride,
                               const UniverseStates &states) noexcept {
  auto pos{l};
  for (const auto &st : states) {
    write_line(pos, st.first, st.second);
    pos += stride;
  }
}

inline static auto trim(std::string_view s) {
  static const auto &loc_c{std::locale::classic()};
  auto not_space = [](char c) { return !std::isspace(c, loc_c); };
  auto nbegin{std::find_if(std::begin(s), std::end(s), not_space)};
  auto nend{
      std::begin(s) +
      (std::distance(std::find_if(std::rbegin(s), std::rend(s), not_space),
                     std::rend(s)))};
  return s.substr(std::distance(std::begin(s), nbegin),
                  std::distance(nbegin, nend));
}

inline static auto split_char(std::string_view s, char c) {
  using rtype = std::pair<std::string_view, std::string_view>;
  auto bpos{s.find(c)};
  if (bpos == std::string_view::npos)
    return rtype{s, std::string_view{s.end()}};

  bpos = s.find_first_not_of(c, bpos);
  if (bpos == std::string_view::npos) bpos = s.size();

  return rtype{std::string_view{s.begin(), bpos},
               std::string_view{s.begin() + bpos}};
}

inline static void ParseChans(std::string_view s, UniverseData &d) {
  constexpr static int mult[]{100, 10, 1};

  d.fill(0);
  std::size_t c{};
  for (std::size_t i{}; i < s.size();) {
    if (i && s[i] == ',') {
      ++i;
      if (i == s.size()) return;
    }

    int v{};
    int digits{};
    for (; digits < 3; ++digits) {
      if ((i + digits) >= s.size()) break;
      char c{s[i + digits]};
      if ((c < '0') || (c > '9')) break;
    }

    if (!digits)
      throw std::runtime_error{"channel undefined / has wrong format"};

    for (int digit{}; digit < digits; ++digit)
      v += (s[i + digit] - '0') * mult[(3 - digits) + digit];
    if (v > std::numeric_limits<UniverseData::value_type>::max())
      throw std::runtime_error{"channel value overflow"};

    d.at(c++) = v;

    i += digits;
  }
}

/**
 * Reads frames from an OLA recorder showfile.
 *
 * If the frame is the last one read, its duration will be set to \c -1 .
 *
 * \param s character input stream to read from.
 * \param f frame to write to.
 * \return \c s.
 *
 * \note Not a fully compliant reader: accepts header at non-0 position.
 */
static std::istream &read_frame(std::istream &s, OLAFrame &f) {
  thread_local std::string buf{};
  bool readdata{false};

  f.clear();
  while (true) {
    if (!std::getline(s, buf)) {
      if (s.eof() && readdata) f.duration_ms = -1;
      break;
    }

    const auto bufv{trim(buf)};
    if ((bufv == show_header) || !bufv.size()) continue;

    auto segs{split_char(bufv, ' ')};

    std::uint32_t val;
    auto rslt{std::from_chars(segs.first.data(),
                              segs.first.data() + segs.first.size(), val)};
    if (rslt.ec != std::errc{})
      throw std::runtime_error{"bad frame duration / universe number"};

    if (!segs.second.size()) {
      if (!readdata) throw std::runtime_error{"no frame before frame time"};
      f.duration_ms = val;
      break;
    }

    f.universe = val;
    ParseChans(segs.second, f.data);
    readdata = true;
  }

  return s;
}
}  // namespace io
}  // namespace olavc

#endif
