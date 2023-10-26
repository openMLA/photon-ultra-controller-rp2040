from smbus2 import SMBus, i2c_msg
import spidev

import errno
from configure_controller import *
from utils import *
from time import sleep
from image_data_transfer import send_image_buffer

def set_test_line_pattern(address):
    block = bus.read_i2c_block_data(address, 0x0C, 6)
    # Returned value is a list of 16 bytes
    print("old test pattern settings is")
    for el in block:
        print(hex(el))


    testpattern_data = [int("10000110",2), 0, 8,8,8,8]
    #testpattern_data.reverse()
    print(testpattern_data)
    bus.write_i2c_block_data(address, 0x0B, testpattern_data) 
    sleep(0.3) 

    turn_on_test_pattern_mode(address, bus)



with SMBus(1) as bus:
    # try to write a dummy byte to the I2C address of the eViewTek board 
    # at address 27 (0x1d). It will give an error 
    DLPC1438_addr = 27
    check_i2c_listed(DLPC1438_addr, bus)

    # with I2C working, we can run a check for I2C connectivity
    spi = spidev.SpiDev()
    spi.open(0, 0)  # we use chip select pin 0 (if usni CE1, use (0,1))
    spi.max_speed_hz = 500000
    spi.mode = 0    

    #turn_on_test_pattern_mode(DLPC1438_addr)

    # sleep(3)
    # set_test_line_pattern(DLPC1438_addr)

    # print(bus.read_byte_data(DLPC1438_addr,0x53))  # should return 0x4, indicating LED 1 is enabled 

    sleep(1)

    ######################
    ### Exposing a pattern
    ######################

    ## setup #############

    prepare_exposure_mode(DLPC1438_addr, bus)  # run the basic setup steps

    send_image_buffer(spi)  # send image data (a dark dummy frame) 

    set_active_buffer_idx(DLPC1438_addr, bus, 1) # swap active buffer in  # TODO seems to be ignored

    enable_video_interface(DLPC1438_addr, bus)  # Set parallel buffer to "Read and send buffer" (enable FPGA video interface)

    turn_on_external_print_mode(DLPC1438_addr, bus)  # now we can switch to 'external print' mode

    sleep(1)  # TODO: handle more gracefully (need to wait for signal)

    # Set External Print Layer Control with the needed dark and exposed frames
    # NOTE: this will actually expose the pattern!
    expose_dark_and_light_frames(DLPC1438_addr, bus, 32,512)

    # OPTIONAL: check CRC for errors
    read_CRC(DLPC1438_addr, bus)  # TODO: format this nicely

    ## expose #############

    # framenumbers = [1, 2]

    # for frame in framenumbers:
    #     send_image_buffer()  # send image data (a dark dummy frame)  
    #     expose_dark_and_light_frames(DLPC1438_addr, bus, 32,512)
    #     # OPTIONAL: check CRC for errors