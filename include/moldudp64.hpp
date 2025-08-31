#pragma once
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

struct MoldHeader {
    std::string session;  
    uint64_t first_seq;  
    uint16_t msg_count;   
};

struct MoldMessage {
    std::vector<unsigned char> payload;
};

class MoldUDP64Reader {
public:
    explicit MoldUDP64Reader(std::istream& in) : in_(in) {}
    bool next_packet(MoldHeader& hdr, std::vector<MoldMessage>& out);
private:
    std::istream& in_;
};
