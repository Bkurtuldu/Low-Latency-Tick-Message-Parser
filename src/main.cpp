#include <fstream>
#include <iostream>
#include <string>
#include "moldudp64.hpp"
#include "itch.hpp"
#include "book.hpp"
#include "strategy.hpp"
#include "pnl.hpp"


// --- helpers to write one-line events in the requested format ---
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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << (argc > 0 ? argv[0] : "reader")
                  << " <file.dat> <target_order_book_id>\n";
        return 1;
    }
    const uint32_t TARGET_BOOK = static_cast<uint32_t>(std::stoul(argv[2]));

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Could not open: " << argv[1] << "\n";
        return 1;
    }

    // --- NEW: open per-book order book log file ---
    const std::string ob_filename = "order_book_" + std::to_string(TARGET_BOOK);
    std::ofstream ob_out(ob_filename, std::ios::out | std::ios::trunc);
    if (!ob_out) {
        std::cerr << "Could not open output file: " << ob_filename << "\n";
        return 1;
    }

    MoldUDP64Reader reader(in);
    MoldHeader hdr;
    std::vector<MoldMessage> msgs;

    OrderBook book;
    Strategy strat(StrategyParams{/*order_qty=*/50, /*pos_min=*/0, /*pos_max=*/1000, /*tick_size=*/20});
    PnL pnl;

    bool closed_for_book = false;

    while (reader.next_packet(hdr, msgs)) {
        ItchParser parser;
        for (auto& m : msgs) {
            ItchMsg msg;
            if (!parser.parse(m.payload, msg)) continue;

            // Filter: only process the target book
            uint32_t book_id = 0;
            uint64_t ts_ns = 0;
            bool is_add=false, is_exec=false, is_del=false, is_state=false;

            switch (msg.type) {
            case ItchType::OrderBookState:
                book_id = msg.obs.book_id; ts_ns = msg.obs.ts_ns; is_state=true; break;
            case ItchType::AddOrder:
                book_id = msg.add.book_id; ts_ns = msg.add.ts_ns; is_add=true; break;
            case ItchType::Executed:
                book_id = msg.exe.book_id; ts_ns = msg.exe.ts_ns; is_exec=true; break;
            case ItchType::Delete:
                book_id = msg.del.book_id; ts_ns = msg.del.ts_ns; is_del=true; break;
            default: break;
            }
            if (book_id != TARGET_BOOK) continue;

            // Stop trading after close for this book; keep parsing but ignore
            if (closed_for_book) continue;

            // 1) Handle state messages (gate and close)
            if (is_state) {
                write_state(ob_out, book_id, ts_ns, msg.obs.state);

                strat.on_state(msg.obs.state);
                if (msg.obs.state == "P_MARJ_YAYIN_KAPANIS") {
                    // At close: realize leftover inventory at last executed price
                    pnl.finalize_with_inventory(strat.position(), book.last_buy_price(), book.last_sell_price());
                    closed_for_book = true;
                    log_info("MAIN", "Closed for book " + std::to_string(book_id) +
                                     "; final P&L=" + std::to_string(pnl.value()) +
                                     " last_buy_px=" + std::to_string(book.last_buy_price()) +
                                     " last_sell_px=" + std::to_string(book.last_sell_price()) );
                }
                continue; // nothing else to do on a pure state message
            }

            // 2) Maintain a minimal order book from Adds/Executes/Deletes (and log to file)
            if (is_add) {
                book.on_add(msg.add.order_id, msg.add.side, msg.add.price, msg.add.qty);
                write_add(ob_out, book_id, msg.add.order_id, msg.add.side, msg.add.price, msg.add.qty, ts_ns);
            } else if (is_exec) {
                book.on_exec(msg.exe.order_id, msg.exe.side, msg.exe.exec_qty);
                write_exec(ob_out, book_id, msg.exe.order_id, msg.exe.side, msg.exe.exec_qty, ts_ns);
            } else if (is_del) {
                book.on_delete(msg.del.order_id, msg.del.side);
                write_del(ob_out, book_id, msg.del.order_id, msg.del.side, ts_ns);
            }

            // 3) Evaluate strategy on every book change
            uint32_t bb=0, ba=0;
            bool have_bid = book.best_bid(bb);
            bool have_ask = book.best_ask(ba);
            
            auto res = strat.on_book_change(have_bid, bb, have_ask, ba, ts_ns, is_exec, is_add);
            if (res.first) {
                const Signal& s = res.second;
                // Update position & PnL on assumed fill (per-trade accounting)
                strat.on_fill(s.side, s.qty /*, s.price*/);
                pnl.on_fill(s.side, s.qty, s.price);
            }
        }
    }

    ob_out.flush();
    ob_out.close();
    return 0;
}





