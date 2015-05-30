#include <iostream>
#include "face.hpp"
#include FT_GLYPH_H

namespace FT {

void Face::putText(cv::Mat & img, cv::Point pos, const std::string & text) {
    FT_Vector pen = {pos.x * 64, pos.y * 64};
    for (const auto & c: text) {
        FT_Vector small = {pen.x % 64, pen.y % 64};
        FT_Set_Transform(m_face, nullptr, &small);
        auto glyph_index = FT_Get_Char_Index(m_face, c);
        auto error = FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
        if (error) {throw Error(error);}
        error = FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);
        if (error) {throw Error(error);}
        const auto & glyph = *m_face->glyph;
        const auto & bitmap = glyph.bitmap;
        cv::Mat glyph_img(bitmap.rows, bitmap.width, CV_8UC1, bitmap.buffer, bitmap.pitch);
        cvtColor(glyph_img, glyph_img, CV_GRAY2RGB);
        glyph_img.convertTo(glyph_img, img.type());

        cv::Mat color = cv::Mat::zeros(bitmap.rows, bitmap.width, img.type());
        color = cv::Scalar(255, 255, 255);

        cv::Rect roi(cv::Size(pen.x / 64 + glyph.bitmap_left, pen.y / 64 - glyph.bitmap_top), glyph_img.size());

        img(roi) = color.mul(glyph_img, 1.0 / 255.0) + img(roi).mul(cv::Scalar(255, 255, 255) - glyph_img, 1.0 / 255.0);

        pen.x += glyph.advance.x;
        pen.y += glyph.advance.y;
    }
}

void Face::putChar(cv::Mat & img, const cv::Rect & pos, const cv::Point & offset,
    const cv::Scalar & fgcolor, const cv::Scalar & bgcolor, uint32_t unicode) {

    auto glyph_index = FT_Get_Char_Index(m_face, unicode);
    auto error = FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {throw Error(error);}

    FT_Glyph glyph;
    error = FT_Get_Glyph(m_face->glyph, &glyph);
    if (error) {throw Error(error);}

    FT_BBox box;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_UNSCALED, &box);
    FT_Done_Glyph(glyph);

    FT_Pos x = (pos.x + offset.x + pos.width / 2) * 64 - (box.xMax - box.xMin) / 2 - box.xMin;
    FT_Pos y = (pos.y + offset.y + pos.height * 5 / 6) * 64;
    FT_Vector small = {x % 64, y % 64};
    FT_Set_Transform(m_face, nullptr, &small);

    // render
    error = FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {throw Error(error);}
    const auto & bitmap = m_face->glyph->bitmap;
    cv::Mat glyph_img(bitmap.rows, bitmap.width, CV_8UC1, bitmap.buffer, bitmap.pitch);
    cv::Rect roi(cv::Size(x / 64 + m_face->glyph->bitmap_left, y / 64 - m_face->glyph->bitmap_top), glyph_img.size());
    cv::Rect shrink1 = roi & pos;
    cv::Rect shrink2(shrink1.tl() - roi.tl(), cv::Point2i(glyph_img.size()) + shrink1.br() - roi.br());

    // crop
    if (shrink2.area() > 0) {
        cvtColor(glyph_img(shrink2), glyph_img, CV_GRAY2RGB);
        glyph_img.convertTo(glyph_img, img.type());
    }

    cv::Mat bg = cv::Mat::zeros(pos.size(), img.type());
    bg = bgcolor;
    bg.copyTo(img(pos));

    if (shrink1.area() > 0) {
        cv::Mat fg = cv::Mat::zeros(glyph_img.rows, glyph_img.cols, img.type());
        fg = fgcolor;
        img(shrink1) = fg.mul(glyph_img, 1.0 / 255.0) + img(shrink1).mul(cv::Scalar(255, 255, 255) - glyph_img, 1.0 / 255.0);
    }
}

void Face::putChar(cv::Mat & img, uint32_t unicode) {
    auto glyph_index = FT_Get_Char_Index(m_face, unicode);
    auto error = FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {throw Error(error);}

    FT_Glyph glyph;
    error = FT_Get_Glyph(m_face->glyph, &glyph);
    if (error) {throw Error(error);}

    FT_BBox box;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_UNSCALED, &box);
    FT_Done_Glyph(glyph);

    FT_Pos x = (img.cols / 2) * 64 - (box.xMax - box.xMin) / 2 - box.xMin;
    FT_Pos y = (img.rows * 5 / 6) * 64;
    FT_Vector small = {x % 64, y % 64};
    FT_Set_Transform(m_face, nullptr, &small);

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
