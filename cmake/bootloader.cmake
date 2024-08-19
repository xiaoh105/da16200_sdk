set(SDK_BOOT_IMG "sdk_ueboot.elf")
add_executable(${SDK_BOOT_IMG})

set(SDK_BOOT_INCLUDE
    ${SDK_ROOT}/apps/app_common/include/system
    ${SDK_ROOT}/apps/da16200/get_started/projects/ueboot/include
    ${SDK_ROOT}/core/config
    ${SDK_ROOT}/core/bsp/cmsis/include
    ${SDK_ROOT}/core/bsp/cortexm/include
    ${SDK_ROOT}/core/bsp/driver/include
    ${SDK_ROOT}/core/bsp/driver/include/DA16200
    ${SDK_ROOT}/core/bsp/driver/include/DA16200/sflash
    ${SDK_ROOT}/core/freertos/include
    ${SDK_ROOT}/core/freertos/portable/GCC/ARM_CM4F
    ${SDK_ROOT}/core/libraries/3rdparty/lwip/src/include
    ${SDK_ROOT}/core/libraries/3rdparty/lwip_osal/include
    ${SDK_ROOT}/core/system/include
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
    ${SDK_ROOT}/tools/version/genfiles/
)
target_include_directories(${SDK_BOOT_IMG} PUBLIC ${SDK_BOOT_INCLUDE})


file(GLOB_RECURSE SDK_BOOT_SOURCES
    ${SDK_ROOT}/core/bsp/*.c
    ${SDK_ROOT}/core/config/*.c
    ${SDK_ROOT}/core/freertos/*.c
    ${SDK_ROOT}/core/libraries/lwip/*.c
    ${SDK_ROOT}/core/libraries/lwip_osal/*.c
    ${SDK_ROOT}/core/system/src/common/*.c
    ${SDK_ROOT}/core/system/src/crypto/*.c
    ${SDK_ROOT}/core/system/src/diag/*.c
    ${SDK_ROOT}/core/bsp/startup/*.S
    ${SDK_ROOT}/apps/da16200/get_started/projects/ueboot/*.c
    ${BUILD_TIME_FILENAME}
)


list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/property/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_verifier/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_cert_parser/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/util/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/secure_boot_debug/x509_cert_parser/cc3x_sb_x509_parser\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/rnd_dma/llf_rnd_fetrng\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/dh/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/kdf/cc_hkdf\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/mbedtls_api/platform_alt\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/host/pal/threadx/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/host/pal/no_os/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/host/pal/linux/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/tests/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_wrst/pka_ec_wrst_smul_no_scap\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/crypto/cryptocell/codesafe/crypto_api/pki/ec_wrst/ec_wrst_dsa_verify\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/util_api\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/sys_apps\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/sys_i2s\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/sys_i2c\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/spi\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/sdio_slave\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/sdio_cis\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/gpio\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/driver/DA16200/adc\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/common_host\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/gpio_handle\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/asd\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_sys_os\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/benchmark/*")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/patch\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/sys_feature\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/common_config\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/syscommand\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/common_uart1\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/sys_common_func\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/sys_user_feature\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/rf_meas_api\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/init_umac\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/easy_setup\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_umac\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_sys\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_sys_hal\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_spi_master\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_rf\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_pwm\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_nvedit\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_net\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_net_cmd\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_mem\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_lmac\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_lmac_tx\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_lmac_msg\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_i2s\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_i2c\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_host\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_adc\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/main/da16x_initialize\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/command/command_net_sys\\.c")
list(FILTER SDK_BOOT_SOURCES EXCLUDE REGEX "core/system/src/common/network/*")

target_sources(${SDK_BOOT_IMG} PUBLIC ${SDK_BOOT_SOURCES})


target_compile_definitions(${SDK_BOOT_IMG} PUBLIC 
    DEBUG
    TRACE
    OS_USE_TRACE_SEMIHOSTING_DEBUG
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
)

target_link_libraries(${SDK_BOOT_IMG} PUBLIC  "cryptopkg")

target_link_directories(${SDK_BOOT_IMG} PUBLIC ${SDK_ROOT}/core/bsp/ldscripts)
target_link_options(${SDK_BOOT_IMG} PUBLIC  "-T" "sections_boot.ld")
