#pragma once
#include <cstdint>
#include <vector>
#include <string>

enum class ItchType {
    Unknown,
    OrderBookState,   // 'O'
    AddOrder,         // 'A'
    Executed,         // 'E'
    Delete,           // 'D'
};

// Itch message structures, each message is constructed using the provided documents.
struct ItchBase {
    ItchType type;
    uint64_t ts_ns;    
    uint32_t book_id;  
    ItchBase() : type(ItchType::Unknown), ts_ns(0), book_id(0) {}
};

struct ItchOrderBookState : ItchBase {
    std::string state; 
};

struct ItchAddOrder : ItchBase {
    uint64_t order_id;
    char     side;      
    int32_t  price;     
    uint64_t qty;      
    ItchAddOrder() : order_id(0), side(0), price(0), qty(0) {}
};

struct ItchExecuted : ItchBase {
    uint64_t order_id;
    uint64_t exec_qty;   
    int32_t  exec_price; 
    char side;
    ItchExecuted() : order_id(0), exec_qty(0), exec_price(0) {}
};

struct ItchDelete : ItchBase {
    uint64_t order_id;
    char     side;
    ItchDelete() : order_id(0), side(0) {}
};

struct ItchMsg {
    ItchType type;
    ItchOrderBookState   obs;
    ItchAddOrder         add;
    ItchExecuted         exe;
    ItchDelete           del;
    ItchMsg() : type(ItchType::Unknown) {}
};

class ItchParser {
public:
    bool parse(const std::vector<unsigned char>& payload, ItchMsg& out) const;
};