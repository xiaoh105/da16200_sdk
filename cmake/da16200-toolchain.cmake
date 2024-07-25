set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT TOOLCHAIN_PATH)
     set(TOOLCHAIN_PATH "/usr")
     message(STATUS "No TOOLCHAIN_PATH specified, using default: " ${TOOLCHAIN_PATH})
else()
     file(TO_CMAKE_PATH "${TOOLCHAIN_PATH}" TOOLCHAIN_PATH)
     message(STATUS "Setting toolchain to " ${TOOLCHAIN_PATH})
endif()

if(NOT TARGET_TRIPLET)
    set(TARGET_TRIPLET "arm-none-eabi")
    message(STATUS "No TARGET_TRIPLET specified, using default: " ${TARGET_TRIPLET})
endif()


set(TOOLCHAIN_SYSROOT  "${TOOLCHAIN_PATH}/${TARGET_TRIPLET}")
set(TOOLCHAIN_BIN_PATH "${TOOLCHAIN_PATH}/bin")
set(TOOLCHAIN_INC_PATH "${TOOLCHAIN_PATH}/${TARGET_TRIPLET}/include")
set(TOOLCHAIN_LIB_PATH "${TOOLCHAIN_PATH}/${TARGET_TRIPLET}/lib")

find_program(CMAKE_OBJCOPY NAMES ${TARGET_TRIPLET}-objcopy PATHS ${TOOLCHAIN_BIN_PATH} NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP NAMES ${TARGET_TRIPLET}-objdump PATHS ${TOOLCHAIN_BIN_PATH} NO_DEFAULT_PATH)
find_program(CMAKE_SIZE NAMES ${TARGET_TRIPLET}-size PATHS ${TOOLCHAIN_BIN_PATH} NO_DEFAULT_PATH)
find_program(CMAKE_DEBUGGER NAMES ${TARGET_TRIPLET}-gdb PATHS ${TOOLCHAIN_BIN_PATH} NO_DEFAULT_PATH)

find_program(CMAKE_C_COMPILER NAMES ${TARGET_TRIPLET}-gcc PATHS ${TOOLCHAIN_BIN_PATH})
find_program(CMAKE_CXX_COMPILER NAMES ${TARGET_TRIPLET}-g++ PATHS ${TOOLCHAIN_BIN_PATH})
find_program(CMAKE_ASM_COMPILER NAMES ${TARGET_TRIPLET}-gcc PATHS ${TOOLCHAIN_BIN_PATH})


add_compile_options(
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    --specs=nano.specs
    --specs=nosys.specs
    -nostartfiles
    -fmessage-length=0 
    -fsigned-char 
    -ffunction-sections 
    -fdata-sections 
    -ffreestanding 
    -fno-move-loop-invariants
    -Os
)

add_link_options(
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    --specs=nano.specs
    --specs=nosys.specs
    -nostartfiles
    -fmessage-length=0 
    -fsigned-char 
    -ffunction-sections 
    -fdata-sections 
    -ffreestanding 
    -fno-move-loop-invariants
    -Os
    -Xlinker --gc-sections
)