function(compile_shaders TARGET SHADER_DIR OUTPUT_DIR)
    file(GLOB_RECURSE SHADER_FILES
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
        "${SHADER_DIR}/*.comp"
        "${SHADER_DIR}/*.rgen"
        "${SHADER_DIR}/*.rmiss"
        "${SHADER_DIR}/*.rchit"
    )

    # Find glslc - search Vulkan SDK, PATH, and common install locations
    find_program(GLSLC glslc
        HINTS
            $ENV{VULKAN_SDK}/Bin
            $ENV{VULKAN_SDK}/bin
            "C:/VulkanSDK/1.4.335.0/Bin"
        PATHS
            "C:/VulkanSDK"
        PATH_SUFFIXES
            Bin bin
    )
    if(NOT GLSLC)
        # Try to find any glslc in standard VulkanSDK install locations
        file(GLOB VULKAN_SDK_DIRS "C:/VulkanSDK/*/Bin/glslc.exe")
        if(VULKAN_SDK_DIRS)
            list(GET VULKAN_SDK_DIRS 0 GLSLC)
        endif()
    endif()

    if(NOT GLSLC)
        message(FATAL_ERROR
            "glslc not found. Install the Vulkan SDK:\n"
            "  Windows: winget install KhronosGroup.VulkanSDK\n"
            "  Or run: scripts/setup.bat\n"
            "Then re-run cmake."
        )
    endif()

    message(STATUS "Using shader compiler: ${GLSLC}")

    set(SPV_FILES "")
    foreach(SHADER ${SHADER_FILES})
        file(RELATIVE_PATH REL_PATH ${SHADER_DIR} ${SHADER})
        set(SPV_FILE "${OUTPUT_DIR}/${REL_PATH}.spv")
        get_filename_component(SPV_DIR ${SPV_FILE} DIRECTORY)

        add_custom_command(
            OUTPUT ${SPV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_DIR}
            COMMAND ${GLSLC}
                -I ${SHADER_DIR}/common
                --target-env=vulkan1.3
                -O
                ${SHADER} -o ${SPV_FILE}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader: ${REL_PATH}"
        )
        list(APPEND SPV_FILES ${SPV_FILE})
    endforeach()

    add_custom_target(${TARGET}_shaders DEPENDS ${SPV_FILES})
    add_dependencies(${TARGET} ${TARGET}_shaders)
endfunction()
