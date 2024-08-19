set(SDK_MAIN_IMG "DA16200_SDK_MAIN")
add_library(${SDK_MAIN_IMG} INTERFACE)

set(SDK_MAIN_INCLUDE
    ${SDK_ROOT}/apps/app_common/include/provisioning
    ${SDK_ROOT}/apps/app_common/include/system
    ${SDK_ROOT}/apps/app_common/include/util
    ${SDK_ROOT}/core/config
    ${SDK_ROOT}/core/bsp/cmsis/include
    ${SDK_ROOT}/core/bsp/cortexm/include
    ${SDK_ROOT}/core/bsp/driver/include
    ${SDK_ROOT}/core/bsp/driver/include/DA16200
    ${SDK_ROOT}/core/freertos/include
    ${SDK_ROOT}/core/freertos/portable/GCC/ARM_CM4F
    ${SDK_ROOT}/core/libraries/3rdparty/lwip/src/include
    ${SDK_ROOT}/core/libraries/3rdparty/lwip_osal/include
    ${SDK_ROOT}/core/libraries/3rdparty/lwip/src/include/lwip/apps
    ${SDK_ROOT}/core/libraries/3rdparty/crypto/include
    ${SDK_ROOT}/core/libraries/3rdparty/json
    ${SDK_ROOT}/core/libraries/3rdparty/lwrb/include
    ${SDK_ROOT}/core/system/include
    ${SDK_ROOT}/core/system/include/at_cmd
    ${SDK_ROOT}/core/system/include/dpm
    ${SDK_ROOT}/core/system/include/coap
    ${SDK_ROOT}/core/system/include/common
    ${SDK_ROOT}/core/system/include/common/library
    ${SDK_ROOT}/core/system/include/crypto
    ${SDK_ROOT}/core/system/include/crypto/cryptocell
    ${SDK_ROOT}/core/system/include/crypto/mbedtls
    ${SDK_ROOT}/core/system/include/crypto/shared
    ${SDK_ROOT}/core/system/include/crypto/shared/hw/include
    ${SDK_ROOT}/core/system/include/crypto/shared/include
    ${SDK_ROOT}/core/system/include/crypto/shared/include/cc_util
    ${SDK_ROOT}/core/system/include/crypto/shared/include/crypto_api
    ${SDK_ROOT}/core/system/include/crypto/shared/include/crypto_api/cc3x
    ${SDK_ROOT}/core/system/include/crypto/shared/include/pal
    ${SDK_ROOT}/core/system/include/crypto/shared/include/pal/freertos
    ${SDK_ROOT}/core/system/include/crypto/shared/include/proj/cc3x
    ${SDK_ROOT}/core/system/include/crypto/shared/include/sbrom
    ${SDK_ROOT}/core/system/include/crypto/shared/include/trng
    ${SDK_ROOT}/core/system/include/mqtt_client
    ${SDK_ROOT}/core/system/include/mqtt_client/lib
    ${SDK_ROOT}/core/system/include/provision
    ${SDK_ROOT}/core/system/include/ota
    ${SDK_ROOT}/core/system/include/ramlib
    ${SDK_ROOT}/core/system/include/temp
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/ec_wrst
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/rsa
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/cc3x_sym/driver
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/secure_boot_gen
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/rnd_dma
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/common
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/platform/stage
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/crypto_driver
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/common
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/cc3x_verifier
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/secure_debug
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/platform/common
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/platform/pal
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_verifier
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_cert_parser
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/util
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/common
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/platform/nvm
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/platform/hal
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_wrst
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/cc3x_sym/api
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_mont
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_edw
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/rsa
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/poly
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/mbedtls_api
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/srp
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/host/cc_mng
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/host/cc3x_lib
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/host/cc3x_productionlib/cmpu
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/host/cc3x_productionlib/common
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/host/cc3x_sbromlib
    ${SDK_ROOT}/core/system/src/crypto/cryptocell/host/hal
    ${SDK_ROOT}/core/wifistack/romac/include
    ${SDK_ROOT}/core/wifistack/umac/inc
    ${SDK_ROOT}/core/wifistack/supplicant/src
    ${SDK_ROOT}/core/wifistack/supplicant/src/ap
    ${SDK_ROOT}/core/wifistack/supplicant/src/common
    ${SDK_ROOT}/core/wifistack/supplicant/src/crypto
    ${SDK_ROOT}/core/wifistack/supplicant/src/drivers
    ${SDK_ROOT}/core/wifistack/supplicant/src/eap_common
    ${SDK_ROOT}/core/wifistack/supplicant/src/eap_peer
    ${SDK_ROOT}/core/wifistack/supplicant/src/eap_server
    ${SDK_ROOT}/core/wifistack/supplicant/src/eapol_auth
    ${SDK_ROOT}/core/wifistack/supplicant/src/eapol_supp
    ${SDK_ROOT}/core/wifistack/supplicant/src/l2_packet
    ${SDK_ROOT}/core/wifistack/supplicant/src/p2p
    ${SDK_ROOT}/core/wifistack/supplicant/src/pae
    ${SDK_ROOT}/core/wifistack/supplicant/src/radius
    ${SDK_ROOT}/core/wifistack/supplicant/src/rsn_supp
    ${SDK_ROOT}/core/wifistack/supplicant/src/utils
    ${SDK_ROOT}/core/wifistack/supplicant/wpa_supplicant
    ${SDK_ROOT}/core/wifistack/wpa_cli
    ${SDK_ROOT}/core/segger_tools/Config
    ${SDK_ROOT}/core/segger_tools/OS
    ${SDK_ROOT}/core/segger_tools/SEGGER
    ${SDK_ROOT}/tools/version/genfiles
    ${SDK_ROOT}/core/main/include
    ${SDK_ROOT}/core/main/config
)
target_include_directories(${SDK_MAIN_IMG} PUBLIC INTERFACE ${SDK_MAIN_INCLUDE})

