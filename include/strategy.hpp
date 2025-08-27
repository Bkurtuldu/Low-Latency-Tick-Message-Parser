#pragma once
#include <cstdint>
#include <algorithm>
#include <string>
#include <utility>
#include "log.hpp"

// Strategy parameters
struct StrategyParams {
    uint64_t order_qty = 100;   // fixed size orders (>=0)
    int64_t  pos_min  = -1000;  // can be negative
    int64_t  pos_max  =  1000;  // can be negative or positive, but pos_min <= pos_max
    uint32_t tick_size = 20;    // 2 kuruş = 1 tick (match your price units)
    StrategyParams() = default;
    StrategyParams(uint64_t oq, int64_t pmn, int64_t pmx, uint32_t ts)
        : order_qty(oq), pos_min(pmn), pos_max(pmx), tick_size(ts) {}
};

enum class MarketState { Unknown, Continuous, Closed };

struct Signal {
    char     side;      // 'B' buy, 'S' sell
    uint32_t price;     // send at the gap price
    uint64_t qty;       // quantity to send
    uint64_t ts_ns;     // total ns timestamp
};

class Strategy {
public:
    explicit Strategy(StrategyParams p) : params(p) {}

    void on_state(const std::string& state) {
        if (state == "P_SUREKLI_ISLEM") {
            st = MarketState::Continuous;
            log_info("STRAT", "Market state -> Continuous");
        } else if (state == "P_MARJ_YAYIN_KAPANIS") {
            st = MarketState::Closed;
            log_info("STRAT", "Market state -> Close");
        } else {
            st = MarketState::Unknown;
            log_info("STRAT", "Market state -> Unknown (" + state + ")");
        }
    }

    void on_fill(char side, uint64_t qty /*, uint32_t price*/) {
        // Buys increase position, sells decrease (signed math)
        pos += (side == 'B' ? static_cast<int64_t>(qty) : -static_cast<int64_t>(qty));
        log_info("STRAT", "FILL side=" + std::string(1, side) +
                          " qty=" + std::to_string(qty) +
                          " newPos=" + std::to_string(pos));
    }

    // Evaluate strategy on each book change
    std::pair<bool, Signal> on_book_change(bool have_bid, uint32_t bid,
                                           bool have_ask, uint32_t ask,
                                           uint64_t ts_ns,
                                           bool event_is_execution,
                                           bool event_is_add)
    {
        // 1) Only in continuous trading
        if (st != MarketState::Continuous) {
            log_info("STRAT", "Skip: not in continuous state");
            return {false, Signal{}};
        }

        // 2) Need both sides
        if (!have_bid || !have_ask) {
            log_info("STRAT", "Skip: one side missing");
            snapshot(bid, have_bid, ask, have_ask, ts_ns);
            return {false, Signal{}};
        }

        // 3) Spread must be exactly 1 tick
        if (ask < bid) {
            log_info("STRAT", "Skip: crossed book ask<bid");
            snapshot(bid, have_bid, ask, have_ask, ts_ns);
            return {false, Signal{}};
        }
        const uint32_t spread = ask - bid;
        if (spread != params.tick_size) {
            log_info("STRAT", "Bid=" + std::to_string(bid) +
                              " Ask=" + std::to_string(ask) +
                              " Spread=" + std::to_string(spread) +
                              " TickSize=" + std::to_string(params.tick_size));
            log_info("STRAT", "Skip: spread != tick_size");
            snapshot(bid, have_bid, ask, have_ask, ts_ns);
            return {false, Signal{}};
        }

        // 4) Same-ns reshuffle rule (exec then add in the SAME time)
        if (last_ts_ns == ts_ns) {
            log_info("STRAT", "Skip: same-ns");
            snapshot(bid, have_bid, ask, have_ask, ts_ns);
            return {false, Signal{}};
        }

        // if (last_ts_ns == ts_ns && last_event_was_execution && event_is_add) {
        //     log_info("STRAT", "Skip: same-ns exec->add reshuffle");
        //     snapshot(bid, have_bid, ask, have_ask, ts_ns);
        //     return {false, Signal{}};
        // }


        // 5) Detect which side disappeared (gap-origin side)
        char trade_side = 0;
        uint32_t send_price = 0;
        if (have_last_bid && have_last_ask) {
            const bool ask_moved_out = (ask > last_ask) && (bid == last_bid);
            const bool bid_moved_out = (bid < last_bid) && (ask == last_ask);
            if (ask_moved_out) {
                trade_side = 'S';
                send_price = ask;
                log_info("STRAT", "Ask disappeared -> SELL signal candidate");
            } else if (bid_moved_out) {
                trade_side = 'B';
                send_price = bid;
                log_info("STRAT", "Bid disappeared -> BUY signal candidate");
            } else {
                log_info("STRAT", "Skip: neither side disappeared");
            }
        } else {
            log_info("STRAT", "Skip: no previous snapshot");
        }

        // 6) Update snapshot/state for next call
        snapshot(bid, have_bid, ask, have_ask, ts_ns);
        last_event_was_execution = event_is_execution;
        last_event_was_add = event_is_add;

        if (!trade_side) return {false, Signal{}};

        // 7) Position limits with SIGNED math (pos_min/pos_max may be negative)
        uint64_t desired = params.order_qty;

        if (trade_side == 'B') {
            // Buying increases pos; don't exceed pos_max
            int64_t room = params.pos_max - pos;     // signed
            if (room <= 0) {
                log_info("STRAT", "Skip: pos >= pos_max");
                return {false, Signal{}};
            }
            uint64_t room_u = static_cast<uint64_t>(room);
            if (desired > room_u) desired = room_u;

        } else { // trade_side == 'S'
            // Selling decreases pos; don't go below pos_min
            int64_t room = pos - params.pos_min;     // signed
            if (room <= 0) {
                log_info("STRAT", "Skip: pos <= pos_min");
                return {false, Signal{}};
            }
            uint64_t room_u = static_cast<uint64_t>(room);
            if (desired > room_u) desired = room_u;
        }

        if (desired == 0) {
            log_info("STRAT", "Skip: desired qty=0 after clamp");
            return {false, Signal{}};
        }

        // 8) Emit signal
        Signal s{trade_side, send_price, desired, ts_ns};
        log_info("STRAT", std::string("Signal side=") + trade_side +
                          " price=" + std::to_string(send_price) +
                          " qty=" + std::to_string(desired) +
                          " ts=" + std::to_string(ts_ns));
        return {true, s};
    }

    int64_t position() const { return pos; }
    MarketState state() const { return st; }

private:
    void snapshot(uint32_t bid, bool have_bid, uint32_t ask, bool have_ask, uint64_t ts) {
        if (have_bid) { last_bid = bid; have_last_bid = true; } else { have_last_bid = false; }
        if (have_ask) { last_ask = ask; have_last_ask = true; } else { have_last_ask = false; }
        last_ts_ns = ts;
    }

    StrategyParams params;
    MarketState st = MarketState::Unknown;
    int64_t  pos = 0;                // signed live position

    // last snapshot of top of book
    uint32_t last_bid = 0, last_ask = 0;
    bool     have_last_bid = false, have_last_ask = false;
    uint64_t last_ts_ns = 0;
    bool     last_event_was_execution = false;
    bool     last_event_was_add = false;
};
