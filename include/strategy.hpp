#pragma once
#include <cstdint>
#include <algorithm>
#include <string>
#include <utility>
#include "log.hpp"

#define CONSTANT_STATE "P_SUREKLI_ISLEM"
#define CLOSED_STATE "P_MARJ_YAYIN_KAPANIS"

struct StrategyParams {
    uint64_t order_qty = 100; 
    int64_t  pos_min  =  0;  
    int64_t  pos_max  =  10000;
    uint32_t tick_size = 10;
    StrategyParams() = default;
    StrategyParams(uint64_t oq, int64_t pmn, int64_t pmx, uint32_t ts)
        : order_qty(oq), pos_min(pmn), pos_max(pmx), tick_size(ts) {}
};

enum class MarketState { Unknown, Continuous, Closed };

struct Signal {
    char     side;     
    uint32_t price;     
    uint64_t qty;       
    uint64_t ts_ns;     
};

class Strategy {
public:
    explicit Strategy(StrategyParams p) : params(p) {}
    
    // Handle market state changes
    void on_state(const std::string& state) { 
        if (state == CONSTANT_STATE) {
            st = MarketState::Continuous;
            // log_info("STRAT", "Market state -> Continuous");
        } else if (state == CLOSED_STATE) {
            st = MarketState::Closed;
            // log_info("STRAT", "Market state -> Close");
        } else {
            st = MarketState::Unknown;
            // log_info("STRAT", "Market state -> Unknown (" + state + ")");
        }
    }

    // Update position on fills
    void on_fill(char side, uint64_t qty) {
        pos += (side == 'B' ? static_cast<int64_t>(qty) : -static_cast<int64_t>(qty));
        // log_info("STRAT", "FILL side=" + std::string(1, side) +
        //                   " qty=" + std::to_string(qty) +
        //                   " newPos=" + std::to_string(pos));
    }

    // Process order book changes; return (emitted, Signal)
    // If emitted is true, Signal contains a valid order to send
    // If emitted is false, Signal is invalid
    // It keeps the execution or deletion operation that formed a gap pending to check if any coming order has add at the same price that fills the gap 
    std::pair<bool, Signal> on_book_change(bool have_bid, uint32_t bid, bool have_ask, uint32_t ask, uint64_t ts_ns, bool is_add, uint32_t add_price = 0)
    {
        bool emitted = false;
        Signal out{};

        // 0) If timestamp advanced, flush any pending from previous ns first. This means we have a real gap
        if (pending.active && ts_ns != pending.ts) {
            auto maybe = finalize_pending();
            if (maybe.first) {
                emitted = true;
                out = maybe.second;
            }
        }

        // 1) Only act during continuous trading
        if (st != MarketState::Continuous) {
            snapshot(bid, have_bid, ask, have_ask, ts_ns);
            return {emitted, out};
        }

        // 2) Always keep snapshots up to date if one side missing or crossed
        if (!have_bid || !have_ask || ask < bid) {
            snapshot(bid, have_bid, ask, have_ask, ts_ns);
            return {emitted, out};
        }

        // 4) If we already have a pending candidate for this ns, check cancel condition on Add.
        if (pending.active && ts_ns == pending.ts && is_add) {
            if (add_price == pending.price) {
                //log_info("STRAT", "Cancel pending due to opposite-side ADD at same price in same ns");
                pending = Pending{}; // clear
            }
        }

        // 5) If no pending yet for this ns, and this event is a non-add that created a gap, park it.
        if (!is_add && !pending.active) {
            if (have_last_bid && have_last_ask && ((last_ask - last_bid) == params.tick_size) && ((ask - bid) == (2u * params.tick_size))) {
                const bool ask_moved_out = (ask > last_ask) && (bid == last_bid); // if ask moved out and bid stayed
                const bool bid_moved_out = (bid < last_bid) && (ask == last_ask); // if bid moved out and ask stayed

                if (ask_moved_out) {
                    pending.active = true;
                    pending.side   = 'S';
                    pending.price  = last_ask;
                    pending.ts     = ts_ns;
                    //log_info("STRAT", "Pending SELL (gap on ask) wait until ns boundary ");
                } else if (bid_moved_out) {
                    pending.active = true;
                    pending.side   = 'B';
                    pending.price  = last_bid;
                    pending.ts     = ts_ns;
                    //log_info("STRAT", "Pending BUY (gap on bid) wait until ns boundary");
                }
            }
        }

        snapshot(bid, have_bid, ask, have_ask, ts_ns); // always update snapshot at end
        return {emitted, out};
    }

    int64_t position() const { return pos; }
    MarketState state() const { return st; }

private:
    struct Pending {
        bool     active = false;
        char     side   = 0; 
        uint32_t price  = 0;
        uint64_t ts     = 0; 
    };

    std::pair<bool, Signal> finalize_pending() {
        if (!pending.active) return {false, Signal{}};

        uint64_t desired = params.order_qty;
        if (pending.side == 'B') {
            int64_t room = params.pos_max - pos; // how much more we can buy
            if (room <= 0) { pending = Pending{}; return {false, Signal{}}; }
            uint64_t room_u = static_cast<uint64_t>(room);
            if (desired > room_u) desired = room_u;
        } else {
            int64_t room = pos - params.pos_min; // how much more we can sell
            if (room <= 0) { pending = Pending{}; return {false, Signal{}}; }
            uint64_t room_u = static_cast<uint64_t>(room);
            if (desired > room_u) desired = room_u;
        }
        if (desired == 0) { pending = Pending{}; return {false, Signal{}}; }

        Signal s{pending.side, pending.price, desired, pending.ts};
        // log_info("STRAT", std::string("EMIT delayed signal side=") + s.side +
        //                   " price=" + std::to_string(s.price) +
        //                   " qty="   + std::to_string(s.qty)  +
        //                   " ts="    + std::to_string(s.ts_ns));
        pending = Pending{};
        return {true, s};
    }

    void snapshot(uint32_t bid, bool have_bid, uint32_t ask, bool have_ask, uint64_t ts) {
        if (have_bid) { last_bid = bid; have_last_bid = true; } else { have_last_bid = false; }
        if (have_ask) { last_ask = ask; have_last_ask = true; } else { have_last_ask = false; }
        last_ts_ns = ts;
    }

    StrategyParams params;
    MarketState st = MarketState::Unknown;
    int64_t  pos = 0;

    uint32_t last_bid = 0, last_ask = 0;
    bool     have_last_bid = false, have_last_ask = false;
    uint64_t last_ts_ns = 0;

    Pending pending;
};
