# Copyright 2022 Touca, Inc. Subject to Apache-2.0 License.

include_guard()

include(FetchContent)

function(touca_find_catch2)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
        GIT_TAG         v2.13.7
    )

    set(CATCH_INSTALL_DOCS OFF)

    FetchContent_MakeAvailable(catch2)
endfunction()

function(touca_find_cxxopts)
    FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
        GIT_TAG         v2.2.1
    )

    set(CXXOPTS_ENABLE_INSTALL ON)
    set(CXXOPTS_BUILD_EXAMPLES OFF)
    set(CXXOPTS_BUILD_TESTS OFF)

    FetchContent_MakeAvailable(cxxopts)
    add_library(cxxopts::cxxopts ALIAS cxxopts)
endfunction()

function(touca_find_fmt)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
        GIT_TAG         8.0.1
    )

    set(FMT_INSTALL ON)
    set(FMT_DOC OFF)
    set(FMT_TEST OFF)

    FetchContent_MakeAvailable(fmt)
endfunction()

function(touca_find_ghcfilesystem)
    FetchContent_Declare(
        ghcFilesystem
        GIT_REPOSITORY  https://github.com/gulrak/filesystem.git
        GIT_TAG         v1.5.10
    )

    set(GHC_FILESYSTEM_WITH_INSTALL ON)
    set(GHC_FILESYSTEM_BUILD_TESTING OFF)
    set(GHC_FILESYSTEM_BUILD_EXAMPLES OFF)

    FetchContent_MakeAvailable(ghcFilesystem)
    add_library(ghcFilesystem::ghcFilesystem ALIAS ghc_filesystem)
endfunction()

function(touca_find_flatbuffers)
    FetchContent_Declare(
        flatbuffers
        GIT_REPOSITORY  https://github.com/google/flatbuffers.git
        GIT_TAG         v2.0.0
    )

    set(FLATBUFFERS_BUILD_INSTALL ON)
    set(FLATBUFFERS_BUILD_TESTS OFF)

    FetchContent_MakeAvailable(flatbuffers)
    add_library(flatbuffers::flatbuffers ALIAS flatbuffers)
endfunction()

function(touca_find_httplib)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY  https://github.com/yhirose/cpp-httplib.git
        GIT_TAG         v0.9.5
    )

    FetchContent_MakeAvailable(httplib)
endfunction()

function(touca_find_rapidjson)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY  https://github.com/Tencent/rapidjson.git
        GIT_TAG         13dfc96c9c2b104be7b0b09a9f6e06871ed3e81d
    )

    set(RAPIDJSON_BUILD_DOC OFF)
    set(RAPIDJSON_BUILD_EXAMPLES OFF)
    set(RAPIDJSON_BUILD_TEST OFF)

    FetchContent_MakeAvailable(rapidjson)

    # TODO remove this after https://github.com/Tencent/rapidjson/pull/1901/files
    #      or similar PR is merged and rapidjson library target is available.
    #add_library(RapidJSON INTERFACE)
    #add_library(RapidJSON::RapidJSON ALIAS RapidJSON)
    #target_include_directories(RapidJSON INTERFACE ${rapidjson_SOURCE_DIR}/include)
endfunction()

function(touca_find_mpark_variant)
    FetchContent_Declare(
        mpark_variant
        GIT_REPOSITORY  https://github.com/mpark/variant.git
        GIT_TAG         v1.4.0
    )

    FetchContent_MakeAvailable(mpark_variant)
    add_library(mpark_variant::mpark_variant ALIAS mpark_variant)
endfunction()

function(touca_find_package)
    set(target_name ${ARGV0}::${ARGV0})
    set(find_module_name ${ARGV0})
    string(TOLOWER ${ARGV0} ots_name)
    if (1 LESS ${ARGC})
        set(find_module_name ${ARGV1})
    endif()

    if (TARGET ${target_name})
        return()
    endif()
    find_package(${find_module_name} QUIET)
    if (${find_module_name}_FOUND AND TARGET ${target_name})
        message(DEBUG "Touca: found package: ${ots_name}")
        return()
    endif()
    message(STATUS "Touca: fetching thirdparty dependency: ${ots_name}")
    cmake_language(CALL touca_find_${ots_name})
endfunction()
