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

static const uint16_t kSymbols[24] = {
    0xA158, // U+2014 —
    0xA1FD, // U+2223 ∣
    0xA2AC, // U+2571 ╱
    0xA2AD, // U+2572 ╲
    0xA2AE, // U+2573 ╳
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
    0xA2A8, // U+25E2 ◢
    0xA2A9, // U+25E3 ◣
    0xA2AB, // U+25E4 ◤
    0xA2AA, // U+25E5 ◥
};
static const uint8_t kSymbolsSize = 24;

static const uint16_t kB2U[] = {
#include "B2U.txt"
};

static FT::Library lib;
//static std::shared_ptr<FT::Face> face = lib.new_face("/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc", 0);
//static std::shared_ptr<FT::Face> face = lib.new_face("/usr/share/fonts/TTF/DejaVuSerifCondensed.ttf", 0);
static std::shared_ptr<FT::Face> face = lib.new_face("/usr/share/fonts/TTF/VeraMono.ttf", 0);

cv::Mat kChar[0x7F - 0x20]; // 0x20-0x7E

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

inline double eq(const cv::Mat & img1, const cv::Mat & img2) {
    cv::Mat tmp;
    cv::subtract(img1, img2, tmp);
    cv::pow(tmp, 2, tmp);
    auto v = cv::sum(tmp);
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
    for (uint32_t unicode = 0x20; unicode <= 0x7E; ++unicode) {
        kChar[unicode - 0x20] = cv::Mat::zeros(kCharH, kCharW, CV_64FC3);
        face->putChar(kChar[unicode - 0x20], unicode);
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
    case '_':
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        cv::line(img, pos.tl() + cv::Point(0, pos.height), pos.tl() + cv::Point(pos.width, pos.height), fgcolor, 1);
        return;
    }
    case 0x2014: // —
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        cv::line(img, pos.tl() + cv::Point(0, pos.height / 2), pos.tl() + cv::Point(pos.width, pos.height / 2), fgcolor, 1);
        return;
    }
    case 0x2223: // ∣
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        if (mode == 1) {
            cv::line(img, pos.tl() + cv::Point(pos.width, 0), pos.tl() + cv::Point(pos.width, pos.height), fgcolor, 1);
        } else {
            cv::line(img, pos.tl() + cv::Point(0, 0), pos.tl() + cv::Point(0, pos.height), fgcolor, 1);
        }
        return;
    }
    case 0x2571: // ╱
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        if (mode == 1) {
            cv::line(img, pos.tl() + cv::Point(0, pos.height), pos.tl() + cv::Point(pos.width, pos.height / 2), fgcolor, 1);
        } else {
            cv::line(img, pos.tl() + cv::Point(0, pos.height / 2), pos.tl() + cv::Point(pos.width, 0), fgcolor, 1);
        }
        return;
    }
    case 0x2572: // ╲
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        if (mode == 1) {
            cv::line(img, pos.tl() + cv::Point(0, 0), pos.tl() + cv::Point(pos.width, pos.height / 2), fgcolor, 1);
        } else {
            cv::line(img, pos.tl() + cv::Point(0, pos.height / 2), pos.tl() + cv::Point(pos.width, pos.height), fgcolor, 1);
        }
        return;
    }
    case 0x2573: // ╳
    {
        cv::rectangle(img, pos, bgcolor, CV_FILLED);
        if (mode == 1) {
            cv::line(img, pos.tl() + cv::Point(0, 0), pos.tl() + cv::Point(pos.width, pos.height / 2), fgcolor, 1);
            cv::line(img, pos.tl() + cv::Point(0, pos.height), pos.tl() + cv::Point(pos.width, pos.height / 2), fgcolor, 1);
        } else {
            cv::line(img, pos.tl() + cv::Point(0, pos.height / 2), pos.tl() + cv::Point(pos.width, 0), fgcolor, 1);
            cv::line(img, pos.tl() + cv::Point(0, pos.height / 2), pos.tl() + cv::Point(pos.width, pos.height), fgcolor, 1);
        }
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
    case 0x25E4: // ◤
    case 0x25E2: // ◢
    case 0x25E5: // ◥
    case 0x25E3: // ◣
    {
        const cv::Scalar *fg = &fgcolor;
        const cv::Scalar *bg = &bgcolor;
        cv::Point pts[3];
        switch (unicode) {
        case 0x25E2: // ◢
        case 0x25E4: // ◤
            if (mode == 1) {
                if (unicode == 0x25E4) std::swap(fg, bg);
                pts[0] = pos.tl() + cv::Point(pos.width, pos.height);
                pts[1] = pos.tl() + cv::Point(0, pos.height);
                pts[2] = pos.tl() + cv::Point(pos.width, pos.height / 2);
            } else {
                if (unicode == 0x25E2) std::swap(fg, bg);
                pts[0] = pos.tl() + cv::Point(0, 0);
                pts[1] = pos.tl() + cv::Point(pos.width, 0);
                pts[2] = pos.tl() + cv::Point(0, pos.height / 2);
            }
            break;
        case 0x25E3: // ◣
        case 0x25E5: // ◥
            if (mode == 1) {
                if (unicode == 0x25E3) std::swap(fg, bg);
                pts[0] = pos.tl() + cv::Point(0, 0);
                pts[1] = pos.tl() + cv::Point(pos.width, 0);
                pts[2] = pos.tl() + cv::Point(pos.width, pos.height / 2);
            } else {
                if (unicode == 0x25E5) std::swap(fg, bg);
                pts[0] = pos.tl() + cv::Point(pos.width, pos.height);
                pts[1] = pos.tl() + cv::Point(0, pos.height);
                pts[2] = pos.tl() + cv::Point(0, pos.height / 2);
            }
            break;
        }
        cv::rectangle(img, pos, *bg, CV_FILLED);
        const cv::Point * ppts = pts;
        int npt = 3;
        cv::fillPoly(img, &ppts, &npt, 1, *fg);
        return;
    }
    default:
        if (0x20 <= unicode && unicode <= 0x7E) {
            cv::Mat & c = kChar[unicode - 0x20];
            cv::Mat d = cv::Scalar(255, 255, 255) - c;
            cv::Mat e = c.mul(fgcolor, 1.0 / 255.0) + d.mul(bgcolor, 1.0 / 255.0);
            e.copyTo(img(pos));
        } else {
            abort();
            //face->putChar(img, pos, (mode == 0 ? cv::Point() : (mode == 1 ? left : right)), fgcolor, bgcolor, unicode);
        }
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
        os << "\n\n";
    }
    return os.str();
}

