#include <fstream>
#include <iostream>
#include <string>
#include "moldudp64.hpp"
#include "itch.hpp"
#include "book.hpp"
#include "strategy.hpp"
#include "pnl.hpp"
#include "helper.cpp"
#include <chrono>

#define CLOSED_STATE "P_MARJ_YAYIN_KAPANIS"
#define TICK_SIZE 10


int main(int argc, char** argv) {
    auto start = std::chrono::high_resolution_clock::now();
    if (argc < 5) { // We get arguments from the user : file, target_book_id, order_qty, pos_min, pos_max
        std::cerr << "Usage: " << (argc > 0 ? argv[0] : "reader")
                  << " <file.dat> <target_order_book_id> <order_qty> <pos_min> <pos_max>\n";
        return 1;
    }
    const uint32_t TARGET_BOOK = static_cast<uint32_t>(std::stoul(argv[2]));
    const uint64_t ORDER_QTY   = static_cast<uint64_t>(std::stoull(argv[3]));
    const int64_t  POS_MIN     = static_cast<int64_t>(std::stoll(argv[4]));
    const int64_t  POS_MAX     = static_cast<int64_t>(std::stoll(argv[5]));

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Could not open: " << argv[1] << "\n";
        return 1;
    }

    // If you want to output the order book events to a file, uncomment below

    // const std::string ob_filename = "order_book_" + std::to_string(TARGET_BOOK);
    // std::ofstream ob_out(ob_filename, std::ios::out | std::ios::trunc);
    // if (!ob_out) {
    //     std::cerr << "Could not open output file: " << ob_filename << "\n";
    //     return 1;
    // }

    Strategy strat(StrategyParams{ORDER_QTY, POS_MIN, POS_MAX, TICK_SIZE}); // trategy initialized

    MoldUDP64Reader reader(in);
    MoldHeader hdr;
    std::vector<MoldMessage> msgs;

    OrderBook book;
    PnL pnl;
    ItchParser parser;
    ItchMsg msg;
    uint32_t bb=0, ba=0; // best bid, best ask
    uint32_t add_price = 0;

    bool closed_for_book = false;
    bool have_bid = false, have_ask = false;

    while (reader.next_packet(hdr, msgs) && !closed_for_book) { // for each packet
        for (auto& m : msgs) { // for each message in the packet
            if (!parser.parse(m.payload, msg)) continue;
            if (closed_for_book) break; 

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

            // 1) Handle state messages (gate and close)
            if (is_state) {
                //write_state(ob_out, book_id, ts_ns, msg.obs.state); // write to file if needed
                // finalize PnL if closed with inventory
                strat.on_state(msg.obs.state);
                if (msg.obs.state == CLOSED_STATE) {
                    pnl.finalize_with_inventory(strat.position(), book.last_executed_price());
                    closed_for_book = true;
                    log_info("MAIN", "Closed for book " + std::to_string(book_id) +
                                     "; final P&L=" + std::to_string(pnl.value()) +
                                     " last_executed_price=" + std::to_string(book.last_executed_price()));
                    break;
                }
                continue;
            }

            if (is_add) {
                book.on_add(msg.add.order_id, msg.add.side, msg.add.price, msg.add.qty, ts_ns);
                add_price = msg.add.price;
            } else if (is_exec) {
                book.on_exec(msg.exe.order_id, msg.exe.side, msg.exe.exec_qty, ts_ns);
            } else if (is_del) {
                book.on_delete(msg.del.order_id, msg.del.side);
            }

            //write(ob_out, msg);

            // 3) Evaluate strategy on every book change
            have_bid = book.best_bid(bb); // updates bb
            have_ask = book.best_ask(ba); // updates ba

            auto res = strat.on_book_change(have_bid, bb, have_ask, ba, ts_ns, is_add, add_price); // process book change

            if (res.first) { // if signal is true, that means we sell or buy
                const Signal& s = res.second;
                strat.on_fill(s.side, s.qty); // update position in strategy
                pnl.on_fill(s.side, s.qty, s.price); // update PnL
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Execution time: " << elapsed.count() << " seconds\n";

    // ob_out.flush(); // needs to be uncommented if file output is used
    // ob_out.close();
    return 0;
}




