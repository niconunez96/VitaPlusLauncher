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
    src/vita/vita_backend.c)
  target_link_libraries(vitaplus vitaplus_core vita2d curl zip z zstd bz2 lzma ssl crypto m pthread
    SceCtrl_stub SceDisplay_stub SceGxm_stub SceLibKernel_stub SceAppMgr_stub SceCommonDialog_stub
    ScePgf_stub SceIofilemgr_stub SceNet_stub SceNetCtl_stub SceSsl_stub SceSysmodule_stub
    ScePromoterUtil_stub_weak)
  vita_create_self(eboot.bin vitaplus UNSAFE)
  vita_create_vpk(vitaplus.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/themes/default/theme.ini assets/default/theme.ini)
else()
  message(STATUS "VITASDK not set: only vitaplus_core is configured")
endif()
