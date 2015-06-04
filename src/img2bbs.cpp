#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgcodecs/imgcodecs_c.h>
#include <opencv2/imgproc.hpp>
#include "library.hpp"
#include "screen.hpp"

namespace {

static thread_local std::mt19937 rd(time(NULL));
static thread_local std::uniform_int_distribution<size_t> gen_idx(0, 99);

/*
void gamma(cv::Mat &img, double g) {
    const double inv_g = 1.0 / g;
    cv::Mat lut(1, 256, CV_8UC1);
    for (size_t i = 0; i < 256; ++i) {
        lut.ptr()[i] = pow(i / 255.0, inv_g) * 255.0;
    }
    cv::LUT(img, lut, img);
}
*/

} // namespace

int main(int argc, char *argv[]) {
    if (argc != 4) return -1;

    cv::Mat src = cv::imread(argv[1]);
    src.convertTo(src, CV_32FC3);
    cv::Mat dst = cv::Mat::zeros(kScreenH * kCharH, kScreenW * kCharW, src.type());
    resize(src, src, dst.size(), 0, 0, cv::INTER_LANCZOS4);

    src.convertTo(src, CV_8UC3);
    cvtColor(src, src, cv::COLOR_BGR2HSV);
    for (int y = 0; y < src.rows; ++y)  {
        for (int x = 0; x < src.cols; ++x)  {
            uint8_t &h = src.at<uint8_t>(y, x * 3);
            h = (h + 15) / 30 * 30 % 180;
            uint8_t &s = src.at<uint8_t>(y, x * 3 + 1);
            s = (s > 0x40) ? 0xFF : 0x00;
            uint8_t &v = src.at<uint8_t>(y, x * 3 + 2);
            if (s == 0x00) {
                v = (v > 0x40) ? (v > 0xa0 ? (v > 0xe0 ? 0xff : 0xc0) : 0x80) : 0x00;
            } else {
                v = (v > 0x40) ? (v > 0xc0 ? 0xff : 0x80) : 0x00;
            }
        }
    }
    cvtColor(src, src, cv::COLOR_HSV2BGR);

    Screen::init();

    auto s = std::make_shared<Screen>(src);
    //s->sample();
    s->best();
    s->render(dst);

    std::cout << s->psnr << std::endl;
    imwrite(argv[2], dst);

    std::ofstream fs(argv[3], std::ios::binary);
    fs << s->to_string();

    return 0;
}
