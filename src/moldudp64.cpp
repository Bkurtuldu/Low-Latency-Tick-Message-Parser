#include "moldudp64.hpp"
#include "bytes.hpp"
#include <iostream>

bool MoldUDP64Reader::next_packet(MoldHeader& hdr, std::vector<MoldMessage>& out) {
    // peek to check EOF
    in_.peek();
    if (in_.eof()) return false;

    // read header
    char session_raw[10];
    unsigned char seq_raw[8];
    unsigned char count_raw[2];

    read_exact(in_, session_raw, 10);
    read_exact(in_, reinterpret_cast<char*>(seq_raw), 8);
    read_exact(in_, reinterpret_cast<char*>(count_raw), 2);

    hdr.session.assign(session_raw, 10);
    hdr.first_seq = be64(seq_raw);
    hdr.msg_count = be16(count_raw);

    out.clear();
    // skip over messages (just consume, don’t parse yet)
    for (int i = 0; i < hdr.msg_count; i++) {
        unsigned char len_raw[2];
        read_exact(in_, reinterpret_cast<char*>(len_raw), 2);
        uint16_t mlen = be16(len_raw);

        MoldMessage msg;
        msg.payload.resize(mlen);
        read_exact(in_, reinterpret_cast<char*>(msg.payload.data()), mlen);
        out.push_back(std::move(msg));
    }

    return true;
}
