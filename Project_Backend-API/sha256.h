#ifndef _SHA256_H_
#define _SHA256_H_

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class Sha256 {
public:
    using Digest = std::array<std::uint8_t, 32>;

    static Digest Compute(std::span<const std::uint8_t> data);
    static Digest Compute(std::string_view text);
    static std::string Hex(std::span<const std::uint8_t> data);
    static std::string Hex(std::string_view text);

private:
    static std::vector<std::uint8_t> Pad(std::span<const std::uint8_t> data);
};

#endif // !_SHA256_H_
