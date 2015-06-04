#include <cwchar>
#include <iostream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <sys/ioctl.h>
#include "library.hpp"
#include "screen.hpp"

namespace {

static const cv::Scalar kColor[] = {
    {0x00, 0x00, 0x00}, // DarkBlack
    {0x00, 0x00, 0x80}, // DarkRed
    {0x00, 0x80, 0x00}, // DarkGreen
    {0x00, 0x80, 0x80}, // DarkYellow
    {0x80, 0x00, 0x00}, // DarkBlue
    {0x80, 0x00, 0x80}, // DarkMagenta
    {0x80, 0x80, 0x00}, // DarkCyan
    {0xc0, 0xc0, 0xc0}, // DarkWhite
    {0x80, 0x80, 0x80}, // LightBlack
    {0x00, 0x00, 0xff}, // LightRed
    {0x00, 0xff, 0x00}, // LightGreen
    {0x00, 0xff, 0xff}, // LightYellow
    {0xff, 0x00, 0x00}, // LightBlue
    {0xff, 0x00, 0xff}, // LightMagenta
    {0xff, 0xff, 0x00}, // LightCyan
    {0xff, 0xff, 0xff}, // LightWhite
};

static const uint16_t kSymbols[15] = {
    0xA262, // U+2581 ▁
    0xA263, // U+2582 ▂
    0xA264, // U+2583 ▃
    0xA265, // U+2584 ▄
    0xA266, // U+2585 ▅
    0xA267, // U+2586 ▆
    0xA268, // U+2587 ▇
    0xA269, // U+2588 █
    0xA270, // U+2589 ▉
    0xA26F, // U+258A ▊
    0xA26E, // U+258B ▋
    0xA26D, // U+258C ▌
    0xA26C, // U+258D ▍
    0xA26B, // U+258E ▎
    0xA26A, // U+258F ▏
};
static const uint8_t kSymbolsSize = sizeof(kSymbols) / sizeof(kSymbols[0]);

static const uint16_t kWides[] = {
#include "wide.txt"
};
static const uint16_t kWidesSize = sizeof(kWides) / sizeof(kWides[0]);
static uint16_t kWidesU[kWidesSize];


static const uint16_t kB2U[] = {
#include "B2U.txt"
};

static FT::Library lib;
static std::shared_ptr<FT::Face> face;

cv::Mat kChar[0x7F - 0x20][2]; // 0x20-0x7E
cv::Mat kWideChar[kWidesSize][2][2]; // 0x20-0x7E

inline uint16_t B2U(uint16_t x) {
    return kB2U[x - 0x8000];
}

inline uint16_t merge(const Char &lhs, const Char &rhs) {
    return static_cast<uint16_t>(lhs.code) * 256 + rhs.code;
}

inline void split(Char &lhs, Char &rhs, uint16_t code) {
    lhs.code = code / 256;
    rhs.code = code % 256;
}

inline double err(const cv::Mat & img1, const cv::Mat & img2) {
    cv::Mat tmp;
    cv::absdiff(img1, img2, tmp);
    cv::Scalar v = cv::sum(tmp);
    return v[0] + v[1] + v[2];
}

inline double eq(const cv::Mat & img1, const cv::Mat & img2) {
    cv::Mat tmp;
    cv::subtract(img1, img2, tmp);
    tmp.convertTo(tmp, CV_16UC3);
    cv::pow(tmp, 2, tmp);
    cv::Scalar v = cv::sum(tmp);
    return v[0] + v[1] + v[2];
}

inline double eqm(const cv::Mat & img1, const cv::Mat & img2) {
    return eq(img1, img2) / (img1.total() * 3);
}

double get_psnr(const cv::Mat & img_a, const cv::Mat & img_b) {
    const int D = 255;
    return (10 * log10((D*D) / eqm(img_a, img_b)));
}

unsigned int term_width() {
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return w.ws_col;
}

std::mutex pool_acquire_m;
std::condition_variable pool_acquire_cv;
std::mutex pool_release_m;
std::condition_variable pool_release_cv;

} // namespace

