declare_cmake_min_version()

project(word_freq_bin)

add_executable(word_freq_bin word_freq_bin.cpp)

# Set as console application
set_target_properties(word_freq_bin PROPERTIES
    WIN32_EXECUTABLE FALSE
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

target_link_libraries(word_freq_bin
    morph_wizard_lib
    lemmatizer_base_lib
    AgramtabLib
    morphology_common
    iconv_subset
)
