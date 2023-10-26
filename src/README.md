# DLP controller code

This code is developed to be run on a Linux system, but could probably be adapted to run on a microcontroller for basic functionality. Still, the recommended device is a Raspberry Pi zero 2, as that is the device used for development.

Python requirements (pip install):

* smbus2
* spidev
* crcmod

### High level overview

To talk to the eViewTek board that in turn talks to the DLP301s micromirror unit, we need to 

1. Configure the system for printing over I2C
2. Send over image data over SPI