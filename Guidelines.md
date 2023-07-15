## Photon Ultra board & DLPC1438 guidelines

> This section will outline some of the guidelines, and excerpts from the datasheets that are relevant to both specifically the Photon Ultra board and the DLPC1438 in general.

Suggested documents to familiarise yourself with

1. [The DLPC1438 datasheet](https://www.ti.com/lit/ds/symlink/dlpc1438.pdf?ts=1689232218137)
2. [The DLPC1438 programming guide (describes the I2C stuff)](https://www.ti.com/lit/ug/dlpu111/dlpu111.pdf?ts=1689282051632&ref_url=https%253A%252F%252Fwww.google.com%252F)
3. [The raspberry pi pico c SDK](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)

### Startup sequence

In the datasheet for the  `DLPC1438` there are multiple sections containing info about the startup sequence:

1. section 7.3.3
2. section 9.2 & 9.3

The main gist of it is that to power on you must both:

1. Provide power to the photon ultra board
2. Keep GPIO_08 (on DLPC1438) (a.k.a. PROJ_ON) high

The controller will then run through a reset/bootup cycle and eventually let you know it is done by pulling HOST_IRQ low.

After HOST_IRQ is low you can start the i2c bus and start talking to the DLPC1438.

> 🖼 Figure 9-1 of the datasheet nicely summarises this.

### I2C notes

For whatever reason (probably a I2C address conflict) Anycubic/eViewTek has changed the default i2c adress of the DLPC1438 on the Ultra's board from `0x36` to `0x1B`. 

> I assume this will be the same for all the boards, but if not you can connect power and the FPC to the ultra's mainboard and only the i2c line to the pico board and then after power on of the Ultra mainboard run an i2c scan script to find out the address of the DLPC1438.



