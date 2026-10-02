if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The Qt GUI and file layer require Linux; core targets remain portable.")
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Test LinguistTools)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(jsondict_files STATIC file_store.cpp)
target_include_directories(jsondict_files PUBLIC .)
target_link_libraries(jsondict_files PUBLIC jsondict_session Qt6::Core)
target_compile_options(jsondict_files PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(jsondict_file_tests ../tests/file_tests.cpp)
target_link_libraries(jsondict_file_tests PRIVATE jsondict_files)
add_test(NAME linux_atomic_files COMMAND jsondict_file_tests)

# The application target and QtTest interaction cases are added below as the
# GUI layer is implemented; core-only builds never find or link Qt.
