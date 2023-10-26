from utils import *
from time import sleep

# section 3.3.7
def set_ext_print_config(address, bus):
    '''Setup external print configuration to use linear gamma and enable led 1'''
    print("\n>>> setting external print configuration...")
    print(f"... old external config: {bus.read_i2c_block_data(address, 0xA9, 2)}")

    # byte 1 is degamma transfer function select
    # byte 2 is illuminator led enable - DLPC1438 docs are a bit vague here
    data = [
        0x00,
        int("00000000",2)
    ]
    bus.write_i2c_block_data(address, 0xA8, data)  # write to 

    print("...new external config:")
    print(bus.read_i2c_block_data(address, 0xA9, 2))


# TODO: this doesnt seem to update immediately.
def enable_CRC16(address, bus):
    '''Reads FPGA control register and enables CRC16 correction if not enabled'''
    print("\n>>> enabling CRC16...")
    print("...old FPGA settings:")
    old_settings = bus.read_byte_data(address, 0xCB) 
    print(bin(old_settings))  # print as binary, note that only final 4 bits are relevant
    new_settings = old_settings | 0b00000100
    print(bin(new_settings))
    bus.write_byte_data(address, 0xCA, new_settings)  

    # check
    sleep(1)
    checkdata = bus.read_byte_data(address, 0xCB)  
    print("...new FPGA settings:")
    print(bin(checkdata))  # print as binary, note that only final 4 bits are relevant
    print('!!! this setting does not actually seem to update...')


# section 3.3.12
# TODO: this doesnt seem to update immediately.
def set_active_buffer_idx(address, bus, index):
    '''Set the current active buffer index'''
    # Note: the documentation is very brief and for some reason the initial value at boot is
    # 198 (0b11000110), which doesn't make a whole lot of sense. Anyway, lets set it to 0?
    print("\n>>> active buffer index...")
    print("...old index settings:")
    old_settings = bus.read_byte_data(address, 0xC6)  
    print(bin(old_settings))
    print(old_settings)

    #bus.write_byte_data(address, 0xC5, 0b11111111)
    bus.write_byte_data(address, 0xC5, index)  

    sleep(1)
    print("...new index settings:")
    new_settings = bus.read_byte_data(address, 0xC6) 
    print(bin(new_settings))
    print(new_settings)
    print('!!! this setting does not actually seem to update...') 


# TODO: this doesnt seem to update immediately.
# manual section 3.3.10
def enable_video_interface(address, bus):
    '''enable the FPGA parallel video interface'''
    old_settings = bus.read_byte_data(address, 0xC4)  
    print("\n>>> enabling video interface...")
    print(f"old settings: {bin(old_settings)}")
    new_settings = old_settings | 0b00000001  # set b(0) to 1
    print(f"new settings: {bin(new_settings)}")
    bus.write_byte_data(address, 0xC3, new_settings)  
    sleep(1)
    print(f"... read back values: {bin(bus.read_byte_data(address, 0xC4))}")  
    print('!!! this setting does not actually seem to update...') 

# manual section 3.3.8
# TODO: this doesnt seem to update in registers
def expose_dark_and_light_frames(address, bus, n_dark, n_exposed):
    '''Sets the number of dark and exposed frames
    
    It is recommended to set the number of dark frames to 3 or more.'''
    print("\n>>> setting dark and light frames AND EXPOSING...")
    print("...old dark/exp settings:")
    old_settings = bus.read_i2c_block_data(address, 0xC2, 5)  
    for setting in old_settings:
        print(bin(setting))
    external_print_settings = old_settings[0]

    assert (n_dark < 65536) and (n_exposed < 65536), "number of dark and exposed frames much each be smaller than 2^16"

    # we need to convert n_dark, n_exposed into two bytes
    dark_bytes = bytearray(n_dark.to_bytes(2, 'big'))
    exposed_bytes = bytearray(n_exposed.to_bytes(2, 'big'))

    new_settings = [external_print_settings,
                     dark_bytes[1], dark_bytes[0], exposed_bytes[1], exposed_bytes[0]]

    bus.write_i2c_block_data(address, 0xC1, new_settings)  # write to 
    print("\n>>>>> EXPOSING <<<<<\n")

    sleep(1)
    check_vals = bus.read_i2c_block_data(address, 0xC2, 5)
    print("read back -> dark/exp settings")  
    for setting in check_vals:
        print(bin(setting))
    print('!!! this setting does not actually seem to update...') 
    

def read_CRC(address, bus):
    '''Read the CRC16 results of the SPI image data stream'''
    crcval_raw = bus.read_i2c_block_data(address, 0xCE, 2)  
    crcval = (crcval_raw[1] << 8) | crcval_raw[0]  # two bytes into a single 16-bit num
    print(f"CRC16 value: {bin(crcval)} ({crcval})")


def prepare_exposure_mode(address, bus):
    '''
    Do setup configuration for external print mode with FPGA front-end

    For more details see steps in DLPC1438 programmers guide section 3.3.2
    '''
    print("configuring the DLPC1438 for exposures")

    # ASSERT: are we in standby mode (or testpattern) -> see 3.3.6
    turn_on_standby_mode(address, bus)

    # check that CRC16 is enabled
    enable_CRC16(address, bus)
    
    # set active buffer number to 0 (we alternate between 0 and 1 between exposures) 
    set_active_buffer_idx(address, bus, 0)

    # Setup 'External Print Configuration'
    set_ext_print_config(address, bus)