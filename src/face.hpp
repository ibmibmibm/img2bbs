#pragma once

#include <string>
#include <memory>
#include <opencv2/imgproc.hpp>
#include "error.hpp"

namespace FT {

class Face: public std::shared_ptr<Face> {
public:
    Face(FT_Face face) noexcept(false): m_face(face) {
        auto error = FT_Set_Char_Size(m_face, 0, 20 * 64, 96, 96);
        if (error) {throw Error(error);}
    }
    ~Face() noexcept {FT_Done_Face(m_face);}
    void putText(cv::Mat & img, cv::Point org, const std::string & text);
    void putChar(cv::Mat & img, const cv::Rect & pos, const cv::Point & offset,
        const cv::Scalar & fgcolor, const cv::Scalar & bgcolor, uint32_t unicode);
    void putChar(cv::Mat & img, uint32_t unicode);
private:
    FT_Face m_face;
};

} // namespace FT
