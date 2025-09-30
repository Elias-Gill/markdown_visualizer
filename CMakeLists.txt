cmake_minimum_required(VERSION 3.16)
project(md_reader C)

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
add_executable(md_reader 
    main.c
    parser.c
    include/md4c/md4c.c
)

# Directorios de include
target_include_directories(md_reader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/clay
    ${CMAKE_CURRENT_SOURCE_DIR}/include/md4c
)

# Enlazar librerías
target_link_libraries(md_reader PUBLIC raylib m)

# ============================================================================
# CONFIGURACIÓN ADICIONAL
# ============================================================================

# Configuración específica de plataforma para Raylib
if(APPLE)
    target_link_libraries(md_reader 
        "-framework IOKit"
        "-framework Cocoa" 
        "-framework OpenGL"
    )
endif()

# En sistemas Unix, agregar pthread si es necesario
if(UNIX AND NOT APPLE)
    target_link_libraries(md_reader PUBLIC pthread)
endif()

# Copiar recursos al directorio de build
add_custom_command(
    TARGET md_reader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${CMAKE_CURRENT_BINARY_DIR}/resources
)
