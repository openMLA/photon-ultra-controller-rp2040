from smbus2 import SMBus, i2c_msg

import errno

def check_i2c_listed(address, bus):
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

def turn_on_standby_mode(address, bus):
    mode = bus.read_byte_data(address,6)  
    if mode != 0xFF:
        print(f"Switching to standby mode (255 / 0xFF) from current mode ({mode})")
        bus.write_byte_data(address, 5, 0xFF)  
    else:
        print("already in standby mode mode (255 / 0xFF), ignoring mode switch request")

def turn_on_external_print_mode(address, bus):
    mode = bus.read_byte_data(address,6)  # should be external print after first boot: (6)
    if mode != 6:
        print(f"Switching to external print mode (6) from current mode ({mode})")
        bus.write_byte_data(address, 5, 6)  
    else:
        print("already in external print mode (6), ignoring mode switch request")


def turn_on_test_pattern_mode(address, bus):
    mode = bus.read_byte_data(address,6)  # should be external print after first boot: (6)
    if mode != 1:
        print(f"Switching to test pattern mode (1) from current mode ({mode})")
        bus.write_byte_data(address, 5, 1)
    else:
        print("already in test pattern mode (1), ignoring mode switch request")