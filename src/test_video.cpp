#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/videoio_c.h>

namespace {

std::string make_name(const char *prefix, size_t idx) {
    std::ostringstream os;
    os << prefix << "/f_" << std::setw(4) << std::setfill('0') << idx << ".png";
    return os.str();
}

}

int main(int argc, char *argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    if (argc != 4) {
        std::cout << "usage: " << argv[0] << " VIDEO fps DIR\n";
        return -1;
    }
    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        std::cout << "open failed.\n";
        return -1;
    }
    const double new_fps = std::strtod(argv[2], nullptr);
    const double fps = cap.get(cv::CAP_PROP_FPS);
    std::cout << "fps:" << fps << std::endl;
    size_t f_idx = 0;
    double f_time = 0;
    size_t f_next_idx = 0;
    double f_next_time = 0;
    cv::Mat img;
    while (true) {
        while (f_time <= f_next_time) {
            if (cap >> img, img.empty()) {
                break;
            }
            ++f_idx;
            f_time = f_idx / fps;
        }
        if (img.empty()) {
            break;
        }
        double msec = cap.get(cv::CAP_PROP_POS_MSEC);
        std::cout << "msec:" << msec << std::endl;
        const std::string filename = make_name(argv[3], f_next_idx);
        std::cout << "idx:" << f_idx-1 << " " << filename << std::endl;
        //imwrite(filename, img);
        ++f_next_idx;
        f_next_time = f_next_idx / new_fps;
    }
    return 0;
}
