#pragma once
#include <cstdint>

class PnL {
public:
    void on_fill(char side, uint32_t qty, uint32_t price_ticks) {
        const int64_t cash = int64_t(qty) * int64_t(price_ticks);
        if (side == 'S') pnl += cash;
        else             pnl -= cash;  // 'B'
    }

    void finalize_with_inventory(int64_t position, uint32_t last_executed_price) {
        if (position > 0) {
            // We are long; sell at last sell price
            const int64_t cash = position * int64_t(last_executed_price);
            pnl += cash;
            log_info("PNL", "Finalizing long inventory pos=" + std::to_string(position) +
                            " at last_executed_price=" + std::to_string(last_executed_price) +
                            " cash=" + std::to_string(cash) +
                            " new PnL=" + std::to_string(pnl));
        } else if (position < 0) {
            // We are short; buy at last buy price
            const int64_t cash = (-position) * int64_t(last_executed_price);
            pnl -= cash;
            log_info("PNL", "Finalizing short inventory pos=" + std::to_string(position) +
                            " at last_executed_price=" + std::to_string(last_executed_price) +
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
