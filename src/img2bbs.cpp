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

cv::Mat normalize(cv::Mat &src) {
    src.convertTo(src, CV_64FC3);
    cv::Mat dst = cv::Mat::zeros(23 * kCharH, 80 * kCharW, src.type());
    cv::Rect roi;
    if (src.cols * dst.rows > src.rows * dst.cols) {
        roi.width = dst.cols;
        roi.x = 0;
        roi.height = src.rows * dst.cols / src.cols;
        roi.y = (dst.rows - roi.height) / 2;
    } else if (src.cols * dst.rows < src.rows * dst.cols) {
        roi.height = dst.rows;
        roi.y = 0;
        roi.width = src.cols * dst.rows / src.rows;
        roi.x = (dst.cols - roi.width) / 2;
    } else {
        roi.x = 0;
        roi.y = 0;
        roi.width = dst.cols;
        roi.height = dst.rows;
    }
    resize(src, dst(roi), roi.size(), 0, 0, cv::INTER_LANCZOS4);
    return std::move(dst);
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc != 4) return -1;

    cv::Mat src = cv::imread(argv[1]);
    src = normalize(src);

    Screen::init();

    auto s = std::make_shared<Screen>(src);
    //s->sample();
    s->best();
    cv::Mat dst;
    s->render(dst);

    std::cout << *s << std::endl;
    std::cout << s->psnr << std::endl;

    imwrite(argv[2], dst);

    std::ofstream fs(argv[3], std::ios::binary);
    fs << s->to_string();
    return 0;
}
