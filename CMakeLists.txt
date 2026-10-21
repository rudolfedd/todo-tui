cmake_minimum_required(VERSION 3.16)
project(todo-tui VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(todo-tui
  src/main.c
  src/app.c
  src/ui.c
  src/model.c
  src/store.c
  src/json.c
  src/editor.c
  src/util.c
)

if(MSVC)
  target_compile_options(todo-tui PRIVATE /W4 /utf-8)
  target_compile_definitions(todo-tui PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(todo-tui PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

# --- curses backend -------------------------------------------------------
if(WIN32)
  # PDCurses (MSYS2 / mingw-w64)
  find_library(PDCURSES_LIB NAMES pdcurses pdcurses_wincon)
  if(PDCURSES_LIB)
    target_link_libraries(todo-tui PRIVATE ${PDCURSES_LIB})
  else()
    message(WARNING "PDCurses not found — install mingw-w64-x86_64-pdcurses")
  endif()
else()
  find_package(Curses REQUIRED)
  target_include_directories(todo-tui PRIVATE ${CURSES_INCLUDE_DIRS})
  target_link_libraries(todo-tui PRIVATE ${CURSES_LIBRARIES})
endif()

install(TARGETS todo-tui RUNTIME DESTINATION bin)

enable_testing()
add_executable(test_json tests/test_json.c src/json.c src/util.c)
target_include_directories(test_json PRIVATE src)
add_test(NAME json COMMAND test_json)
