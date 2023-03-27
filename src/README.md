## Running the DLP pico demo

To run the demo code on your pico, copy over the `build/DLP_pico.uf2` over to the pico.

### Building the demo code

I don't know a thing about compiling C stuff but given that this code is based on the nice [example code by Hunter Adams](https://vanhunteradams.com/Pico/VGA/VGA.html) most of the cmake stuff is already set up. To be able to build the project you may need to install the pico sdk. You can read more about this [in the official getting started guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

To build the project, head into the `build` directory and run

```
cmake ..
```

and follow that up with 

```
make
```

Which should finally create the file `DLP_pico.uf2` that you can then use to program the pico. The `DLP_pico.uf2` file is also included in the repository to avoid having to go through the build process itself if you don't need to modify anything.