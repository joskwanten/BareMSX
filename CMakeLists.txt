# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoCalcMSX C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(PicoCalcMSX
        PicoCalcMSX.c
        lcd.c
        i2ckbd.c
        keymap.c
        audio.c
        # MSX emulator core (ported from sdl-msx)
        emu/machine.c
        zeta/Z80.c
        emu/tms9918.c
        emu/PPI.c
        emu/slots.c
        emu/subslots.c
        emu/ram.c
        emu/rom.c
        emu/empty.c
        emu/util.c
        emu/konami-mega-rom-scc.c
        emu/emu2149.c
        )

pico_set_program_name(PicoCalcMSX "PicoCalcMSX")
pico_set_program_version(PicoCalcMSX "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(PicoCalcMSX 0)
pico_enable_stdio_usb(PicoCalcMSX 1)

# Add the standard library to the build
target_link_libraries(PicoCalcMSX
        pico_stdlib
        pico_multicore)

# Add the standard include files to the build
target_include_directories(PicoCalcMSX PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/emu
        ${CMAKE_CURRENT_LIST_DIR}/zeta
)

# Zeta Z80 core wordt statisch mee-gecompileerd
target_compile_definitions(PicoCalcMSX PRIVATE Z80_STATIC)

# Add any user requested libraries
target_link_libraries(PicoCalcMSX
        hardware_spi
        hardware_i2c
        hardware_dma
        hardware_pwm
        pico_cyw43_arch_none
        )

pico_add_extra_outputs(PicoCalcMSX)

