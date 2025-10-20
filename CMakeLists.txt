cmake_minimum_required(VERSION 3.16)
project(markdown_visualizer C)

set(CMAKE_C_STANDARD 99)

# ============================================================================
# DEPENDENCIAS EXTERNAS
# ============================================================================

# Raylib
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# Detectar plataforma y descargar el release correcto
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_linux_amd64.tar.gz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_macos.tar.gz")
else()
    message(FATAL_ERROR "Plataforma no soportada: ${CMAKE_SYSTEM_NAME}")
endif()

FetchContent_Declare(
    raylib
    URL ${RAYLIB_URL}
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(raylib)

# ============================================================================
# Configuracion del PROYECTO
# ============================================================================

# Archivos fuente principales
file(GLOB_RECURSE SRC_FILES "src/*.c") # listar ".c" recursivamente en 'src'
add_executable(markdown_visualizer 
    ${SRC_FILES} 
    include/md4c/md4c.c
)

if(WHITE_MODE)
    target_compile_definitions(markdown_visualizer PRIVATE WHITE_MODE)
endif()

# Directorios de include (donde buscar .h)
target_include_directories(markdown_visualizer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/clay
    ${CMAKE_CURRENT_SOURCE_DIR}/include/md4c
)

# Enlazar librerías al ejecutable
target_link_libraries(markdown_visualizer PUBLIC raylib m)

# ============================================================================
# CONFIGURACIÓN ADICIONAL
# ============================================================================

# Configuración específica de plataforma para Raylib
if(APPLE)
    target_link_libraries(markdown_visualizer 
        "-framework IOKit"
        "-framework Cocoa" 
        "-framework OpenGL"
    )
endif()

# En sistemas Unix, agregar pthread si es necesario
if(UNIX AND NOT APPLE)
    target_link_libraries(markdown_visualizer PUBLIC pthread)
endif()

# Copiar recursos al directorio de build
add_custom_command(
    TARGET markdown_visualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${CMAKE_CURRENT_BINARY_DIR}/resources
)