std::ostream & operator<<(std::ostream & os, const Screen &s) {
    for (size_t y = 0; y < s.m_screen.size(); ++y) {
        const auto & row = s.m_screen[y];
        for (size_t x = 0; x < row.size(); ++x) {
            const auto & c = row[x];
            if (c.is_wide() && x + 1 < row.size()) {
                auto & d = row[x+1];
                std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8conv;
                std::cout << utf8conv.to_bytes(B2U(merge(c, d))) << ' ';
                x += 1;
            } else {
                os << static_cast<char>(c.code);
            }
        }
        os << std::endl;
    }
    return os;
}

double Screen::best_optimize(cv::Mat &img, Char &c, const cv::Mat &ref, int mode, size_t x, size_t y, uint16_t code) {
    static const cv::Rect roi(cv::Point(0, 0), cv::Size(kCharW, kCharH));
    const cv::Rect rect(cv::Point(x * kCharW, y * kCharH), cv::Size(kCharW, kCharH));
    auto curr_eq = 255 * 255 * kCharW * kCharH * 3;
    for (size_t fg = Char::FGColor::DarkBlack; fg <= Char::FGColor::LightWhite; ++fg) {
        for (size_t bg = Char::BGColor::Black; bg <= Char::BGColor::White; ++bg) {
            putChar(img, roi, mode, kColor[fg], kColor[bg], code);
            auto new_eq = eq(img, ref(rect));
            if (new_eq < curr_eq) {
                c.fg = static_cast<Char::FGColor>(fg);
                c.bg = static_cast<Char::BGColor>(bg);
                curr_eq = new_eq;
            }
        }
    }
    return curr_eq;
}

