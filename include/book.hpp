#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include "log.hpp"

struct OBOrder {
    uint64_t id = 0;
    char side = 'B'; 
    uint32_t price = 0;  
    uint32_t qty = 0;
    uint64_t ts_ns = 0; 
    OBOrder() = default;
    OBOrder(uint64_t i, char s, uint32_t p, uint32_t q, uint64_t ns=0)
        : id(i), side(s), price(p), qty(q), ts_ns(ns) {}
};


class OrderBook {
public:

    // Inline key creation for by_id map
    static inline uint64_t make_key(uint64_t id, char side) noexcept {
        return (id << 1) | (side == 'S' ? 1ull : 0ull);
    }

    // Add a new order
    // Create key and keep in by_id map
    // Update bids or asks map
    void on_add(uint64_t id, char side, uint32_t price, uint32_t qty, uint64_t ts_ns) {
        if (qty == 0) return;
        OBOrder o{id, side, price, qty, ts_ns};
        by_id[make_key(id, side)] = o;

        auto& book = (side == 'B') ? bids : asks;
        book[price] += qty;
    }

    // Execute an order
    // Find order by id and side
    // Update or remove from bids or asks map
    // Update last executed price
    void on_exec(uint64_t id, char side, uint32_t exec_qty, uint64_t ts_ns) {
        auto it = by_id.find(make_key(id, side));
        if (it == by_id.end() || exec_qty == 0) return;

        OBOrder& o = it->second;
        if (exec_qty > o.qty) exec_qty = o.qty;
        o.qty -= exec_qty;
        o.ts_ns = ts_ns;

        auto& book = (o.side == 'B') ? bids : asks;
        auto pit = book.find(o.price);
        if (pit != book.end()) {
            if (pit->second <= exec_qty) book.erase(pit);
            else                         pit->second -= exec_qty;
        }
        if (o.qty == 0) by_id.erase(it);

        last_executed_price_ = o.price;
    }

    // Delete an order
    // Find order by id and side
    // Update or remove from bids or asks map
    // Remove from by_id map
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
    }

    // Get best bid price; return false if no bids
    bool best_bid(uint32_t &px) const {
        if (bids.empty()) return false;
        px = bids.rbegin()->first;
        return true;
    }
    // Get best ask price; return false if no asks
    bool best_ask(uint32_t &px) const {
        if (asks.empty()) return false;
        px = asks.begin()->first;
        return true;
    }


    uint32_t last_executed_price() const { return last_executed_price_; }

private:
    std::map<uint32_t, uint64_t> bids; // ascending, so best bid is rbegin(), O(1)
    std::map<uint32_t, uint64_t> asks; // ascending, so best ask is begin(), O(1)

    std::unordered_map<uint64_t, OBOrder> by_id; // O(1) access by (id,side) key

    uint32_t last_executed_price_ = 0; // to finalize PnL with inventory
};
