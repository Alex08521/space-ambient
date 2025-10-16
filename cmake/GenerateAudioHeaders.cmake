# GenerateAudioHeaders.cmake
function(generate_audio_headers AUDIO_FILES)
    find_program(XXD xxd)
    if(NOT XXD)
        message(FATAL_ERROR "Error: xxd not found")
    endif()

    set(GEN_DIR "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${GEN_DIR}")

    set(HEADER_LIST "")
    set(DATA_INITER "")

    foreach(audio_file IN LISTS AUDIO_FILES)
        get_filename_component(basename "${audio_file}" NAME_WE)
        string(REGEX REPLACE "[^A-Za-z0-9]" "_" basename "${basename}")
        set(var_name "${basename}_data")
        set(out_file "${GEN_DIR}/${basename}_audio.h")

        add_custom_command(
            OUTPUT "${out_file}"
            COMMAND ${XXD} -i -n ${var_name} "${audio_file}" "${out_file}"
            DEPENDS "${audio_file}"
            COMMENT "Generating ${basename}_audio.h"
        )

        list(APPEND HEADER_LIST "#include \"${basename}_audio.h\"")
        list(APPEND DATA_INITER "tm_data={${basename}_data,${basename}_data_len,0} \n\taudio_files.push_back(tm_data) \n")
        list(APPEND HEADER_FILES "${out_file}")
    endforeach()

    add_custom_target(
        GenerateAudioHeaders ALL
        DEPENDS ${HEADER_FILES}
        COMMENT "Generating audio headers"
    )

    string(REPLACE ";" "\n" HEADER_LIST "${HEADER_LIST}")
    string(REPLACE ";" "\t" DATA_INITER "${DATA_INITER}")
    string(REPLACE " " ";" DATA_INITER "${DATA_INITER}")

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/genaudio.h.in"
        "${GEN_DIR}/genaudio.h"
        @ONLY
    )
endfunction()