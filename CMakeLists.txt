cmake_minimum_required(VERSION 3.25)

project(TinyPoly
    VERSION 0.5.4
    LANGUAGES C
)

# C23
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(TinyPoly ${SOURCES})

# Windows: exe 파일 아이콘 (assets/icon.ico는 tools/make_icon.py로 만듦)
if(WIN32)
    enable_language(RC)
    target_sources(TinyPoly PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets/tinypoly.rc")
endif()

target_include_directories(TinyPoly PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# raylib: 설치된 패키지가 있으면 사용하고, 없으면 소스를 받아서 빌드
find_package(raylib 5.5 CONFIG QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

target_compile_definitions(TinyPoly PRIVATE TINYPOLY_VERSION="${PROJECT_VERSION}")

target_link_libraries(TinyPoly PRIVATE raylib)
if(NOT MSVC)
    target_link_libraries(TinyPoly PRIVATE m)
endif()

if(MSVC)
    target_compile_options(TinyPoly PRIVATE /W4)
else()
    target_compile_options(TinyPoly PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(TinyPoly PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Windows: raylib을 DLL로 쓰는 경우 exe 옆으로 복사
if(WIN32)
    add_custom_command(TARGET TinyPoly POST_BUILD
        COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:TinyPoly>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:TinyPoly>;$<TARGET_FILE_DIR:TinyPoly>>"
        COMMAND_EXPAND_LISTS)
endif()

add_custom_target(run
    COMMAND $<TARGET_FILE:TinyPoly>
    DEPENDS TinyPoly
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)
