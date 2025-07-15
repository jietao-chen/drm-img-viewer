cmake_minimum_required(VERSION 3.10)
project(drm-test C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(drm-test drm_simple_draw.c load_png.c)

target_include_directories(drm-test PRIVATE /usr/include/libdrm)

# 链接 libdrm
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED libdrm)
target_link_libraries(drm-test PRIVATE ${LIBDRM_LIBRARIES})
target_include_directories(drm-test PRIVATE ${LIBDRM_INCLUDE_DIRS})

# 链接 libpng
find_package(PNG REQUIRED)
pkg_check_modules(LIBPNG REQUIRED libpng)
target_link_libraries(drm-test PRIVATE ${LIBPNG_LIBRARIES})
target_include_directories(drm-test PRIVATE ${LIBPNG_INCLUDE_DIRS})

add_custom_command(TARGET drm-test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/res/test.png
        $<TARGET_FILE_DIR:drm-test>
)
