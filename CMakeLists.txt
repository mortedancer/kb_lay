cmake_minimum_required(VERSION 3.16)
project(kb_lay C)

if(NOT WIN32)
  message(FATAL_ERROR "kb_lay is Windows-only")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(kb_lay WIN32 src/kb_lay.c)
target_compile_definitions(kb_lay PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(kb_lay PRIVATE user32 shell32 advapi32)

add_executable(kb_lay_test src/kb_lay.c)
target_compile_definitions(kb_lay_test PRIVATE KB_LAY_TEST _CRT_SECURE_NO_WARNINGS)

if(MSVC)
  target_compile_options(kb_lay PRIVATE /W3 /O1 /utf-8)
  target_compile_options(kb_lay_test PRIVATE /W3 /O1 /utf-8)
else()
  target_compile_options(kb_lay PRIVATE -Wall -O2 -finput-charset=UTF-8)
  target_link_options(kb_lay PRIVATE -static)
  target_compile_options(kb_lay_test PRIVATE -Wall -O2 -finput-charset=UTF-8)
endif()

enable_testing()
add_test(NAME layout_map COMMAND kb_lay_test)
