#include "sha256.h"

#include <array>
#include <bit>
#include <iomanip>
#include <sstream>

namespace {
    constexpr std::array<std::uint32_t, 64> kRoundConstants{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    constexpr std::array<std::uint32_t, 8> kInitialState{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    constexpr std::uint32_t Ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        return (x & y) ^ (~x & z);
    }

    constexpr std::uint32_t Maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    constexpr std::uint32_t Sigma0(std::uint32_t x) {
        return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
    }

    constexpr std::uint32_t Sigma1(std::uint32_t x) {
        return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
    }

    constexpr std::uint32_t SmallSigma0(std::uint32_t x) {
        return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
    }

    constexpr std::uint32_t SmallSigma1(std::uint32_t x) {
        return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
    }
}

std::vector<std::uint8_t> Sha256::Pad(std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> padded(data.begin(), data.end());
    padded.push_back(0x80);

    while ((padded.size() % 64) != 56) {
        padded.push_back(0x00);
    }

    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
    for (int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<std::uint8_t>((bitLength >> (i * 8)) & 0xFF));
    }

    return padded;
}

Sha256::Digest Sha256::Compute(std::span<const std::uint8_t> data) {
    auto state = kInitialState;
    const auto padded = Pad(data);

    std::array<std::uint32_t, 64> w{};

    for (std::size_t chunk = 0; chunk < padded.size(); chunk += 64) {
        for (std::size_t i = 0; i < 16; ++i) {
            const auto base = chunk + i * 4;
            w[i] =
                (static_cast<std::uint32_t>(padded[base]) << 24) |
                (static_cast<std::uint32_t>(padded[base + 1]) << 16) |
                (static_cast<std::uint32_t>(padded[base + 2]) << 8) |
                (static_cast<std::uint32_t>(padded[base + 3]));
        }

        for (std::size_t i = 16; i < 64; ++i) {
            w[i] = SmallSigma1(w[i - 2]) + w[i - 7] + SmallSigma0(w[i - 15]) + w[i - 16];
        }

        auto [a0, b0, c0, d0, e0, f0, g0, h0] = state;
        auto a = a0;
        auto b = b0;
        auto c = c0;
        auto d = d0;
        auto e = e0;
        auto f = f0;
        auto g = g0;
        auto h = h0;

        for (std::size_t i = 0; i < 64; ++i) {
            const auto t1 = h + Sigma1(e) + Ch(e, f, g) + kRoundConstants[i] + w[i];
            const auto t2 = Sigma0(a) + Maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    Digest digest{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        digest[i * 4] = static_cast<std::uint8_t>((state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state[i] & 0xFF);
    }

    return digest;
}

Sha256::Digest Sha256::Compute(std::string_view text) {
    return Compute(std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
}

std::string Sha256::Hex(std::span<const std::uint8_t> data) {
    const auto digest = Compute(data);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::string Sha256::Hex(std::string_view text) {
    return Hex(std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
}
