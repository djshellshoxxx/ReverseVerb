cmake_minimum_required(VERSION 3.22)

get_filename_component(REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${REPO_DIR}/.github/workflows/ci.yml" CI_TEXT)

foreach(UNSUPPORTED "ubuntu" "macos" "matrix" "Install Linux dependencies" "Validate Linux" "Validate macOS" "ReverseVerb_AU" "build/release")
    if(CI_TEXT MATCHES "${UNSUPPORTED}")
        message(FATAL_ERROR "CI still contains unsupported token: ${UNSUPPORTED}")
    endif()
endforeach()

foreach(REQUIRED "windows-2022" "build-windows-debug" "build-windows-release" "test-windows-debug" "test-windows-release" "pluginval_Windows.zip" "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab" "Validate Windows VST3")
    if(NOT CI_TEXT MATCHES "${REQUIRED}")
        message(FATAL_ERROR "CI is missing required token: ${REQUIRED}")
    endif()
endforeach()

message(STATUS "Windows-only CI policy passed")
