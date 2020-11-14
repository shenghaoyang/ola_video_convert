#ifndef _IO_HPP_INCLUDED
#define _IO_HPP_INCLUDED

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <utility>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>
#include <algorithm>
#include <map>
#include <iostream>
#include <type_traits>

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

    OLAFrame() {
        clear();
    }

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
inline static uint8_t* write_line(
    uint8_t *l, std::uint32_t universe, const UniverseData& data) noexcept {
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
inline static void write_lines(uint8_t *l, size_t stride, const UniverseStates& states) noexcept {
    auto pos{l};
    for (const auto& st : states) {
        write_line(pos, st.first, st.second);
        pos += stride;
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
template<typename T>
static T& read_frame(T& s, OLAFrame& f) {
    static_assert(std::is_base_of<std::istream, T>::value, 
        "s must be a char input stream");
    
    thread_local std::string buf{};
    thread_local std::vector<
        boost::iterator_range<std::string::const_iterator>> split{};
    bool readdata{false};

    split.clear();
    f.clear();
    while (true) {
        if (!std::getline(s, buf)) {
            if (s.eof() && readdata)
                f.duration_ms = -1;
            break;
        }

        boost::trim(buf);
        if ((buf == show_header) || !buf.size())
            continue;

        boost::split(split, buf, [](const char c){return c == ' ';});

        std::uint32_t val;
        const auto& first{split[0]};
        const auto *beg{&(*first.begin())};
        auto rslt{std::from_chars(beg, beg + first.size(), val)};
        if (rslt.ec != std::errc{})
            throw std::runtime_error{"bad frame duration / universe number"};
        
        if (split.size() < 2) {
            if (!readdata)
                throw std::runtime_error{"no frame before frame time"};

            f.duration_ms = val;
            break;
        }

        f.universe = val;
        beg = &(*split[1].begin());
        std::string_view second{beg, split[1].size()};
        split.clear();

        boost::split(split, second, [](const char c){return c == ',';});
        for (std::size_t i{0}; i < split.size(); ++i) {
            const auto& channel{split[i]};
            // Trailing commas
            if (!channel.size() && (i == (split.size() - 1)))
                break;

            const auto *beg{&(*channel.begin())};
            auto rslt{std::from_chars(beg, 
                beg + channel.size(), f.data[i])};
            if (rslt.ec != std::errc{})
                throw std::runtime_error{"bad frame data"};
        }
        readdata = true;
    }

    return s;
}
}
}

#endif