set(BUILD_TIME_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/time)
add_custom_command(OUTPUT ${BUILD_TIME_FILENAME}
    COMMAND ${SDK_ROOT}/tools/util/prebuild_start.sh ${SDK_ROOT}/tools dummy ${CMAKE_CURRENT_BINARY_DIR} da16200
    VERBATIM
)

file(GLOB_RECURSE SDK_MAIN_SOURCES
    ${SDK_ROOT}/apps/app_common/*.c
    ${SDK_ROOT}/core/bsp/*.c
    ${SDK_ROOT}/core/config/*.c
    ${SDK_ROOT}/core/freertos/*.c
    ${SDK_ROOT}/core/libraries/*.c
    ${SDK_ROOT}/core/segger_tools/*.c
    ${SDK_ROOT}/core/system/*.c
    ${SDK_ROOT}/core/wifistack/*.c
    ${SDK_ROOT}/core/bsp/startup/*.S
    ${SDK_ROOT}/core/main/*.c
)

list(APPEND SDK_MAIN_SOURCES ${BUILD_TIME_FILENAME})

list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/property/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_verifier/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_cert_parser/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/util/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_cert_parser/cc3x_sb_x509_parser\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/rnd_dma/llf_rnd_fetrng\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/dh/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/kdf/cc_hkdf\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/mbedtls_api/platform_alt\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/host/pal/threadx/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/host/pal/no_os/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/host/pal/linux/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/tests/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_wrst/pka_ec_wrst_smul_no_scap\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_wrst/ec_wrst_dsa_verify\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/wifistack/supplicant/src/crypto/sha256\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/wifistack/supplicant/src/crypto/sha1\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/libraries/3rdparty/lwip/src/apps/http/fsdata\\.c")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/libraries/3rdparty/lwip/src/apps/http/makefsdata/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/libraries/3rdparty/lwip/src/apps/http/fs/*")
list(FILTER SDK_MAIN_SOURCES EXCLUDE REGEX "core/wifistack/supplicant/src/utils/browser-wpadebug\\.c")

target_sources(${SDK_MAIN_IMG} PUBLIC INTERFACE ${SDK_MAIN_SOURCES})



target_compile_definitions(${SDK_MAIN_IMG} PUBLIC INTERFACE
    DEBUG
    TRACE
    OS_USE_TRACE_SEMIHOSTING_DEBUG
    __DA16200__
    DA16200_MODULE_AAC
    MBEDTLS_CONFIG_FILE="mbedtls/config.h"
    CC_IOT
    CC_SB_SUPPORT_IOT
    CC_SUPPORT_PKA_64_16
    DLLI_MAX_BUFF_SIZE=0x10000
    CC_DOUBLE_BUFFER_MAX_SIZE_IN_BYTES=8192
    CC_MNG_MIN_BACKUP_SIZE_IN_BYTES=512
    CC_SB_CERT_VERSION_MAJOR=1
    CC_SB_CERT_VERSION_MINOR=0
    HASH_SHA_512_SUPPORTED
    CC_CONFIG_TRNG_MODE=1
    CC_SB_INDIRECT_SRAM_ACCESS
    USE_MBEDTLS_CRYPTOCELL
    CC_HW_VERSION=0xFF
    ENABLE_AES_DRIVER=1
    __SUPPORT_EVK_LED__
    __TCP_CLIENT_SLEEP2_SAMPLE__
    CONFIG_RTT
)


target_link_libraries(${SDK_MAIN_IMG} PUBLIC INTERFACE "umac")
target_link_libraries(${SDK_MAIN_IMG} PUBLIC INTERFACE "lmac")
target_link_libraries(${SDK_MAIN_IMG} PUBLIC INTERFACE "romac")
target_link_libraries(${SDK_MAIN_IMG} PUBLIC INTERFACE "cryptopkg")
target_link_libraries(${SDK_MAIN_IMG} PUBLIC INTERFACE "m")

target_link_directories(${SDK_MAIN_IMG} INTERFACE ${SDK_ROOT}/core/bsp/ldscripts)
target_link_options(${SDK_MAIN_IMG} PUBLIC INTERFACE "-T" "sections_xip.ld")