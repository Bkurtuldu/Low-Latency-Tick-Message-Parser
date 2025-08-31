#include <fstream>
#include <cstdint>
#include <itch.hpp>

enum class WriteType { Add, Delete, Execute, State };

static inline void write_add(std::ofstream& out, uint32_t book_id, uint64_t id, char side, uint32_t px, uint32_t qty, uint64_t ns) {
    out << "A  book=" << book_id
        << " id=" << id
        << " side=" << side
        << " px=" << px
        << " qty=" << qty
        << " ns=" << ns << "\n";
}

static inline void write_del(std::ofstream& out, uint32_t book_id, uint64_t id, char side, uint64_t ns) {
    out << "D  book=" << book_id
        << " id=" << id
        << " side=" << side
        << " ns=" << ns << "\n";
}

static inline void write_exec(std::ofstream& out, uint32_t book_id, uint64_t id, char side, uint32_t qty, uint64_t ns) {
    out << "E  book=" << book_id
        << " id=" << id
        << " side=" << side
        << " qty=" << qty
        << " ns=" << ns << "\n";
}

static inline void write_state(std::ofstream& out, uint32_t book_id, uint64_t ns, const std::string& state) {
    out << "O  book=" << book_id
        << " ns=" << ns
        << " state=" << state << "\n";
}

static inline void write(std::ofstream& out, ItchMsg msg) {
    switch(msg.type) {
        case ItchType::AddOrder:
            write_add(out, msg.add.book_id, msg.add.order_id, msg.add.side, msg.add.price, static_cast<uint32_t>(msg.add.qty), msg.add.ts_ns);
            break;
        case ItchType::Delete:
            write_del(out, msg.del.book_id, msg.del.order_id, msg.del.side, msg.del.ts_ns);
            break;
        case ItchType::Executed:
            write_exec(out, msg.exe.book_id, msg.exe.order_id, msg.exe.side, static_cast<uint32_t>(msg.exe.exec_qty), msg.exe.ts_ns);
            break;
        case ItchType::OrderBookState:
            write_state(out, msg.obs.book_id, msg.obs.ts_ns, msg.obs.state);
            break;
        default:
            break;
    }
}


