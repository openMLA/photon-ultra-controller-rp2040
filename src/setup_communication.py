from smbus2 import SMBus, i2c_msg

import errno
from time import sleep

def check_i2c_listed(address):
    print("Checking if DLPC1436 is found on I2C...")
    try:
        bus.write_byte(address, 0)
        print("> Found eViewTekboard on I2C")
    except IOError as e:
        if e.errno != errno.EREMOTEIO:
            print("> Error: {0} on address {1}".format(e, hex(address)))
        else:
            print(f"> Unable to find eViewTek board at address {address} {hex(address)}")
            print(f"> You may need to switch the board off and on with SW1")
    except Exception as e: # exception if read_byte fails
        print("> Error unk: {0} on address {1}".format(e, hex(address)))


def turn_on_external_print_mode(address):
    mode = bus.read_byte_data(DLPC1438_addr,6)  # should be external print after first boot: (6)
    if mode != 6:
        print(f"Switching to external print mode (6) from current mode ({mode})")
        bus.write_byte_data(DLPC1438_addr, 5, 6)  
    else:
        print("already in external print mode (6), ignoring mode switch request")


def turn_on_test_pattern_mode(address):
    mode = bus.read_byte_data(DLPC1438_addr,6)  # should be external print after first boot: (6)
    if mode != 1:
        print(f"Switching to test pattern mode (1) from current mode ({mode})")
        bus.write_byte_data(DLPC1438_addr, 5, 1)
    else:
        print("already in test pattern mode (1), ignoring mode switch request")


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

    turn_on_test_pattern_mode(address)


with SMBus(1) as bus:
    # try to write a dummy byte to the I2C address of the eViewTek board 
    # at address 27 (0x1d). It will give an error 
    DLPC1438_addr = 27
    check_i2c_listed(DLPC1438_addr)

    #turn_on_test_pattern_mode(DLPC1438_addr)

    sleep(3)
    set_test_line_pattern(DLPC1438_addr)

    print(bus.read_byte_data(DLPC1438_addr,0x53))  # should return 0x4, indicating LED 1 is enabled 

    sleep(4)
    turn_on_external_print_mode(DLPC1438_addr)
    print(bus.read_byte_data(DLPC1438_addr,0x53))  # should be external print after first boot: (6)




    