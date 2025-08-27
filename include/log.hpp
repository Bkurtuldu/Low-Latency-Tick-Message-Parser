#pragma once
#include <iostream>
#include <iomanip>
#include <string>

inline void log_info(const std::string& tag, const std::string& msg) {
    std::cout << "[" << tag << "] " << msg << std::endl;
}
