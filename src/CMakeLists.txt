# cmake version
cmake_minimum_required(VERSION 3.13)

# include the sdk.cmake file
include(pico_sdk_import.cmake)

# give the project a name (anything you want)
project(DLP-pico-project)

# initialize the sdk
pico_sdk_init()

#######

# name anything you want
add_executable(DLP_pico test_image.c)

# must match with pio filename and executable name from above
pico_generate_pio_header(DLP_pico ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(DLP_pico ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(DLP_pico ${CMAKE_CURRENT_LIST_DIR}/pxl.pio)
pico_generate_pio_header(DLP_pico ${CMAKE_CURRENT_LIST_DIR}/pxl_clk.pio)

# must match with executable name and source file names
target_sources(DLP_pico PRIVATE DLP_pico.c)

# must match with executable name
target_link_libraries(DLP_pico PRIVATE pico_stdlib hardware_pio hardware_dma hardware_i2c)

# must match with executable name
pico_add_extra_outputs(DLP_pico)

# serial over USB please
pico_enable_stdio_usb(DLP_pico 1)
pico_enable_stdio_uart(DLP_pico 0)
