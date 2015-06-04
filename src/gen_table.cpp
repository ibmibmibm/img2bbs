#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>

namespace {

static const uint16_t kB2U[] = {
#include "B2U.txt"
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
    using std::cout;
    using std::endl;
    cout << std::hex;
    cout << "<!DOCTYPE html>\n"
        "<html><head><meta charset=utf-8></head><body><table>\n"
        "<tr>\n"
        " <th></th>\n";
    for (uint16_t x = 0x0; x < 0x10; ++x) {
        cout << " <th>" << x << "</th>\n";
    }
    cout << "</tr>\n";
    for (uint16_t y = 0x8000; y >= 0x8000; y += 0x10) {
        cout << "<tr>\n"
            " <th>" << y << "</th>\n";
        for (uint16_t x = 0x0; x < 0x10; ++x) {
            cout << " <td>" << B2U(y + x) << "</td>\n";
        }
        cout << "</tr>\n";
    }
    cout << "</table></body>\n";
}
