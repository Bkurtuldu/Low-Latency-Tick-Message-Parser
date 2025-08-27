#include "itch.hpp"
#include "bytes.hpp"
#include <cstring>

static inline bool need(const std::vector<unsigned char>& p, size_t n) {
    return p.size() >= n;
}

bool ItchParser::parse(const std::vector<unsigned char>& p, ItchMsg& out) const {
    if (p.empty()) return false;

    const unsigned char* b = p.data();
    unsigned char t = b[0];

    switch (t) {
    // 'O' Order Book State
    case 'O': {
        if (!need(p, 29)) return false;                 // 0..28
        out.type = ItchType::OrderBookState;
        out.obs.type    = out.type;
        out.obs.ts_ns   = be32(b + 1);                  // [1..4]
        out.obs.book_id = be32(b + 5);                  // [5..8]
        out.obs.state.assign(reinterpret_cast<const char*>(b + 9), 20); // [9..28]
        while (!out.obs.state.empty() && out.obs.state.back() == ' ')
            out.obs.state.pop_back();
        return true;
    }

    // 'A' Add Order (No MPID)
    case 'A': {
        if (!need(p, 34)) return false;                 // up to price @ [30..33]
        out.type = ItchType::AddOrder;
        out.add.type     = out.type;
        out.add.ts_ns    = be32(b + 1);
        out.add.order_id = be64(b + 5);
        out.add.book_id  = be32(b + 13);
        out.add.side     = static_cast<char>(b[17]);
        // skip rank seq @ [18..21]
        out.add.qty      = be64(b + 22);
        out.add.price    = be32s(b + 30);
        return true;
    }

    // 'E' Executed (no trade price)
    case 'E': {
        if (!need(p, 26)) return false;                 // through qty @ [18..25]
        out.type = ItchType::Executed;
        out.exe.type     = out.type;
        out.exe.ts_ns    = be32(b + 1);
        out.exe.order_id = be64(b + 5);
        out.exe.book_id  = be32(b + 13);
        out.exe.side     = static_cast<char>(b[17]);
        out.exe.exec_qty = be64(b + 18);
        out.exe.exec_price = 0;
        return true;
    }

    // 'D' Delete
    case 'D': {
        if (!need(p, 18)) return false;                 // up to side @ [17]
        out.type = ItchType::Delete;
        out.del.type     = out.type;
        out.del.ts_ns    = be32(b + 1);
        out.del.order_id = be64(b + 5);
        out.del.book_id  = be32(b + 13);
        out.del.side     = static_cast<char>(b[17]);
        return true;
    }
    default:
        return false;
    }
}
