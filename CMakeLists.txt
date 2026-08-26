cmake_minimum_required (VERSION 3.12)
project (slick-edit)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set (CMAKE_CXX_STANDARD_LIBRARIES "kernel32.lib user32.lib gdi32.lib")
set (CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /Zi /W3 /Zc:preprocessor")
set (CMAKE_CXX_FLAGS_DEBUG   "/MDd /Ob0 /Od /DDEBUG /EHsc /RTC1")
set (CMAKE_CXX_FLAGS_RELEASE "/MD  /Ob2 /O2 /DRELEASE")

add_executable (slick-edit)

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# dependencies

set (BUILD_SHARED_LIBS OFF)

#
# toml++
#
target_include_directories (slick-edit PRIVATE "deps/tomlplusplus/include")

#
# pearl regex lib
#
set (PCRE2_BUILD_PCRE2_8 ON)
set (BUILD_STATIC_LIBS ON)
set (PCRE2_NEWLINE ANYCRLF)
set (PCRE2_SUPPORT_JIT ON CACHE BOOL "Enable JIT support" FORCE)
set (PCRE2_NEVER_BACKSLASH_C OFF)
set (PCRE2_BUILD_PCRE2GREP OFF)
set (PCRE2_BUILD_TESTS OFF)

add_subdirectory ("deps/pcre2")
target_link_libraries (slick-edit pcre2-8-static)
target_include_directories (slick-edit PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/deps/pcre2/interface")

#
# tree-sitter
#
add_subdirectory ("deps/tree-sitter")
target_link_libraries (slick-edit tree-sitter)
target_include_directories (slick-edit PRIVATE "deps/tree-sitter/lib/include")

#
# cJSON
#
set (CJSON_BUILD_SHARED_LIBS OFF)

add_subdirectory ("deps/cJSON")
target_link_libraries(slick-edit cjson)
target_include_directories (slick-edit PRIVATE "deps/cJSON")

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# main executable

set(WINDOWS_DEPENDENCIES d2d1.lib dwrite.lib dwmapi.lib shell32.lib)
if (CMAKE_BUILD_TYPE MATCHES "[Dd]ebug")
	list(APPEND WINDOWS_DEPENDENCIES dbghelp.lib)
endif()

target_link_libraries(slick-edit ${WINDOWS_DEPENDENCIES})
set_target_properties(slick-edit PROPERTIES CXX_STANDARD 20)

target_include_directories (slick-edit PRIVATE "src")

file (GLOB_RECURSE SOURCES "src/*")
target_sources (slick-edit PRIVATE ${SOURCES})

install (TARGETS slick-edit RUNTIME DESTINATION .)
install (FILES "$<TARGET_PDB_FILE:slick-edit>" DESTINATION .)

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# test executable

add_executable (slick-edit-tests)

target_include_directories (slick-edit-tests PRIVATE "deps/doctest")
target_include_directories (slick-edit-tests PRIVATE "src")
target_include_directories (slick-edit-tests PRIVATE $<TARGET_PROPERTY:slick-edit,INCLUDE_DIRECTORIES>)

target_link_libraries(slick-edit-tests PRIVATE $<TARGET_PROPERTY:slick-edit,LINK_LIBRARIES>)
# set_target_properties(slick-edit-tests PROPERTIES CXX_STANDARD $<TARGET_PROPERTY:slick-edit,CXX_STANDARD>)
set_target_properties(slick-edit-tests PROPERTIES CXX_STANDARD 20)

file (GLOB_RECURSE TEST_SOURCES "src/*" "test/*")
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc")
target_sources (slick-edit-tests PRIVATE ${TEST_SOURCES})

install (TARGETS slick-edit-tests RUNTIME DESTINATION .)
install (FILES "$<TARGET_PDB_FILE:slick-edit-tests>" DESTINATION .)
