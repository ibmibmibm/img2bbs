#pragma once

#include <string>
#include <memory>
#include <opencv2/imgproc.hpp>
#include "screen.hpp"
#include "error.hpp"

namespace FT {

class Face: public std::shared_ptr<Face> {
public:
    Face(FT_Face face) noexcept(false);
    ~Face() noexcept {FT_Done_Face(m_face);}
    void putChar(cv::Mat & img, uint32_t unicode);
private:
    FT_Face m_face;
};

} // namespace FT