void Screen::init() {
    //face = lib.new_face("/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc", 0);
    //face = lib.new_face("/usr/share/fonts/TTF/DejaVuSerifCondensed.ttf", 0);
    //face = lib.new_face("/usr/share/fonts/TTF/VeraMono.ttf", 0);
    face = lib.new_face("/usr/share/fonts/TTF/ukai.ttc", 0);
    for (uint32_t unicode = 0x20; unicode <= 0x7E; ++unicode) {
        cv::Mat c = cv::Mat::zeros(kCharH, kCharW, CV_8UC3);
        face->putChar(c, unicode);
        cv::Mat d = cv::Scalar(255, 255, 255) - c;
        c.copyTo(kChar[unicode - 0x20][0]);
        d.copyTo(kChar[unicode - 0x20][1]);
    }
    for (size_t index = 0; index < kWidesSize; ++index) {
        kWidesU[index] = B2U(kWides[index]);
        cv::Mat c = cv::Mat::zeros(kCharH, kCharW * 2, CV_8UC3);
        face->putChar(c, B2U(kWides[index]));
        cv::Mat d = cv::Scalar(255, 255, 255) - c;
        c(cv::Rect(0, 0, kCharW, kCharH)).copyTo(kWideChar[index][0][0]);
        d(cv::Rect(0, 0, kCharW, kCharH)).copyTo(kWideChar[index][0][1]);
        c(cv::Rect(kCharW, 0, kCharW, kCharH)).copyTo(kWideChar[index][1][0]);
        d(cv::Rect(kCharW, 0, kCharW, kCharH)).copyTo(kWideChar[index][1][1]);
    }
}

void Screen::putChar(cv::Mat & img, const cv::Rect & pos, int mode, const cv::Scalar & fgcolor, const cv::Scalar & bgcolor, uint32_t unicode) {
    static const cv::Point left(+kCharW / 2, 0);
    static const cv::Point right(-kCharW / 2, 0);

    switch (unicode) {
    case 0x2588: // █
    {
        cv::rectangle(img, pos, fgcolor, CV_FILLED);
        return;
    }
    case 0:
    case ' ':
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        return;
    }
    case 0x2581: // ▁
    case 0x2582: // ▂
    case 0x2583: // ▃
    case 0x2584: // ▄
    case 0x2585: // ▅
    case 0x2586: // ▆
    case 0x2587: // ▇
    {
        cv::rectangle(img, pos.tl(), pos.br() - cv::Point(0, pos.height * (unicode - 0x2580) / 8), bgcolor, CV_FILLED);
        cv::rectangle(img, pos.tl() + cv::Point(0, pos.height * (0x2588 - unicode) / 8), pos.br(), fgcolor, CV_FILLED);
        return;
    }
    case 0x2589: // ▉
    case 0x258A: // ▊
    case 0x258B: // ▋
    {
        if (mode == 1) {
            cv::rectangle(img, pos, fgcolor, CV_FILLED);
        } else {
            cv::rectangle(img, pos.tl(), pos.br() - cv::Point(pos.width * (unicode - 0x2588) / 4, 0), fgcolor, CV_FILLED);
            cv::rectangle(img, pos.tl() + cv::Point(pos.width * (0x258C - unicode) / 4, 0), pos.br(), bgcolor, CV_FILLED);
        }
        return;
    }
    case 0x258C: // ▌
    {
        if (mode == 1) {
            cv::rectangle(img, pos, fgcolor, CV_FILLED);
        } else {
            cv::rectangle(img, pos, bgcolor, CV_FILLED);
        }
        return;
    }
    case 0x258D: // ▍
    case 0x258E: // ▎
    case 0x258F: // ▏
    {
        if (mode == 2) {
            cv::rectangle(img, pos, bgcolor, CV_FILLED);
        } else {
            cv::rectangle(img, pos.tl(), pos.br() - cv::Point(pos.width * (unicode - 0x258C) / 4, 0), fgcolor, CV_FILLED);
            cv::rectangle(img, pos.tl() + cv::Point(pos.width * (0x2590 - unicode) / 4, 0), pos.br(), bgcolor, CV_FILLED);
        }
        return;
    }
    default:
        cv::Mat c;
        cv::Mat d;
        if (0x20 <= unicode && unicode <= 0x7E) {
            c = kChar[unicode - 0x20][0];
            d = kChar[unicode - 0x20][1];
        } else {
            auto it = std::find(std::begin(kWidesU), std::end(kWidesU), unicode);
            if (it == std::end(kWides)) {
                abort();
            }
            auto index = std::distance(std::begin(kWidesU), it);
            if (mode == 1) {
                c = kWideChar[index][0][0];
                d = kWideChar[index][0][1];
            } else {
                c = kWideChar[index][1][0];
                d = kWideChar[index][1][1];
            }
        }
        img(pos) = c.mul(fgcolor, 1.0 / 255.0) + d.mul(bgcolor, 1.0 / 255.0);
    }
}

