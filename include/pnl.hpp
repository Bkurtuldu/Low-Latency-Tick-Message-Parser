#pragma once
#include <cstdint>

class PnL {
public:
    // Update P&L for our own fills:
    // Sell => +qty*price; Buy => -qty*price
    void on_fill(char side, uint32_t qty, uint32_t price_ticks) {
        const int64_t cash = int64_t(qty) * int64_t(price_ticks);
        if (side == 'S') pnl += cash;
        else             pnl -= cash;  // 'B'
    }

    // At close, realize any remaining inventory at the last executed price
    void finalize_with_inventory(int64_t position, uint32_t last_buy_price, uint32_t last_sell_price) {
        if (position > 0) {
            // We are long; sell at last sell price
            const int64_t cash = position * int64_t(last_sell_price);
            pnl += cash;
            log_info("PNL", "Finalizing long inventory pos=" + std::to_string(position) +
                            " at last_sell_price=" + std::to_string(last_sell_price) +
                            " cash=" + std::to_string(cash) +
                            " new PnL=" + std::to_string(pnl));
        } else if (position < 0) {
            // We are short; buy at last buy price
            const int64_t cash = (-position) * int64_t(last_buy_price);
            pnl -= cash;
            log_info("PNL", "Finalizing short inventory pos=" + std::to_string(position) +
                            " at last_buy_price=" + std::to_string(last_buy_price) +
                            " cash=" + std::to_string(cash) +
                            " new PnL=" + std::to_string(pnl));
        } else {
            log_info("PNL", "No inventory to finalize");
        }
    }

    int64_t value() const { return pnl; }

private:
    int64_t pnl = 0;
};
