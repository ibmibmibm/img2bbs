#pragma once

#include <memory>
#include "error.hpp"

namespace FT {

class Face;

class Library {
public:
    Library() noexcept(false) {
        auto error = FT_Init_FreeType(&m_lib);
        if (error) {
            throw Error(error);
        }
    }
    ~Library() noexcept {FT_Done_FreeType(m_lib);}
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    std::shared_ptr<Face> new_face(const char * filepathname, FT_Long face_index) noexcept(false);
private:
    FT_Library m_lib;
};

} // namespace FT