void Screen::render(cv::Mat &img) {
    img = cv::Mat::zeros(m_ref.rows, m_ref.cols, m_ref.type());
    for (size_t y = 0; y < m_screen.size(); ++y) {
        const auto & row = m_screen[y];
        for (size_t x = 0; x < row.size(); ++x) {
            const auto & c = row[x];
            if (c.is_wide() && x + 1 < row.size()) {
                auto & d = row[x+1];
                const auto code = B2U(merge(c, d));
                putChar(img, cv::Rect(cv::Point(x * kCharW, y * kCharH), cv::Size(kCharW, kCharH)),
                    1, kColor[c.fg], kColor[c.bg], code);
                putChar(img, cv::Rect(cv::Point((x+1) * kCharW, y * kCharH), cv::Size(kCharW, kCharH)),
                    2, kColor[d.fg], kColor[d.bg], code);
                x += 1;
            } else {
                putChar(img, cv::Rect(x * kCharW, y * kCharH, kCharW, kCharH),
                    0, kColor[c.fg], kColor[c.bg], c.code);
            }
        }
    }
    psnr = get_psnr(img, m_ref);
}

std::string Screen::to_string() const noexcept {
    typedef Char::FGColor FGColor;
    typedef Char::BGColor BGColor;
    std::ostringstream os;
    std::string seq;
    FGColor fg = FGColor::DarkWhite;
    BGColor bg = BGColor::Black;
    for (size_t y = 0; y < m_screen.size(); ++y) {
        const auto & row = m_screen[y];
        for (size_t x = 0; x < row.size(); ++x) {
            const auto & c = row[x];
            if (fg <= FGColor::DarkWhite && FGColor::LightBlack <= c.fg) {
                seq += "1;";
            } else if (c.fg <= FGColor::DarkWhite && FGColor::LightBlack <= fg) {
                seq += ";";
                fg = FGColor::DarkWhite;
                bg = BGColor::Black;
            }
            if ((fg % 8) != (c.fg % 8)) {
                seq += std::to_string((c.fg % 8) + 30) + ';';
            }
            if (bg != c.bg) {
                seq += std::to_string(c.bg + 40) + ';';
            }
            if (!seq.empty()) {
                // 0x1B Ctrl-[
                // 0x15 Ctrl-U
                seq.resize(seq.size() - 1);
                os << "\x1B[" << seq << 'm';
            }
            os << static_cast<char>(c.code);
            fg = c.fg;
            bg = c.bg;
            seq.clear();
        }
        os << "\r\n";
    }
    return os.str();
}

std::ostream & operator<<(std::ostream & os, const Screen &s) {
    typedef Char::FGColor FGColor;
    typedef Char::BGColor BGColor;
    FGColor fg = FGColor::DarkWhite;
    BGColor bg = BGColor::Black;
    std::string seq;
    for (size_t y = 0; y < s.m_screen.size(); ++y) {
        const auto & row = s.m_screen[y];
        for (size_t x = 0; x < row.size(); ++x) {
            const auto & c = row[x];
            if (fg <= FGColor::DarkWhite && FGColor::LightBlack <= c.fg) {
                seq += "1;";
            } else if (c.fg <= FGColor::DarkWhite && FGColor::LightBlack <= fg) {
                seq += ";";
                fg = FGColor::DarkWhite;
                bg = BGColor::Black;
            }
            if ((fg % 8) != (c.fg % 8)) {
                seq += std::to_string((c.fg % 8) + 30) + ';';
            }
            if (bg != c.bg) {
                seq += std::to_string(c.bg + 40) + ';';
            }
            if (!seq.empty()) {
                // 0x1B Ctrl-[
                // 0x15 Ctrl-U
                seq.resize(seq.size() - 1);
                os << "\x1B[" << seq << 'm';
            }
            if (c.is_wide() && x + 1 < row.size()) {
                auto & d = row[x+1];
                std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8conv;
                std::cout << utf8conv.to_bytes(B2U(merge(c, d)));
                x += 1;
            } else {
                os << static_cast<char>(c.code);
            }
            fg = c.fg;
            bg = c.bg;
            seq.clear();
        }
        os << std::endl;
    }
    return os;
}

