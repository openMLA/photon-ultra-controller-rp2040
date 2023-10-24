from smbus2 import SMBus, i2c_msg

import errno

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




with SMBus(1) as bus:
    # try to write a dummy byte to the I2C address of the eViewTek board 
    # at address 27 (0x1d). It will give an error 
    DLPC1438_addr = 27
    check_i2c_listed(DLPC1438_addr)

    