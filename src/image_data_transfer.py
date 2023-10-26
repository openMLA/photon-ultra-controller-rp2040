import spidev
from crcmod import mkCrcFun
from CRCcheck_TI import RowColIdx, crc

# TODO:  set buffer size in SPI, it is stored in /sys/module/spidev/parameters/bufsiz

# TODO: probably use writebuf2 and input a np.bytearray. Challenge: auto-chunking issues

def send_image_buffer(spi):
    '''Send image data over SPI to FPGA and switch buffer index'''
    print("\n>>>  sending image data over spi...")

    # form the pixel data
    data = [0x80] * int((128*4)/64)

    # format the SPI command
    swap = False
    row_col_idx = RowColIdx(active_buffer=0, row_start_index=180, col_start_index=5, col_end_index=6)
    SPI_cmd = crc.CRC_data_format(crc, data, swap, row_col_idx, to_log=False)

    # use writebytes2 with np byte array if efficiency matters
    # send data with xfer2 command, which returns the response from controller 
    spi_return = spi.xfer2(SPI_cmd)
    print(f"> spi return value from DLPC1438: {spi_return}")



# see section on crcmod in 4.2
def make_CRC16_function():
    crc16_func = mkCrcFun(0x18005, initCrc=0xFFFF, xorOut=0x0, rev=False)
    return crc16_func 

if __name__ == "__main__":
    generator = make_CRC16_function()
    print(bin(generator(b'1234567896666666wadwadwadawdwdawdawdwad6231231231231231231245454456456347456745674574574566666666666666666666666666666666666666666666')))  # 16 bit number
    print(bin(generator(b'1234567896666666wadwadwadawdwdawdawdwad62312312312312312312454544564563474567456745766666666666666666666666666666666666666666')))  # 16 bit number
    print(bin(generator(b'123454563474567456745766666666666666666666666666666666666666666')))  # 16 bit number

