cmake_minimum_required(VERSION 3.16)
project(wbml C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(WBML_CONSOLE "Show console window on Windows (useful for debugging)" OFF)

# ----- SDL2 discovery -------------------------------------------------------
# Windows : vendored copy in third_party/SDL2
# macOS   : Homebrew (/opt/homebrew on Apple Silicon, /usr/local on Intel)
# Linux   : system package  (sudo apt install libsdl2-dev)
if(WIN32)
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/SDL2")
elseif(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

find_package(SDL2 REQUIRED)

# ----- Output directory (keeps build/ consistent with the existing Makefile) -
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg}" "${CMAKE_SOURCE_DIR}/build")
endforeach()

# ----- Target ---------------------------------------------------------------
add_executable(flydo
    src/win-ref/main.c
    src/win-ref/machine.c
    src/win-ref/mc8123.c
    src/win-ref/video.c
    src/win-ref/sn76489.c
    src/win-ref/cfg.c
    src/win-ref/menu.c
    src/win-ref/z80/z80.c
)

target_include_directories(flydo PRIVATE
    src/win-ref
    src/win-ref/z80
)

if(MSVC)
    target_compile_options(flydo PRIVATE /W4)
else()
    target_compile_options(flydo PRIVATE -Wall -Wextra)
endif()

# SDL2 >= 2.0.12 ships its own CMake config (imported targets).
# Older systems fall back to the CMake built-in find module (plain variables).
if(TARGET SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(flydo PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(flydo PRIVATE SDL2::SDL2)
else()
    target_include_directories(flydo PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(flydo PRIVATE ${SDL2_LIBRARIES})
endif()

# ----- Platform tweaks ------------------------------------------------------
if(WIN32)
    # No console window in release; keep visible when WBML_CONSOLE=ON
    if(NOT WBML_CONSOLE)
        set_target_properties(flydo PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    # Copy vendored SDL2.dll next to the executable
    add_custom_command(TARGET flydo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll"
            "$<TARGET_FILE_DIR:flydo>/SDL2.dll"
        COMMENT "Copying SDL2.dll"
    )
endif()

if(APPLE)
    # Embed the Homebrew SDL2 rpath so the binary is runnable without env tweaks
    set_target_properties(flydo PROPERTIES
        BUILD_RPATH  "/opt/homebrew/lib;/usr/local/lib"
        INSTALL_RPATH "/opt/homebrew/lib;/usr/local/lib"
    )
endif()
