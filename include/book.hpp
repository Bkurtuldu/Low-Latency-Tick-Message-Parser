#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <log.hpp>

struct OBOrder {
    uint64_t id = 0;
    char side = 'B';          // 'B' or 'S'
    uint32_t price = 0;       // integer price (ticks/kuruş)
    uint32_t qty = 0;
    OBOrder() = default;
    OBOrder(uint64_t i, char s, uint32_t p, uint32_t q)
        : id(i), side(s), price(p), qty(q) {}
};

class OrderBook {
public:
    static std::string make_key(uint64_t id, char side) {
        return std::to_string(id) + side;
    }

    void on_add(uint64_t id, char side, uint32_t price, uint32_t qty) {
        if (qty == 0) return;
        OBOrder o{id, side, price, qty};
        by_id[make_key(id, side)] = o;

        auto& book = (side == 'B') ? bids : asks;
        book[price] += qty;

        // log_info("BOOK", "ADD id=" + std::to_string(id) +
        //                 " side=" + std::string(1, side) +
        //                 " px=" + std::to_string(price) +
        //                 " qty=" + std::to_string(qty));

        // std::string oline = "ORDERS: ";
        // for (const auto& kv : by_id) {
        //     const auto& o = kv.second;
        //     oline += "[id=" + std::to_string(o.id) +
        //              " side=" + std::string(1, o.side) +
        //              " px=" + std::to_string(o.price) +
        //              " qty=" + std::to_string(o.qty) + "] ";
        // }

        // dump_state("AFTER ADD");
    }

    void on_exec(uint64_t id, char side, uint32_t exec_qty) {
        auto it = by_id.find(make_key(id, side));
        if (it == by_id.end() || exec_qty == 0) return;

        OBOrder& o = it->second;
        if (exec_qty > o.qty) exec_qty = o.qty;
        o.qty -= exec_qty;

        auto& book = (o.side == 'B') ? bids : asks;
        auto pit = book.find(o.price);
        if (pit != book.end()) {
            if (pit->second <= exec_qty) book.erase(pit);
            else                         pit->second -= exec_qty;
        }
        if (o.qty == 0) by_id.erase(it);

        if (side == 'S') last_sell_price_ = o.price;
        else             last_buy_price_ = o.price;

        // log_info("BOOK", "EXEC id=" + std::to_string(id) +
        //                 " side=" + std::string(1, o.side) +
        //                 " px=" + std::to_string(o.price) +
        //                 " exec_qty=" + std::to_string(exec_qty));
    }

    void on_delete(uint64_t id, char side) {
        auto it = by_id.find(make_key(id, side));
        if (it == by_id.end()) return;
        OBOrder o = it->second;

        auto& book = (o.side == 'B') ? bids : asks;
        auto pit = book.find(o.price);
        if (pit != book.end()) {
            if (pit->second <= o.qty) book.erase(pit);
            else                      pit->second -= o.qty;
        }
        by_id.erase(it);

        // log_info("BOOK", "DEL id=" + std::to_string(id) +
        //                 " side=" + std::string(1, o.side) +
        //                 " px=" + std::to_string(o.price) +
        //                 " qty=" + std::to_string(o.qty));
    }

    // Best levels
    bool best_bid(uint32_t &px) const {
        if (bids.empty()) return false;
        px = bids.rbegin()->first; // highest price
        return true;
    }
    bool best_ask(uint32_t &px) const {
        if (asks.empty()) return false;
        px = asks.begin()->first; // lowest price
        return true;
    }

    uint32_t last_sell_price() const { return last_sell_price_; }
    uint32_t last_buy_price() const { return last_buy_price_; }

private:
    // price -> agg qty
    std::map<uint32_t, uint64_t> bids;                   // ascending; use rbegin for best
    std::map<uint32_t, uint64_t> asks;                   // ascending; begin is best
    std::unordered_map<std::string, OBOrder> by_id;         // order detail
    uint32_t last_sell_price_;
    uint32_t last_buy_price_;
 
    void dump_state(const std::string& tag) const {
        log_info("BOOK", "---- " + tag + " DUMP ----");

        // Print bids
        std::string bline = "BIDS: ";
        for (auto it = bids.rbegin(); it != bids.rend(); ++it) {
            bline += "[px=" + std::to_string(it->first) +
                     " qty=" + std::to_string(it->second) + "] ";
        }
        log_info("BOOK", bline);

        // Print asks
        std::string aline = "ASKS: ";
        for (auto it = asks.begin(); it != asks.end(); ++it) {
            aline += "[px=" + std::to_string(it->first) +
                     " qty=" + std::to_string(it->second) + "] ";
        }
        log_info("BOOK", aline);
    }
};
