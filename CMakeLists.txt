cmake_minimum_required (VERSION 3.12)
project (slick-edit)

option (BuildTs "compile the tree-sitter library" OFF)
option (BuildcJSON "compile the json library" ON)

set (DependenciesDir "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	set (CMAKE_CXX_STANDARD_LIBRARIES "kernel32.lib user32.lib gdi32.lib")
	set (CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /Zi /W3 /Zc:preprocessor")
	set (CMAKE_CXX_FLAGS_DEBUG   "/MDd /Ob0 /Od /DDEBUG /EHsc /RTC1")
	set (CMAKE_CXX_FLAGS_RELEASE "/MD  /Ob2 /O2 /DRELEASE")

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
if (BuildTs)
	add_library(tree-sitter OBJECT "${DependenciesDir}/tree-sitter/src/lib.c")
	target_compile_options(tree-sitter PRIVATE "/W0") # disable warnings
	target_include_directories (tree-sitter PRIVATE "${DependenciesDir}/tree-sitter/include")
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
if (BuildcJSON)
	add_library(cJSON STATIC)
	target_include_directories (cJSON PRIVATE "${DependenciesDir}/cJSON")
    target_compile_options(cJSON PRIVATE "/GS" "/Za" "/sdl" "/W4" "/wd4001" "/D_CRT_SECURE_NO_WARNINGS" "/TC")
	target_sources (cJSON PRIVATE
		"${DependenciesDir}/cJSON/cJSON.c"
		"${DependenciesDir}/cJSON/cJSON_Utils.c")
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_executable(slick-edit)

set_target_properties(slick-edit PROPERTIES CXX_STANDARD 23)
# target_compile_options(slick-edit PRIVATE "/Zc:__cplusplus")

target_include_directories (slick-edit PRIVATE "source" ${DependenciesDir} )

file (GLOB_RECURSE Sources "source/*")
target_sources (slick-edit PRIVATE ${Sources})

install (TARGETS slick-edit RUNTIME DESTINATION .)
install (FILES "$<TARGET_PDB_FILE:slick-edit>" DESTINATION .)

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#install (DIRECTORY "languages" DESTINATION .)

# cmake_policy(SET CMP0087 NEW)
# install (CODE
# 	"message(STATUS \"executing 'download-clangd.ps1'\")
# 	execute_process (
# 		COMMAND powershell -NoProfile -NoLogo -File download-clangd.ps1
# 		OUTPUT_VARIABLE Output
# 		COMMAND_ERROR_IS_FATAL ANY
# 		WORKING_DIRECTORY $<INSTALL_PREFIX>/languages/cpp)
# 	message(\${Output})")
