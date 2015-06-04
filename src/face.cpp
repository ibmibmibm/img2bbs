#include <iostream>
#include "face.hpp"
#include FT_GLYPH_H

namespace FT {

Face::Face(FT_Face face) noexcept(false): m_face(face) {
    auto error = FT_Set_Char_Size(m_face, 0, (kCharH - 7) * 64, 96, 96);
    if (error) {throw Error(error);}
}

void Face::putChar(cv::Mat & img, uint32_t unicode) {
    auto glyph_index = FT_Get_Char_Index(m_face, unicode);
    auto error = FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO);
    if (error) {throw Error(error);}

    FT_Glyph glyph;
    error = FT_Get_Glyph(m_face->glyph, &glyph);
    if (error) {throw Error(error);}

    FT_BBox box;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_UNSCALED, &box);
    FT_Done_Glyph(glyph);

    FT_Pos x = 0;
    FT_Pos y = (img.rows * 14 / 16) * 64;
//    FT_Vector small = {x % 64, y % 64};
//    FT_Set_Transform(m_face, nullptr, &small);

    // render
    error = FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {throw Error(error);}
    const auto & bitmap = m_face->glyph->bitmap;

    cv::Mat glyph_img(bitmap.rows, bitmap.width, CV_8UC1, bitmap.buffer, bitmap.pitch);

    cv::Rect roi(cv::Point(x / 64 + m_face->glyph->bitmap_left, y / 64 - m_face->glyph->bitmap_top), glyph_img.size());
    cv::Rect shrink1 = roi & cv::Rect(0, 0, img.cols, img.rows);
    cv::Rect shrink2(shrink1.tl() - roi.tl(), cv::Point(glyph_img.size()) + shrink1.br() - roi.br());

    // crop
    if (shrink2.area() == 0) {
        return;
    }
    cvtColor(glyph_img(shrink2), glyph_img, CV_GRAY2RGB);
    glyph_img.convertTo(img(shrink1), img.type());
}

} // namespace FT
