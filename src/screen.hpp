#pragma once

#include <iosfwd>
#include <vector>
#include <array>
#include <atomic>
#include "face.hpp"

struct Char {
    enum BGColor: uint8_t {
        Black,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
    };
    enum FGColor: uint8_t {
        DarkBlack,
        DarkRed,
        DarkGreen,
        DarkYellow,
        DarkBlue,
        DarkMagenta,
        DarkCyan,
        DarkWhite,
        LightBlack,
        LightRed,
        LightGreen,
        LightYellow,
        LightBlue,
        LightMagenta,
        LightCyan,
        LightWhite,
    };
    Char(): code(0), bg(Black), fg(DarkBlack) {}
    bool is_wide() const {return code > 0x7f;}
    uint8_t code;
    BGColor bg;
    FGColor fg;
};

namespace {
    enum {
        kCharW = 16,
        kCharH = 32,
        kScreenW = 79,
        kScreenH = 23,
    };
} // namespace

class Screen: public std::enable_shared_from_this<Screen> {
public:
    static void init();
    Screen(const cv::Mat &ref): m_ref(ref) {};
    void sample();
    void best();
    void render(cv::Mat &img);
    std::string to_string() const noexcept;
    double psnr;
private:
    std::array<std::array<Char, kScreenW>, kScreenH> m_screen;
    const cv::Mat &m_ref;
    std::atomic_uint m_progress;
    double best_optimize(cv::Mat &img, Char &c, const cv::Mat &ref, int mode, size_t x, size_t y, uint16_t code);
    void putChar(cv::Mat & img, const cv::Rect & pos, int mode, const cv::Scalar & fgcolor, const cv::Scalar & bgcolor, uint32_t unicode);
    friend std::ostream & operator<<(std::ostream & os, const Screen &s);
};

std::ostream & operator<<(std::ostream & os, const Screen &s);
