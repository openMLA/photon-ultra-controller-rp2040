import tifffile as tif
import numpy as np
import argparse

# code assumes the input image is an 8-bit grayscale tiff file with only two values: 0 and 255.
# and dimensions 720x1280
# you can achieve this in imageJ with the following operations from an image:
# * IMAGE > COLOR > RGB TO LUMINANCE
# * PROCESS > BINARY > MAKE BINARY 

def format_into_binary(row):
    # e.g. [false, false, true, true, false, true, false,true] -> 0b10101100
    x = sum(map(lambda x: x[1] << x[0], enumerate(row)))
    return bin(x)

def format_into_hex(row):
    # e.g. [false, false, true, true, false, true, false,true] -> 0b10101100
    x = sum(map(lambda x: x[1] << x[0], enumerate(row)))
    return hex(x)

def make_image_byte_array_string(filepath, do_binary=False, skipfile=False):
    with tif.TiffFile(filepath) as image: # "open_mla_logo_sample_image.tif"
        pixelarray = image.asarray()
        assert pixelarray.shape == (720, 1280)  #  check that the image is the correct size
        #assert all(np.unique(pixelarray) == [0, 255])  # assert that we have a binary image (but still 8-bit)

        pixelarray_boolean = (pixelarray > 0)  # we only have two values, lets map them to 0 and 1 (true false)
        
        linear_array = pixelarray_boolean.reshape((720*1280, 1))  # make into linear array row by row
        pixel_array_per_8 = linear_array.reshape((int(720*1280/8), 8))  # split into blocks of 8

        # now make that into a c array string.
        if do_binary:
            byte_string_list = np.apply_along_axis(format_into_binary, axis=1, arr=pixel_array_per_8)
        else:
            byte_string_list = np.apply_along_axis(format_into_hex, axis=1, arr=pixel_array_per_8)

        # and now make that list of hex or binary into something that we can copy paste.
        byte_string_array = ', '.join(byte_string_list)

        with open(f'{filepath}.txt', 'w') as f:
            f.write(byte_string_array)

        print(byte_string_list)
        #print(byte_string_array)
    
    
if __name__ == "__main__":
    # example: python grayscale_tiff_to_bytes.py --file "open_mla_logo_sample_image.tif"

    parser = argparse.ArgumentParser(description="Turn 8-bit 720x1280 tif into pasteable array string")
    parser.add_argument("--file", type=str, help="filename of image. Must be TIF.")
    parser.add_argument("--binary", action='store_true', help="use binary format for byte instead of hex")
    parser.add_argument("--skipfile", action='store_true', help="do not create a textfile with pasteable string. Only print to terminal.")

    args = parser.parse_args()
    make_image_byte_array_string(args.file, args.binary, args.skipfile)