cmake_minimum_required(VERSION 3.15)

project(RawFrame
    VERSION 0.1.0
    DESCRIPTION "Lightweight modular image processing library in pure C"
    LANGUAGES C
)

enable_testing()

# Use C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Option to enable SDL display backend
#option(RAWFRAME_ENABLE_SDL "Enable SDL2 display backend" ON)

# Include public headers
include_directories(include)

# ===============================
# Core Library Sources
# ===============================

set(RAWFRAME_CORE
    src/core/img.c
    src/core/img_format.c
    src/core/img_convert.c
    src/core/img_pipeline.c
    src/state/isp_pipeline.c
)

set(RAWFRAME_IO
    src/io/bmp_reader.c
    src/io/bmp_writer.c
    src/io/ppm_reader.c
    src/io/ppm_writer.c
)

set(RAWFRAME_PROCESSING
    src/processing/img_copy.c
    src/processing/img_grayscale.c
    src/processing/img_invert.c
    src/processing/img_blur.c
)

set(RAWFRAME_DISPLAY)

# ===============================
# SDL2 (Optional)
# ===============================

#if(RAWFRAME_ENABLE_SDL)
#    find_package(SDL2 REQUIRED)
#    list(APPEND RAWFRAME_DISPLAY
#        src/display/img_sdl_display.c
#    )
#endif()

# ===============================
# Create Library
# ===============================

add_library(rawframe STATIC
    ${RAWFRAME_CORE}
    ${RAWFRAME_IO}
    ${RAWFRAME_PROCESSING}
    ${RAWFRAME_DISPLAY}
)
add_library(unity STATIC third_party/Unity/src/unity.c)
target_include_directories(unity PUBLIC third_party/Unity/src)
target_include_directories(rawframe PUBLIC include)

if(RAWFRAME_ENABLE_SDL)
    target_link_libraries(rawframe PUBLIC SDL2::SDL2)
endif()

# ===============================
# Examples
# ===============================

#add_executable(example_load_display examples/example_load_display.c)
#target_link_libraries(example_load_display rawframe)

add_executable(example_copy examples/example_copy.c)
target_link_libraries(example_copy rawframe)
add_executable(example_convert examples/example_convert.c)
target_link_libraries(example_convert rawframe)
add_executable(example_ppm examples/example_ppm.c)
target_link_libraries(example_ppm rawframe)
add_executable(example_blur examples/example_blur.c)
target_link_libraries(example_blur rawframe)
#add_executable(example_processing examples/example_processing.c)
#target_link_libraries(example_processing rawframe)

add_executable(test_img_convert tests/test_img_convert.c)
target_link_libraries(test_img_convert rawframe unity)
add_test(NAME test_img_convert COMMAND test_img_convert)

add_executable(test_io tests/test_io.c)
target_link_libraries(test_io rawframe unity)
add_test(NAME test_io COMMAND test_io)

add_executable(test_nv12 tests/test_nv12.c)
target_link_libraries(test_nv12 rawframe unity)
add_test(NAME test_nv12 COMMAND test_nv12)

add_executable(test_blur tests/test_blur.c)
target_link_libraries(test_blur rawframe unity)
add_test(NAME test_blur COMMAND test_blur)

add_executable(test_isp tests/test_isp.c)
target_link_libraries(test_isp rawframe unity)
add_test(NAME test_isp COMMAND test_isp)