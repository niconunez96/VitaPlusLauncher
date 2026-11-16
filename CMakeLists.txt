cmake_minimum_required(VERSION 3.18)
project(vitaplus C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(vitaplus_core src/core/vitaplus.c)
target_include_directories(vitaplus_core PUBLIC include)
target_compile_options(vitaplus_core PRIVATE -Wall -Wextra -Werror)

if(DEFINED ENV{VITASDK})
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  find_package(CURL REQUIRED)
  set(VITA_APP_NAME "VitaPlus")
  set(VITA_TITLEID "VPLUS0001")
  set(VITA_VERSION "0.1.0")

  add_executable(vitaplus src/vita/main.c)
  target_link_libraries(vitaplus vitaplus_core vita2d CURL::libcurl
    SceCtrl_stub SceDisplay_stub SceGxm_stub SceLibKernel_stub SceAppMgr_stub ScePgf_stub)
  vita_create_self(eboot.bin vitaplus UNSAFE)
  vita_create_vpk(vitaplus.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME})
else()
  message(STATUS "VITASDK not set: only vitaplus_core is configured")
endif()
