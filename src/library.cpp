#include "face.hpp"
#include "library.hpp"

namespace FT {

std::shared_ptr<Face> Library::new_face(const char * filepathname, FT_Long face_index) noexcept(false) {
    FT_Face face;
    auto error = FT_New_Face(m_lib, filepathname, face_index, &face);
    if (error) {
        throw Error(error);
    }
    return std::make_shared<Face>(face);
}

} // namespace FT
