cmake_minimum_required(VERSION 3.30)
project(toiya-hyperapi LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_LIBRARIES ON)

if (MSVC)
else ()
    add_compile_options(-Wall -Wextra -Werror)
endif ()

if (WIN32)
    set(TABLEAU_HYPER_API_DOWNLOAD_URL "https://downloads.tableau.com/tssoftware//tableauhyperapi-cxx-windows-x86_64-release-main.0.0.20746.reac9bd2d.zip")
elseif (APPLE)
    if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(TABLEAU_HYPER_API_DOWNLOAD_URL "https://downloads.tableau.com/tssoftware//tableauhyperapi-cxx-macos-arm64-release-main.0.0.20746.reac9bd2d.zip")
    else ()
        set(TABLEAU_HYPER_API_DOWNLOAD_URL "https://downloads.tableau.com/tssoftware//tableauhyperapi-cxx-macos-x86_64-release-main.0.0.20746.reac9bd2d.zip")
    endif ()
else ()
    set(TABLEAU_HYPER_API_DOWNLOAD_URL "https://downloads.tableau.com/tssoftware//tableauhyperapi-cxx-linux-x86_64-release-main.0.0.20746.reac9bd2d.zip")
endif ()

include(FetchContent)
FetchContent_Declare(
    tableauhyperapi-cxx
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    URL "${TABLEAU_HYPER_API_DOWNLOAD_URL}"
)

FetchContent_MakeAvailable(tableauhyperapi-cxx)
list(APPEND CMAKE_PREFIX_PATH "${tableauhyperapi-cxx_SOURCE_DIR}/share/cmake")
find_package(tableauhyperapi-cxx CONFIG REQUIRED)

FetchContent_Declare(nanoarrow
    GIT_REPOSITORY https://github.com/apache/arrow-nanoarrow.git
    GIT_TAG apache-arrow-nanoarrow-0.6.0
)
FetchContent_MakeAvailable(nanoarrow)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if (CLANG_TIDY_EXE_FOUND)
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "-checks=-*,modernize-*")
else ()
    message("Could not find clang-tidy installation - checks disabled")
endif ()

add_subdirectory(src/toiya-hyperapi)