cmake_minimum_required(VERSION 3.0)

project(morph_gen)

add_executable(morph_gen morph_gen.cpp)

# Set as console application
set_target_properties(morph_gen PROPERTIES
    WIN32_EXECUTABLE FALSE
    LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
)

target_link_libraries(morph_gen
    morph_wizard_lib
    lemmatizer_base_lib
    AgramtabLib
    morphology_common
    iconv_subset
)

add_subdirectory(tests)


