cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_TOOLCHAIN_FILE cmake/da16200-toolchain.cmake)

project(da16200-sdk C ASM)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

set(SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_library("umac" STATIC IMPORTED)
set_target_properties("umac" PROPERTIES IMPORTED_LOCATION ${SDK_ROOT}/library/libumac.a)

add_library("lmac" STATIC IMPORTED)
set_target_properties("lmac" PROPERTIES IMPORTED_LOCATION ${SDK_ROOT}/library/liblmac.mod.a)

add_library("romac" STATIC IMPORTED)
set_target_properties("romac" PROPERTIES IMPORTED_LOCATION ${SDK_ROOT}/library/libromac.a)

add_library("cryptopkg" STATIC IMPORTED)
set_target_properties("cryptopkg" PROPERTIES IMPORTED_LOCATION ${SDK_ROOT}/library/libcryptopkg.a)

if (NOT DEFINED MAIN_TARGET)
    message(FATAL_ERROR "MAIN_TARGET must be set to the main executable target")
endif(NOT DEFINED MAIN_TARGET)

if (NOT DEFINED MAIN_TARGET_NAME) 
    message(FATAL_ERROR "MAIN_TARGET_NAME must be set to the name of the main executable, sans filename")
endif (NOT DEFINED MAIN_TARGET_NAME)

if (NOT DEFINED MAIN_TARGET_INCLUDE)
    message(FATAL_ERROR "MAIN_TARGET_INCLUDE must be set to the main executable include directory list")
endif (NOT DEFINED MAIN_TARGET_INCLUDE)

include(main_app)
include(bootloader)

target_include_directories(${SDK_BOOT_IMG} PUBLIC ${MAIN_TARGET_INCLUDE})

add_custom_target(post_build_da16200
    ALL
    COMMAND ${SDK_ROOT}/cmake/helpers/run_custom_toolchain.sh ${TOOLCHAIN_BIN_PATH} ${SDK_ROOT}/tools/util/postbuild_start.sh ${SDK_ROOT}/tools dummy ${CMAKE_BINARY_DIR} ${MAIN_TARGET_NAME} DA16200
    VERBATIM
)
add_dependencies(post_build_da16200 ${MAIN_TARGET})

add_custom_target(post_build_ueboot
    ALL
    COMMAND ${SDK_ROOT}/cmake/helpers/run_custom_toolchain.sh ${TOOLCHAIN_BIN_PATH} ${SDK_ROOT}/tools/util/mk_boot_loader_image_start.sh ${SDK_ROOT}/tools dummy ${CMAKE_BINARY_DIR} ${MAIN_TARGET_NAME} ueboot
    VERBATIM
)

add_dependencies(post_build_ueboot ${SDK_BOOT_IMG})