declare_cmake_min_version()

project(StatDatBin)

add_executable(StatDatBin homon_freq_bin.cpp)

# Set as console application
set_target_properties(StatDatBin PROPERTIES
    WIN32_EXECUTABLE FALSE
    LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
)

target_link_libraries(StatDatBin
    morph_wizard_lib
    lemmatizer_base_lib
    AgramtabLib
    morphology_common
    iconv_subset
)
