#include "error.hpp"

namespace FT {

const char * Error::what() const noexcept {
    switch (m_error) {

#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s) \
        case (e): return (s);
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST
#include FT_ERRORS_H

        default: return "unknown";
    }
}

} // namespace FT
