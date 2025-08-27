#pragma once
#include <cstdint>
#include <istream>
#include <stdexcept>

inline void read_exact(std::istream& in, char* dst, std::streamsize n) {
    in.read(dst, n);
    if (in.gcount() != n) throw std::runtime_error("Unexpected EOF");
}

inline uint16_t be16(const unsigned char b[2]) {
    return (uint16_t(b[0]) << 8) | uint16_t(b[1]);
}
inline uint32_t be32(const unsigned char b[4]) {
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) |
           (uint32_t(b[2]) << 8)  |  uint32_t(b[3]);
}
inline uint64_t be64(const unsigned char b[8]) {
    return (uint64_t(b[0]) << 56) | (uint64_t(b[1]) << 48) |
           (uint64_t(b[2]) << 40) | (uint64_t(b[3]) << 32) |
           (uint64_t(b[4]) << 24) | (uint64_t(b[5]) << 16) |
           (uint64_t(b[6]) <<  8) |  uint64_t(b[7]);
}
inline int32_t be32s(const unsigned char b[4]) {
    return static_cast<int32_t>(be32(b));
}