double Screen::best_optimize(cv::Mat &img, Char &c, const cv::Mat &ref, int mode, size_t x, size_t y, uint16_t code) {
    static const cv::Rect roi(cv::Point(0, 0), cv::Size(kCharW, kCharH));
    const cv::Rect rect(cv::Point(x * kCharW, y * kCharH), cv::Size(kCharW, kCharH));
    double curr_eq = 255 * 255 * kCharW * kCharH * 3;
    for (size_t fg = Char::FGColor::DarkBlack; fg <= Char::FGColor::LightWhite; ++fg) {
        for (size_t bg = Char::BGColor::Black; bg <= Char::BGColor::White; ++bg) {
            putChar(img, roi, mode, kColor[fg], kColor[bg], code);
            double new_eq = err(img, ref(rect));
            if (new_eq < curr_eq) {
                c.fg = static_cast<Char::FGColor>(fg);
                c.bg = static_cast<Char::BGColor>(bg);
                curr_eq = new_eq;
            }
        }
    }
    return curr_eq;
}

void Screen::sample() {
    typedef Char::FGColor FGColor;
    typedef Char::BGColor BGColor;
    BGColor bg = BGColor::Black;
    FGColor fg = FGColor::DarkBlack;
    for (size_t y = 0; y < m_screen.size(); ++y) {
        auto & row = m_screen[y];
        for (size_t x = 0; x < row.size(); ++x) {
            auto & c = row[x];
            const size_t index = (y * row.size() + x) / 2 % (kSymbolsSize + kWidesSize);
            const uint16_t code = (index < kSymbolsSize ? kSymbols[index] : kWides[index - kSymbolsSize]);
            c.code = (x % 2 == 0 ? code / 256 : code % 256);
            //c.code = ((y * row.size() + x) % (0x7f - 0x20)) + 0x20;
            c.fg = fg;
            c.bg = bg;
            bg = static_cast<BGColor>(static_cast<unsigned int>(bg) + 1);
            if (bg > BGColor::White) {
                bg = BGColor::Black;
                fg = static_cast<FGColor>(static_cast<unsigned int>(fg) + 1);
                if (fg > FGColor::LightWhite) {
                    fg = FGColor::DarkBlack;
                }
            }
        }
    }
}

