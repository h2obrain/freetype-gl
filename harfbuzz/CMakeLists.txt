# Freetype GL - A C OpenGL Freetype engine
#
# Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
# file `LICENSE` for more details.

add_compile_options(-Wall -Wextra -ggdb3 -O0)

set(CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules"
    "${CMAKE_MODULE_PATH}"
)
# The additional / is important to remove the last character from the path.
# Note that it does not matter if the OS uses / or \, because we are only
# saving the path size.
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

message(STATUS "Building for ${CMAKE_SYSTEM_NAME} target system")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} compiler")

set(freetype-gl_WITH_GLEW_DEFAULT ON)

find_package(HarfBuzz REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")
include_directories(
	/usr/include/harfbuzz
    ${HARFBUZZ_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(FREETYPE_GL_HB_HDR
    freetype-gl.h
    opengl.h
    texture-atlas.h
    texture-font.h
    vec234.h
    vector.h
    vertex-attribute-gl.h
    vertex-buffer-gl.h
    freetype-gl-err.h
    freetype-gl-errdef.h
    utf8-utils.h
    distance-field.h
    edtaa3func.h
    font-manager.h
    text-buffer.h
    block-layout.h
)

set(FREETYPE_GL_HB_SRC
    texture-atlas.c
    texture-font.c
    vector.c
    vertex-attribute-gl.c
    vertex-buffer-gl.c
    freetype-gl-err.c
    utf8-utils.c
    distance-field.c
    edtaa3func.c
    font-manager.c
    text-buffer.c
    block-layout.c
)

if(freetype-gl_BUILD_SHARED)
    add_library(freetype-gl-hb SHARED
        ${FREETYPE_GL_HB_SRC}
        ${FREETYPE_GL_HB_HDR}
    )
    SET_TARGET_PROPERTIES(
        freetype-gl-hb
    PROPERTIES
        VERSION 0.3.2
        SOVERSION 0)
    target_link_libraries (freetype-gl-hb
			   ${OPENGL_LIBRARY}
			   ${FREETYPE_LIBRARIES}
			   ${MATH_LIBRARY}
			   ${GLEW_LIBRARY}
			   ${HARFBUZZ_LIBRARIES}
			   )
else()
    add_library(freetype-gl-hb STATIC
        ${FREETYPE_GL_HB_SRC}
        ${FREETYPE_GL_HB_HDR}
    )
endif()

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

if ("${LIB64}" STREQUAL "TRUE")
    set(LIBSUFFIX 64)
else()
    set(LIBSUFFIX "")
endif()

set(INSTALL_LIB_DIR     lib${LIBSUFFIX} CACHE PATH "Installation directory for libraries")
mark_as_advanced(INSTALL_LIB_DIR)

install(TARGETS freetype-gl-hb
	ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR}
	COMPONENT library)
install(FILES ${FREETYPE_GL_HB_HDR} DESTINATION include/harfbuzz
	COMPONENT headers)

set(CMAKE_BUILD_TYPE Release)

