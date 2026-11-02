# ==============================================================================
# Sanitizers.cmake: Configures AddressSanitizer (ASan) & UBSan
# ==============================================================================

function(configure_target_sanitizers TARGET_NAME)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(STATUS "Configuring ASan and UBSan for target: ${TARGET_NAME}")
            target_compile_options(${TARGET_NAME} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
                -fno-optimize-sibling-calls
                -g
            )
            target_link_options(${TARGET_NAME} PRIVATE
                -fsanitize=address,undefined
            )
        else()
            message(WARNING "Sanitizers requested but compiler ${CMAKE_CXX_COMPILER_ID} is not Clang or GCC.")
        endif()
    endif()
endfunction()
