import tifffile as tif
import numpy as np
import argparse
import os

# code assumes the input image is an 8-bit grayscale tiff file with only two values: 0 and 255.
# and dimensions 720x1280
# you can achieve this in imageJ/Fiji (https://imagej.net/software/fiji/downloads) 
# with the following operations from an image:
# * IMAGE > COLOR > RGB TO LUMINANCE
# * PROCESS > BINARY > MAKE BINARY 

def format_into_binary(row):
    # e.g. [0, 1, 2, 2] -> 0b10100100
    x = sum(map(lambda x: x[1] << x[0]*2, enumerate(row)))
    return bin(x)


def format_into_hex(row):
    # e.g. [0, 3, 3, 2] -> 0b10111100
    x = sum(map(lambda x: x[1] << x[0]*2, enumerate(row)))
    return hex(x)


def make_image_byte_array_string(filepath, do_binary=False, skipimage=False):

    filename_base = os.path.splitext(os.path.basename(filepath))[0] 

    with tif.TiffFile(filepath) as image: # "open_mla_logo_sample_image.tif"
        pixelarray = image.asarray()
        assert pixelarray.shape == (720, 1280)  #  check that the image is the correct size

        # now we will split the pixel intensities into 4 regions
        # namely: [0, 63], [64, 127] [128, 191] [192,255] with values 0, 1, 2, 3 (2-bit)
        pixelarray_discrete = np.floor(pixelarray/256*4).astype(int)

        if not skipimage:  # save the discretised version of array to disk, so you can inspect it
            tif.imwrite(filename_base+"_discretised.tif", pixelarray_discrete*256/4, photometric='minisblack')

        linear_array = pixelarray_discrete.reshape((720*1280, 1))  # make into linear array row by row

        assert np.amax(linear_array) <= 3  # just some sanity checks 
        assert np.amin(linear_array) >= 0  # just some sanity checks 
                
        # split into bytes, which means blocks of 4 pixels, as each pixel has 2-bit values 
        pixel_array_per_4 = linear_array.reshape((int(720*1280/4), 4))   

        # now make that into a c array string.
        if do_binary:
            byte_string_list = [format_into_binary(row) for row in pixel_array_per_4]
        else:
            byte_string_list = [format_into_hex(row) for row in pixel_array_per_4]

        print(byte_string_list[:10])  # preview some bytes 
        assert len(byte_string_list) == 720*1280 / 4  # check that we have 4 pixels per byte

        # and now make that list of hex or binary into something that we can copy paste.
        byte_string_array = ','.join(byte_string_list)

        # and write the result to a text file that we can copy paste from.
        # should probably be a binary file but this is fine for now.
        with open(filename_base+'.txt', 'w') as f:
            f.write(byte_string_array)
    
    
if __name__ == "__main__":
    # example: python grayscale_tiff_to_bytes.py --file "open_mla_logo_sample_image.tif"

    parser = argparse.ArgumentParser(description="Turn 8-bit 720x1280 tif into pasteable array string")
    parser.add_argument("--file", type=str, help="filename of image. Must be grayscale TIF.")
    parser.add_argument("--binary", action='store_true', help="use binary format for byte instead of hex")
    parser.add_argument("--skipimage", action='store_true', help="do not write the discretised image to disk for inspection.")

    args = parser.parse_args()
    make_image_byte_array_string(args.file, args.binary, args.skipimage)