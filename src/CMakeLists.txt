include_directories(${FREETYPE_INCLUDE_DIRS})
add_executable(img2bbs
    img2bbs.cpp

    error.cpp
    library.cpp
    face.cpp
    screen.cpp
)
target_compile_features(img2bbs PRIVATE
    cxx_auto_type
    cxx_constexpr
    cxx_decltype
    cxx_defaulted_functions
    cxx_defaulted_move_initializers
    cxx_deleted_functions
    cxx_explicit_conversions
    cxx_func_identifier
    cxx_lambdas
    cxx_noexcept
    cxx_nullptr
    cxx_range_for
    cxx_rvalue_references
    cxx_variadic_macros
    cxx_variadic_templates
    cxx_template_template_parameters
)
target_link_libraries(img2bbs ${OpenCV_LIBS} ${FREETYPE_LIBRARIES})
