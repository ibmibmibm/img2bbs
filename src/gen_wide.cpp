#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>

namespace {

static const uint16_t kB2U[] = {
#include "B2U.txt"
};
static const uint16_t kWides[] = {
#include "wide.txt"
};
std::string B2U(uint16_t code) {
    uint16_t unicode = kB2U[code - 0x8000];
    uint8_t result[4];
    size_t size = 0;
    if (unicode == 0) {
        return std::string();
    } else if (unicode < 0x80) {
        result[0] = unicode;
        size = 1;
    } else if (unicode < 0x800) {
        result[0] = 0xC0 | (unicode >> 6);
        result[1] = 0x80 | (unicode & 0x3F);
        size = 2;
    } else /*if (unicode < 0x10000)*/ {
        result[0] = 0xE0 | (unicode >> 12);
        result[1] = 0x80 | ((unicode >> 6) & 0x3F);
        result[2] = 0x80 | (unicode & 0x3F);
        size = 3;
    }
    return std::string(result, result + size);
}

}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << std::hex << std::uppercase;
    for (const auto &i: kWides) {
        std::cout << "0x" << std::setw(4) << std::setfill('0') << i
            << ", // U+" << std::setw(4) << std::setfill('0')
            << kB2U[i - 0x8000] << ' ' << B2U(i) << std::endl;
    }
}
