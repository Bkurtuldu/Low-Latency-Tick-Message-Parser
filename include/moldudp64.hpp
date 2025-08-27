#pragma once
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

struct MoldHeader {
    std::string session;   // 10 bytes ASCII
    uint64_t first_seq;    // big-endian
    uint16_t msg_count;    // big-endian
};

struct MoldMessage {
    std::vector<unsigned char> payload; // raw ITCH bytes (not parsed yet)
};

class MoldUDP64Reader {
public:
    explicit MoldUDP64Reader(std::istream& in) : in_(in) {}
    bool next_packet(MoldHeader& hdr, std::vector<MoldMessage>& out);
private:
    std::istream& in_;
};
