cmake_minimum_required(VERSION 3.18)

if(DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
      CACHE FILEPATH "VitaSDK CMake toolchain")
endif()

project(vitaplus C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(vitaplus_core src/core/vitaplus.c)
target_include_directories(vitaplus_core PUBLIC include)
target_compile_options(vitaplus_core PRIVATE -Wall -Wextra -Werror)

if(DEFINED ENV{VITASDK})
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(VITA_APP_NAME "VitaPlus")
  set(VITA_TITLEID "VPLUS0001")
  set(VITA_VERSION "01.00")

  add_executable(vitaplus
    src/vita/main.c
    src/vita/ui_audio.c
    src/vita/ui_presentation.c
    src/vita/ui_renderer.c
    src/vita/vita_backend.c)
  # Catch large automatic buffers before they can exhaust a Vita thread stack.
  target_compile_options(vitaplus PRIVATE -Werror=frame-larger-than=16384)
  target_link_libraries(vitaplus vitaplus_core vita2d png jpeg curl zip z zstd bz2 lzma ssl crypto m pthread
    SceCtrl_stub SceDisplay_stub SceGxm_stub SceLibKernel_stub SceAppMgr_stub SceCommonDialog_stub SceIme_stub
    ScePgf_stub SceIofilemgr_stub SceNet_stub SceNetCtl_stub SceSsl_stub SceSysmodule_stub SceAudio_stub
    ScePromoterUtil_stub_weak)
  configure_file("$ENV{VITASDK}/arm-vita-eabi/ssl/openssl.cnf"
    "${CMAKE_CURRENT_BINARY_DIR}/openssl.cnf" COPYONLY)
  vita_create_self(eboot.bin vitaplus UNSAFE)
  # Explicit inventory: a missing generated icon must fail packaging, not ship silently.
  set(VP_AURORA_ASSETS wallpaper emblem
    platform_nes platform_famicom platform_snes platform_gameboy platform_gba platform_nds
    platform_genesis platform_mastersystem platform_gamegear platform_psx platform_psp platform_vita
    platform_arcade platform_pce platform_atari2600 platform_wonderswan platform_homebrew platform_generic
    platform_dreamcast platform_n3ds platform_gamecube platform_ps2 platform_wii
    ui_library ui_home ui_store ui_settings ui_download ui_check ui_star ui_search
    ui_play ui_back ui_grid ui_list ui_carousel ui_sync ui_key ui_folder ui_romm)
  set(VP_CONTROLLER_ASSETS
    platform_arcade platform_dreamcast platform_gba platform_n3ds platform_nds platform_gamecube
    platform_psx platform_ps2 platform_psp platform_genesis platform_snes platform_wii
    ui_library ui_home ui_store ui_settings ui_download ui_check ui_star ui_search
    ui_play ui_back ui_grid ui_list ui_carousel ui_sync ui_key ui_folder ui_romm)
  set(VP_AURORA_FILES)
  foreach(asset IN LISTS VP_AURORA_ASSETS)
    if(asset IN_LIST VP_CONTROLLER_ASSETS)
      set(asset_source "assets/controller-tiles/${asset}.png")
    else()
      set(asset_source "assets/aurora/${asset}.png")
    endif()
    list(APPEND VP_AURORA_FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/${asset_source}" "assets/aurora/${asset}.png")
  endforeach()
  vita_create_vpk(vitaplus.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
     NAME ${VITA_APP_NAME}
     FILE ${CMAKE_CURRENT_SOURCE_DIR}/themes/default/theme.ini assets/default/theme.ini
            ${CMAKE_CURRENT_BINARY_DIR}/openssl.cnf openssl.cnf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/controller-tiles/RomM-AGPL-3.0.txt assets/licenses/RomM-AGPL-3.0.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/controller-tiles/romm-isotipo.svg assets/licenses/romm-isotipo.svg
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/controller-tiles/NOTICE.md assets/licenses/controller-icons.md
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/01-navegacion-alternativa.wav assets/audio/01-navegacion-alternativa.wav
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/02-cambio-tab.wav assets/audio/02-cambio-tab.wav
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/03-entrar.wav assets/audio/03-entrar.wav
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/04-volver-salir.wav assets/audio/04-volver-salir.wav
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/05-iniciar-descarga.wav assets/audio/05-iniciar-descarga.wav
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/06-descarga-completa.wav assets/audio/06-descarga-completa.wav
           ${VP_AURORA_FILES})
else()
  message(STATUS "VITASDK not set: only vitaplus_core is configured")
endif()
