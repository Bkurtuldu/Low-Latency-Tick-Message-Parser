#include "moldudp64.hpp"
#include "bytes.hpp"
#include <iostream>

bool MoldUDP64Reader::next_packet(MoldHeader& hdr, std::vector<MoldMessage>& out) {
    in_.peek();
    if (in_.eof()) return false;

    char session_raw[10]; // session id is 10 bytes
    unsigned char seq_raw[8]; // sequence number is 8 bytes
    unsigned char count_raw[2]; // message count is 2 bytes

    read_exact(in_, session_raw, 10); // read session id
    read_exact(in_, reinterpret_cast<char*>(seq_raw), 8); // read sequence number
    read_exact(in_, reinterpret_cast<char*>(count_raw), 2); // read message count

    hdr.session.assign(session_raw, 10); // assign session id
    hdr.first_seq = be64(seq_raw); // convert and assign sequence number
    hdr.msg_count = be16(count_raw); // convert and assign message count

    out.clear();

    MoldMessage msg;

    // Read each message in the packet
    for (int i = 0; i < hdr.msg_count; i++) {
        unsigned char len_raw[2]; // message length is 2 bytes
        read_exact(in_, reinterpret_cast<char*>(len_raw), 2); // read message length
        uint16_t mlen = be16(len_raw); // convert message length

        msg.payload.resize(mlen); // resize payload to fit message
        read_exact(in_, reinterpret_cast<char*>(msg.payload.data()), mlen); // read message payload
        out.push_back(std::move(msg)); // add message to output vector
    }

    return true;
}