void Screen::best() {
    m_progress.store(0, std::memory_order_release);
    std::thread work([this] () {
        cv::Mat img = cv::Mat::zeros(kCharH, kCharW, m_ref.type());
        const cv::Rect roi(cv::Point(0, 0), cv::Size(kCharW, kCharH));
        struct {
            double tide_eq, wide_eq;
            uint8_t tide_code;
            uint16_t wide_code;
            Char::FGColor tide_fg, wide1_fg, wide2_fg;
            Char::BGColor tide_bg, wide1_bg, wide2_bg;
            bool wide;
            double eq() const {return wide ? wide_eq : tide_eq;}
        } table[kScreenW];
        Char c, d;

        for (size_t y = 0; y < kScreenH; ++y) {
            auto & row = m_screen[y];
            table[0].tide_eq = 255 * 255 * kCharW * kCharH * 3;
            table[0].wide_eq = 255 * 255 * kCharW * kCharH * 3;
            {
                for (uint8_t code = 0x20; code < 0x7F; ++code) {
                    double eq;
                    eq = best_optimize(img, c, m_ref, 0, 1, y, code);
                    if (eq < table[0].tide_eq) {
                        table[0].tide_eq = eq;
                        table[0].tide_code = code;
                        table[0].tide_fg = c.fg;
                        table[0].tide_bg = c.bg;
                    }
                }
                table[0].wide = false;
                ++m_progress;
            }
            for (size_t x = 1; x < row.size(); ++x) {
                table[x].tide_eq = 255 * 255 * kCharW * kCharH * 3;
                table[x].wide_eq = 255 * 255 * kCharW * kCharH * 3;
                for (uint8_t code = 0x20; code < 0x7F; ++code) {
                    double eq;
                    eq = best_optimize(img, c, m_ref, 0, x, y, code);
                    if (eq < table[x].tide_eq) {
                        table[x].tide_eq = eq;
                        table[x].tide_code = code;
                        table[x].tide_fg = c.fg;
                        table[x].tide_bg = c.bg;
                    }
                }
                for (size_t idx = 0; idx < kSymbolsSize + kWidesSize; ++idx) {
                    const uint16_t code = (idx < kSymbolsSize ? kSymbols[idx] : kWides[idx - kSymbolsSize]);
                    uint32_t eq1;
                    eq1 = best_optimize(img, c, m_ref, 1, x-1, y, B2U(code));
                    uint32_t eq2;
                    eq2 = best_optimize(img, d, m_ref, 2, x, y, B2U(code));
                    if (eq1 + eq2 < table[x].wide_eq) {
                        table[x].wide_eq = eq1 + eq2;
                        table[x].wide_code = code;
                        table[x].wide1_fg = c.fg;
                        table[x].wide1_bg = c.bg;
                        table[x].wide2_fg = d.fg;
                        table[x].wide2_bg = d.bg;
                    }
                }
                table[x].tide_eq += table[x - 1].eq();
                if (x >= 2) {table[x].wide_eq += table[x - 2].eq();}
                if (table[x].tide_eq > table[x].wide_eq) {
                    table[x].wide = true;
                } else {
                    table[x].wide = false;
                }
                ++m_progress;
            }
            //last_diff = cv::Scalar();
            for (size_t x = row.size() - 1; x < row.size(); --x) {
                if (table[x].wide) {
                    split(row[x-1], row[x], table[x].wide_code);
                    row[x].fg = table[x].wide2_fg;
                    row[x].bg = table[x].wide2_bg;
                    row[x-1].fg = table[x].wide1_fg;
                    row[x-1].bg = table[x].wide1_bg;
                    --x;
                } else {
                    row[x].code = table[x].tide_code;
                    row[x].fg = table[x].tide_fg;
                    row[x].bg = table[x].tide_bg;
                }
            }
            /* reduce string length by changing space character's fg */
            for (size_t x = 0; x < row.size() - 1; ++x) {
                if (table[x].wide) {
                    switch (merge(row[x], row[x+1])) {
                    case 0xA269: // U+2588 █
                    case 0xA270: // U+2589 ▉
                    case 0xA26F: // U+258A ▊
                        // bg color for left byte is nonsence
                        row[x].bg = row[x + 1].bg;
                        break;
                    case 0xA26D: // U+258C ▌
                    case 0xA26C: // U+258D ▍
                    case 0xA26B: // U+258E ▎
                    case 0xA26A: // U+258F ▏
                        // fg color for right byte is nonsence
                        row[x + 1].fg = row[x].fg;
                        break;
                    case 0xA26E: // U+258B ▋
                        // both case
                        row[x].bg = row[x + 1].bg;
                        row[x + 1].fg = row[x].fg;
                    default:
                        break;
                    }
                    ++x;
                    continue;
                }
                if (row[x].code == ' ') {
                    if (x > 0) {
                        row[x].fg = row[x - 1].fg;
                    } else if (y > 0) {
                        row[x].fg = m_screen[y - 1][row.size() - 1].fg;
                    } else {
                        row[x].fg = Char::FGColor::DarkWhite;
                    }
                }
            }
        }
    });
    std::thread display([this]() {
        auto chrono_begin = std::chrono::system_clock::now();
        const unsigned int count = kScreenW * kScreenH;
        unsigned int progress;
        while (progress = m_progress.load(std::memory_order_acquire), progress < count) {
            auto chrono_now = std::chrono::system_clock::now();
            const unsigned int width = term_width();
            const unsigned int bar_width = (width > 20 ? width - 20 : 0);
            const unsigned int len = bar_width * progress / count;
            std::cout << '[';
            for (unsigned int i = 0; i < len; ++i) {
                std::cout << '=';
            }
            std::cout.put('>');
            for (unsigned int i = 0; i < bar_width - len; ++i) {
                std::cout << '-';
            }
            std::cout << "] " << std::setw(4) << std::setfill(' ') << progress << '/' << std::setw(4) << count;
            if (progress) {
                std::chrono::duration<double> diff = chrono_now - chrono_begin;
                int est = (diff.count() * count / progress) - diff.count();
                std::cout << ' ' << std::setw(2) << std::setfill('0') << est / 60 << 'm'
                    << std::setw(2) << std::setfill('0') << est % 60 << 's';
            } else {
                std::cout << " ??m??s";
            }
            std::cout << "\n\033[1A";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        const unsigned int width = term_width();
        const unsigned int bar_width = (width > 20 ? width - 20 : 0);
        for (unsigned int i = 0; i < bar_width; ++i) {
            std::cout << '=';
        }
        std::cout << "]\n";
    });
    work.join();
    display.join();
}
