cmake_minimum_required(VERSION 3.25)
project(MidiSynthC23 LANGUAGES C)

# --- Force C23 ---
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Environment Check ---
if(MSVC)
    message(FATAL_ERROR "MSVC detected! This project uses C23 features (static constexpr) not yet supported by cl.exe. Please use Clang with the Ninja generator.")
endif()

# --- Compiler Flags ---
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Clang on Windows might need -std=c23 explicitly if it's an older version
    add_compile_options(-std=c23 -Wall -Wextra -O3)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -O3)
endif()

# --- Python & Path Logic ---
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MIDI_FILE "input.mid" CACHE STRING "Path to the MIDI file to compile")
set(MIDI_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${MIDI_FILE}")
set(OUTPUT_BASE "theme_song")
# Use absolute paths for the generator to avoid Ninja path confusion
set(GEN_C_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}.c")
set(GEN_BIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}_data.bin")

# If you have 'uv' installed, this is the most robust way:
find_program(UV_EXE uv)

if(UV_EXE)
    set(PYTHON_RUN_CMD ${UV_EXE} run --with mido)
else()
    set(PYTHON_RUN_CMD ${Python3_EXECUTABLE})
endif()

add_custom_command(
    OUTPUT "${GEN_C_FILE}" "${GEN_BIN_FILE}"
    # Use 'cmake -E env' to set UTF8 mode for the Python process
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUTF8=1 
            ${PYTHON_RUN_CMD} "${CMAKE_CURRENT_SOURCE_DIR}/midi_compiler.py" 
            "${MIDI_INPUT}" "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/midi_compiler.py" "${MIDI_INPUT}"
    COMMENT "Compiling MIDI to C and Binary Assets..."
)

# --- Step 2: Main Executable ---
add_executable(music_renderer synth_dsl.c "${GEN_C_FILE}")
target_include_directories(music_renderer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# --- Threading & Math ---
if(WIN32)
    # On Windows, threads are part of Kernel32, usually no extra lib needed for Clang
    target_link_libraries(music_renderer PRIVATE) 
else()
    find_package(Threads REQUIRED)
    target_link_libraries(music_renderer PRIVATE Threads::Threads m)
endif()

# --- Step 3: Render Target ---
add_custom_target(render
    COMMAND music_renderer
    DEPENDS music_renderer
    COMMENT "Generating WAV file..."
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)