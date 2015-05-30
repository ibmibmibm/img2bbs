#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include <exception>

namespace FT {

class Error: public std::exception {
public:
    Error(FT_Error error) noexcept: m_error(error) {}
    virtual const char * what() const noexcept;
private:
    FT_Error m_error;
};

} // namespace FT
