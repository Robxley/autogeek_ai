# cmake/FFmpegFetch.cmake
include(FetchContent)

set(FFMPEG_VERSION "7.0")
set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip")

FetchContent_Declare(
    ffmpeg_bin
    URL ${FFMPEG_URL}
)

FetchContent_GetProperties(ffmpeg_bin)
if(NOT ffmpeg_bin_POPULATED)
    message(STATUS "Downloading FFmpeg binaries...")
    FetchContent_Populate(ffmpeg_bin)
endif()

# Le zip est extrait directement dans ffmpeg_bin_SOURCE_DIR dans les builds récents de BtbN
set(FFMPEG_ROOT_DIR "${ffmpeg_bin_SOURCE_DIR}")

set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT_DIR}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT_DIR}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT_DIR}/bin")

message(STATUS "FFmpeg Root: ${FFMPEG_ROOT_DIR}")

# Fonction pour créer une cible importée pour une lib FFmpeg
function(add_ffmpeg_library name)
    add_library(FFmpeg::${name} SHARED IMPORTED)
    
    # Préférer les fichiers .lib natifs MSVC fournis par BtbN
    set(LIB_PATH "${FFMPEG_LIB_DIR}/${name}.lib")
    set(DLL_PATH "${FFMPEG_BIN_DIR}/${name}-*.dll") # Les DLL ont souvent un suffixe de version
    
    # Résoudre le chemin exact de la DLL (car elle peut s'appeler avcodec-61.dll)
    file(GLOB DLL_RESOLVED "${FFMPEG_BIN_DIR}/${name}-*.dll")
    if(NOT DLL_RESOLVED)
        file(GLOB DLL_RESOLVED "${FFMPEG_BIN_DIR}/lib${name}-*.dll")
    endif()
    
    set_target_properties(FFmpeg::${name} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
        IMPORTED_IMPLIB "${LIB_PATH}"
        IMPORTED_LOCATION "${DLL_RESOLVED}"
    )
endfunction()

# Créer les cibles pour les composants principaux
set(FFMPEG_COMPONENTS avcodec avformat avutil swscale swresample)
foreach(comp ${FFMPEG_COMPONENTS})
    add_ffmpeg_library(${comp})
endforeach()

message(STATUS "FFmpeg targets created: avcodec, avformat, avutil, swscale, swresample")
