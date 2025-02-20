set(CMAKE_LEGACY_CYGWIN_WIN32 0)
declare_cmake_min_version()

project(MorphAllLanguages)

# Convert paths to absolute paths
if(DEFINED DICTS_FOLDER)
    get_filename_component(output_morph_folder "${DICTS_FOLDER}/Morph" ABSOLUTE)
else()
    get_filename_component(output_morph_folder "${PROJECT_SOURCE_DIR}/../../Dicts/Morph" ABSOLUTE)
endif()

message("dicts_folder=${output_morph_folder}")

# Create base output directory with proper permissions
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${output_morph_folder}")

macro(Runmorph_gen langua)
    get_filename_component(input_folder "${PROJECT_SOURCE_DIR}/${langua}" ABSOLUTE)
    get_filename_component(output_folder "${output_morph_folder}/${langua}" ABSOLUTE)
    get_filename_component(morph_gen_exe "${CMAKE_BINARY_DIR}/bin/morph_gen.exe" ABSOLUTE)
    
    # Create language-specific output directory with proper permissions
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${output_folder}")

    add_custom_target(
        "morph_gen_on_${langua}"
        SOURCES 
            "${output_folder}/morph.bases"
            "${output_folder}/morph.annot"
            "${output_folder}/morph.forms_autom"
            "${output_folder}/gramtab.json"
    )

    # Copy gramtab.json with proper dependencies
    add_custom_command(
        OUTPUT "${output_folder}/gramtab.json"
        DEPENDS "${input_folder}/gramtab.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_folder}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "${input_folder}/gramtab.json"
            "${output_folder}/gramtab.json"
        COMMENT "Copying gramtab.json for ${langua}"
        VERBATIM
    )

    # Generate morphology files
    add_custom_command(
        OUTPUT 
            "${output_folder}/morph.bases"
            "${output_folder}/morph.annot"
            "${output_folder}/morph.forms_autom"
        DEPENDS 
            morph_gen 
            "${input_folder}/morphs.json"
            "${input_folder}/project.mwz"
            "${input_folder}/gramtab.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_folder}"
        COMMAND ${CMAKE_COMMAND} -E env 
            "PATH=${CMAKE_BINARY_DIR}/bin/Debug;$ENV{PATH}"
            "${morph_gen_exe}"
            --input "${input_folder}/project.mwz"
            --output-folder "${output_folder}"
            --postfix-len 5
            --min-freq 3
        COMMENT "Convert Morphology ${langua}"
        VERBATIM
    )
endmacro()

macro(StatDatBinRun langua Topic)
    get_filename_component(src "${PROJECT_SOURCE_DIR}/${langua}/StatData.txt" ABSOLUTE)
    get_filename_component(morph_folder "${output_morph_folder}/${langua}" ABSOLUTE)
    get_filename_component(trg "${morph_folder}/${Topic}homoweight.bin" ABSOLUTE)

    add_custom_command(
        OUTPUT "${trg}"
        DEPENDS 
            StatDatBin 
            "${src}" 
            "${morph_folder}/morph.bases"
            "${morph_folder}/gramtab.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${morph_folder}"
        COMMAND StatDatBin 
            --morph-folder "${morph_folder}"
            --input "${src}"
            --language "${langua}"
            --output "${trg}"
        COMMENT "StatDatBin"
        VERBATIM
    )
endmacro()

macro(word_freq_binRun langua Topic)
    get_filename_component(src "${PROJECT_SOURCE_DIR}/${langua}/WordData.txt" ABSOLUTE)
    get_filename_component(morph_folder "${output_morph_folder}/${langua}" ABSOLUTE)
    get_filename_component(trg "${morph_folder}/${Topic}wordweight.bin" ABSOLUTE)

    add_custom_command(
        OUTPUT "${trg}"
        DEPENDS 
            word_freq_bin 
            "${src}"
            "${morph_folder}/morph.bases"
            "${morph_folder}/gramtab.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${morph_folder}"
        COMMAND word_freq_bin 
            --morph-folder "${morph_folder}"
            --input "${src}"
            --language "${langua}"
            --output "${trg}"
        COMMENT "word_freq_bin"
        VERBATIM
    )
endmacro()

macro(CreateAllMorphBinFiles langua)
    Runmorph_gen(${langua})
    
    foreach(Topic l c f)
        StatDatBinRun(${langua} ${Topic})
        word_freq_binRun(${langua} ${Topic})
    endforeach()

    get_filename_component(folder "${output_morph_folder}/${langua}" ABSOLUTE)
    add_custom_target(
        "${langua}_Morph"
        SOURCES 
            "${folder}/morph.bases"
            "${folder}/chomoweight.bin"
            "${folder}/lhomoweight.bin"
            "${folder}/fhomoweight.bin"
            "${folder}/cwordweight.bin"
            "${folder}/lwordweight.bin"
            "${folder}/fwordweight.bin"
        WORKING_DIRECTORY "${folder}"
    )
endmacro()

CreateAllMorphBinFiles(Russian)
CreateAllMorphBinFiles(German)
CreateAllMorphBinFiles(English)

add_custom_target(
    MorphDicts
    DEPENDS Russian_Morph German_Morph English_Morph
)

