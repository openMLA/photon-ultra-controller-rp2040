## Running the DLP pico demo

To run the demo code on your pico, copy over the `build/DLP_pico.uf2` over to the pico.

### Building the demo code

I don't know a thing about compiling C stuff but given that this code is based on the nice [example code by Hunter Adams](https://vanhunteradams.com/Pico/VGA/VGA.html) most of the cmake stuff is already set up. To be able to build the project you may need to install the pico sdk. You can read more about this [in the official getting started guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

To build the project, head into the `build` directory and run

```
cmake .. -DPICO_BOARD=pico_w
```

and follow that up with 

```
make
```

Which should finally create the file `DLP_pico.uf2` that you can then use to program the pico. The `DLP_pico.uf2` file is also included in the repository to avoid having to go through the build process itself if you don't need to modify anything.

### Tools

In terms of hardware, I would suggest either using a [pico debug probe](https://www.raspberrypi.com/products/debug-probe/) to upload code, reset the pi and read the serial output, or a raspberry pi zero for the same application. You really don't want to have to unplug the board and press the reset button every time you want to try a new version of the code. Future versions of the board should make wiring SWD and UART readout easier.

I am not experienced enough with C to make a solid recommendation, but for the project I used Microsoft Visual Studio Code. With extensions:

* cmake
* serial monitor  (so we can get some information back from the pico)

