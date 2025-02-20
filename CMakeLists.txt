macro (declare_cmake_min_version)
    cmake_minimum_required(VERSION 3.20)
endmacro(declare_cmake_min_version)

declare_cmake_min_version()

project(morphology)


set (CMAKE_CXX_STANDARD 17)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")


add_subdirectory (contrib/iconv_subset)

if(WIN32)
    # Try to find ZLIB using ZLIB_ROOT first
    if(NOT DEFINED ZLIB_ROOT)
        set(ZLIB_ROOT "${CMAKE_SOURCE_DIR}/external/zlib/install")
    endif()
    
    set(ZLIB_USE_STATIC_LIBS ON)
    find_package(ZLIB REQUIRED)
    
    if(NOT ZLIB_FOUND)
        message(FATAL_ERROR "ZLIB not found. Please run install_zlib.bat first")
    endif()
endif()

add_subdirectory (common)
add_subdirectory (agramtab)
add_subdirectory (morph_wizard)
add_subdirectory (lemmatizer_base_lib)
add_subdirectory (morph_gen)
add_subdirectory (homon_freq_bin)
add_subdirectory (word_freq_bin)
add_subdirectory (data)
add_subdirectory (test_lem)
