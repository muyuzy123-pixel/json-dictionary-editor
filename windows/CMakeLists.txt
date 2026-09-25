cmake_minimum_required(VERSION 3.22)

project(JSONDictionaryEditorWindows VERSION 1.1.1 LANGUAGES CXX)
option(JDE_BUILD_GUI "Build the native Windows application" ${WIN32})

if(JDE_BUILD_GUI)
    if(NOT WIN32)
        message(FATAL_ERROR "The GUI requires a Windows target; use -DJDE_BUILD_GUI=OFF for core tests.")
    endif()
    enable_language(RC)
    add_executable(JSONDictionaryEditor WIN32
        src/json_core.cpp
        src/windows_app.cpp
        resources/app.rc
    )
    target_compile_features(JSONDictionaryEditor PRIVATE cxx_std_17)
    target_include_directories(JSONDictionaryEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    target_compile_definitions(JSONDictionaryEditor PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
    )
    target_link_libraries(JSONDictionaryEditor PRIVATE
        bcrypt comctl32 comdlg32 shell32 ole32 uxtheme gdi32 user32 uuid
    )
    if(MSVC)
        target_compile_options(JSONDictionaryEditor PRIVATE /utf-8 /W4 /permissive-)
        # app.rc embeds the PerMonitorV2 manifest as resource #1.
        target_link_options(JSONDictionaryEditor PRIVATE /MANIFEST:NO)
    elseif(MINGW)
        target_compile_options(JSONDictionaryEditor PRIVATE -Wall -Wextra -Wpedantic)
        target_link_options(JSONDictionaryEditor PRIVATE -municode -static -static-libgcc -static-libstdc++)
        set_source_files_properties(resources/app.rc PROPERTIES COMPILE_OPTIONS "--codepage=65001")
    endif()
endif()

add_executable(JSONDictionaryEditorCoreTests src/json_core.cpp src/core_self_test.cpp)
target_compile_features(JSONDictionaryEditorCoreTests PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(JSONDictionaryEditorCoreTests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(JSONDictionaryEditorCoreTests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME strict_json_core COMMAND JSONDictionaryEditorCoreTests)

# Build metadata contains no personal source/build paths.
file(SHA256 "${CMAKE_CXX_COMPILER}" JDE_COMPILER_SHA256)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/build-info-$<CONFIG>.txt" CONTENT
"project_version=${PROJECT_VERSION}\ncmake_version=${CMAKE_VERSION}\ngenerator=${CMAKE_GENERATOR}\ncompiler_id=${CMAKE_CXX_COMPILER_ID}\ncompiler_version=${CMAKE_CXX_COMPILER_VERSION}\ncompiler_sha256=${JDE_COMPILER_SHA256}\ntarget_system=${CMAKE_SYSTEM_NAME}\ntarget_processor=${CMAKE_SYSTEM_PROCESSOR}\ngenerator_platform=${CMAKE_GENERATOR_PLATFORM}\nwindows_sdk=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\nconfiguration=$<CONFIG>\ngui=${JDE_BUILD_GUI}\n")