void Screen::job(size_t y) {
    {
        std::unique_lock<std::mutex> lock(pool_acquire_m);
        pool_acquire_cv.wait(lock);
    }
    cv::Mat img = cv::Mat::zeros(kCharH, kCharW, m_ref.type());
    const cv::Rect roi(cv::Point(0, 0), cv::Size(kCharW, kCharH));
    struct {
        double tide_eq, wide_eq, result;
        uint8_t tide_code;
        uint16_t wide_code;
        Char::FGColor tide_fg, wide1_fg, wide2_fg;
        Char::BGColor tide_bg, wide1_bg, wide2_bg;
        bool wide;
    } table[80];
    Char c, d;
    auto & row = m_screen[y];
    table[0].tide_eq = 255 * 255 * kCharW * kCharH * 3;
    table[0].wide_eq = 255 * 255 * kCharW * kCharH * 3;
    {
        for (uint8_t code = 0x20; code < 0x7F; ++code) {
            double eq = best_optimize(img, c, m_ref, 0, 1, y, code);
            if (eq < table[0].tide_eq) {
                table[0].tide_eq = eq;
                table[0].tide_code = code;
                table[0].tide_fg = c.fg;
                table[0].tide_bg = c.bg;
            }
        }
        table[0].result = table[0].tide_eq;
        table[0].wide = false;
        ++m_progress;
    }
    for (size_t x = 1; x < row.size(); ++x) {
        table[x].tide_eq = 255 * 255 * kCharW * kCharH * 3;
        table[x].wide_eq = 255 * 255 * kCharW * kCharH * 3;
        for (uint8_t code = 0x20; code < 0x7F; ++code) {
            double eq = best_optimize(img, c, m_ref, 0, x, y, code);
            if (eq < table[x].tide_eq) {
                table[x].tide_eq = eq;
                table[x].tide_code = code;
                table[x].tide_fg = c.fg;
                table[x].tide_bg = c.bg;
            }
        }
        for (uint8_t idx = 0; idx < kSymbolsSize; ++idx) {
            const uint16_t code = kSymbols[idx];
            double eq1 = best_optimize(img, c, m_ref, 1, x-1, y, B2U(code));
            double eq2 = best_optimize(img, d, m_ref, 2, x, y, B2U(code));
            if (eq1 + eq2 < table[x].wide_eq) {
                table[x].wide_eq = eq1 + eq2;
                table[x].wide_code = code;
                table[x].wide1_fg = c.fg;
                table[x].wide1_bg = c.bg;
                table[x].wide2_fg = d.fg;
                table[x].wide2_bg = d.bg;
            }
        }
        table[x].tide_eq += table[x - 1].result;
        if (x >= 2) {table[x].wide_eq += table[x - 2].result;}
        if (table[x].tide_eq > table[x].wide_eq) {
            table[x].result = table[x].wide_eq;
            table[x].wide = true;
        } else {
            table[x].result = table[x].tide_eq;
            table[x].wide = false;
        }
        ++m_progress;
    }
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
    pool_release_cv.notify_one();
}

void Screen::sample() {
    typedef Char::FGColor FGColor;
    typedef Char::BGColor BGColor;
    FGColor fg = FGColor::DarkBlack;
    BGColor bg = BGColor::Black;
    for (size_t y = 0; y < m_screen.size(); ++y) {
        auto & row = m_screen[y];
        for (size_t x = 0; x < row.size(); ++x) {
            auto & c = row[x];
            //c.code = (x % 2 == 0 ? 0xA2 : 0x65);
            c.code = '%';
            c.fg = fg;
            c.bg = bg;
            fg = static_cast<FGColor>(static_cast<unsigned int>(fg) + 1);
            if (fg > FGColor::LightWhite) {
                fg = FGColor::DarkBlack;
                bg = static_cast<BGColor>(static_cast<unsigned int>(bg) + 1);
                if (bg > BGColor::White) {
                    bg = BGColor::Black;
                }
            }
        }
    }
}

void Screen::best() {
    m_progress.store(0, std::memory_order_release);
    std::thread threads[23];
    for (size_t y = 0; y < 23; ++y) {
        threads[y] = std::thread(std::bind(&Screen::job, this, y));
    }
    std::cout << "thread:" << std::thread::hardware_concurrency() << std::endl;
    for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
        pool_acquire_cv.notify_one();
    }
    std::thread display([this]() {
        const uint_fast16_t max = 80 * 23;
        while (true) {
            const unsigned int progress = m_progress.load(std::memory_order_acquire);
            if (progress == max) {
                break;
            }
            const unsigned int width = term_width();
            const unsigned int len = (width > 6 ? width - 6 : 0) * progress / max;
            std::cout << '[';
            for (unsigned int i = 0; i < len; ++i) {
                std::cout << '=';
            }
            std::cout.put('>');
            for (unsigned int i = 7; i < width - len; ++i) {
                std::cout << '-';
            }
            std::cout << "]" << std::setw(3) << (100 * progress / max) << "%\n\033[1A";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        const unsigned int width = term_width();
        for (unsigned int i = 2; i < width; ++i) {
            std::cout << '=';
        }
        std::cout << "]\n";
    });
    for (size_t y = 0; y < 23; ++y) {
        std::unique_lock<std::mutex> lock(pool_release_m);
        pool_release_cv.wait(lock);
        pool_acquire_cv.notify_one();
    }
    for (size_t y = 0; y < 23; ++y) {
        threads[y].join();
    }
    display.join();
}
