cmake_minimum_required(VERSION 3.20)
project(candle C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W3 /wd4244 /wd4267 /wd4996)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
endif()

# ────────────────────────────────────────────────────────────────────────────
# candlec — the compiler / interpreter / REPL
# ────────────────────────────────────────────────────────────────────────────

set(CANDLEC_SRC
    src/main.c
    src/lexer.c
    src/token.c
    src/util.c
    src/ast.c
    src/parser.c
    src/sema.c
    src/codegen.c
    src/value.c
    src/interp.c
    src/threading.c
    src/builtins/builtins.c
    src/builtins/modules.c
    src/builtins/ffi.c
    src/builtins/ffi_mem.c
    src/builtins/http.c
    src/builtins/time.c
    src/builtins/fs.c
    src/builtins/file.c
    src/builtins/random.c
    src/builtins/process.c
    src/builtins/path.c
    src/builtins/encoding.c
    src/builtins/gui.c
    src/builtins/datetime.c
    src/builtins/collections.c
    src/builtins/crypto.c
    src/vendor/cjson/cJSON.c
)

add_executable(candlec ${CANDLEC_SRC})
target_include_directories(candlec PRIVATE src src/builtins)

# ── Boehm GC (interpreter only; transpiled programs use plain malloc) ─────
set(GC_ROOT "${CMAKE_SOURCE_DIR}/src/vendor/bdwgc")
target_include_directories(candlec PRIVATE "${GC_ROOT}/include")
target_link_libraries(candlec PRIVATE "${GC_ROOT}/build_clang/Release/gc.lib")
target_compile_definitions(candlec PRIVATE CANDLE_USE_BOEHM_GC GC_NOT_DLL)

# ── Win32 system libraries ────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(candlec PRIVATE ws2_32 gdi32 user32)
endif()

message(STATUS "Boehm GC: ${GC_ROOT}")

# ────────────────────────────────────────────────────────────────────────────
# candle_runtime — vendor-backed runtime library, linked to transpiled programs
# ────────────────────────────────────────────────────────────────────────────

set(VENDOR_SRC
    src/vendor/cjson/cJSON.c
    src/vendor/mongoose/mongoose.c
    src/std/gui_runtime.c
    src/std/_builtins_aot_runtime.c
)

add_library(candle_runtime STATIC ${VENDOR_SRC})
target_include_directories(candle_runtime PUBLIC src src/vendor)
target_compile_options(candle_runtime PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>
    $<$<C_COMPILER_ID:MSVC>:/w>
)

if(WIN32)
    target_link_libraries(candle_runtime PUBLIC ws2_32 gdi32 user32)
endif()

# Optional OpenMP for transpiled programs that use `parallel`
find_package(OpenMP COMPONENTS C QUIET)
if(OpenMP_C_FOUND)
    target_link_libraries(candle_runtime PUBLIC OpenMP::OpenMP_C)
    message(STATUS "OpenMP found — parallel candle programs supported")
else()
    message(STATUS "OpenMP not found — parallel programs will fall back to sequential")
endif()

# ────────────────────────────────────────────────────────────────────────────
# Helper: add_candle_executable(target src.candle)
# ────────────────────────────────────────────────────────────────────────────

function(add_candle_executable target_name candle_src)
    get_filename_component(candle_abs ${candle_src} ABSOLUTE)
    set(generated "${CMAKE_BINARY_DIR}/${target_name}.c")
    add_custom_command(
        OUTPUT  ${generated}
        COMMAND $<TARGET_FILE:candlec> --emit-c ${candle_abs} > ${generated}
        DEPENDS candlec ${candle_abs}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Transpiling ${candle_src} → ${target_name}.c"
        VERBATIM
    )
    add_executable(${target_name} ${generated})
    target_link_libraries(${target_name} PRIVATE candle_runtime)
endfunction()

# ────────────────────────────────────────────────────────────────────────────
# Examples (opt-in)
# ────────────────────────────────────────────────────────────────────────────

option(CANDLE_BUILD_EXAMPLES "Build .candle example programs" OFF)
if(CANDLE_BUILD_EXAMPLES)
    add_candle_executable(ex_basic    grammar/examples/basic.candle)
    add_candle_executable(ex_class    grammar/examples/class.candle)
    add_candle_executable(ex_control  grammar/examples/control.candle)
    add_candle_executable(ex_parallel grammar/examples/parallel.candle)
    add_candle_executable(ex_module   grammar/examples/module.candle)
endif()

# ────────────────────────────────────────────────────────────────────────────
# Status summary
# ────────────────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "  Candle build configuration")
message(STATUS "    C compiler:  ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "    Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "    Examples:    ${CANDLE_BUILD_EXAMPLES} (-DCANDLE_BUILD_EXAMPLES=ON to enable)")
message(STATUS "    OpenMP:      ${OpenMP_C_FOUND}")
message(STATUS "    GC:          Boehm ${GC_ROOT}")
message(STATUS "")
