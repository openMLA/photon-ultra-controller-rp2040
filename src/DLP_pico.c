/**
 * Nemo Andrea (nemoandrea@outlook.com)
 * Based on demo code by  Hunter Adams (vha3@cornell.edu)
 * 
 * Texas Instruments DLPC1438 video signal generator
 * Uses PIO-assembly of RP2040
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 19 ---> Hsync
 *  - GPIO 18 ---> DATAEN_CMD (data valid) 
 *  - GPIO 17 ---> Vsync
 *  - GPIO 16 ---> PCLK (pixel clock)
 *  - GPIO 8:15 ---> Pdata[0:8] (pixel data 8bit grayscale)
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, 2 and 3 on PIO instance 0
 *  - DMA channels 0 and 1
 *  - 230.4 kBytes of RAM (for pixel color data)
 *
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
// Our assembled programs:
// Each gets the name <pio_filename.pio.h>
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "pxl.pio.h"
#include "pxl_clk.pio.h"


// VGA timing constants
#define H_ACTIVE   1300    // (active + frontporch - 1) - one cycle delay for mov //TODO Tune
#define V_ACTIVE   720    // (active - 1) //TODO Tune
#define DATAEM_CMD_ACTIVE 320    // horizontal_pixels/pixels_per_byte = 1280/4 = 320


// Length of the pixel array, and number of DMA transfers
#define TXCOUNT 230400 // Total number of chars/8bit numbers we need. (for DLP300/301 in normal mode)
                       // We run 1280x720 pixels and 2 bits per pixel so we need 1280*720*(2/8) 

// Pixel grayscale array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)
unsigned char DLP_data_array[TXCOUNT];
char * address_pointer = &DLP_data_array[0] ;

// Give the I/O pins that we're using some names that make sense
// the pins match the layout in the PCB and the pico board pinout
#define DATAEN_CMD   18
#define HSYNC     19
#define VSYNC     17
#define PXL_CLK   16
#define BASE_PXL_PIN   8  // first pin (of 8) contiguous pixel bits

// A function for drawing a pixel with a specified grayscale value.
// Note that because information is passed to the PIO state machines through
// a DMA channel, we only need to modify the contents of the array and the
// pixels will be automatically updated on the screen.

// NOTE: for now we constrain brightness to be {0,1,2,3} (2 bits only)
void drawPixel(int x, int y, uint8_t brightness) {
    // Which array index is it?
    int pixel = ((640 * y) + x) ;

    switch (pixel % 4)
    {
        case 0:
            DLP_data_array[pixel>>1] |= brightness << 6 ;
            break;
        case 1:
            DLP_data_array[pixel>>1] |= brightness << 4;
                break;
        case 2:
            DLP_data_array[pixel>>1] |= brightness << 2;
                break;
        default:
            DLP_data_array[pixel>>1] |= brightness;
                break;
    }
}

int main() {

    // Initialize stdio
    stdio_init_all();

    // Choose which PIO instance to use (there are two instances, each with 4 state machines)
    PIO pio = pio0;

    // Our assembled program needs to be loaded into this PIO's instruction
    // memory. This SDK function will find a location (offset) in the
    // instruction memory where there is enough space for our program. We need
    // to remember these locations!
    //
    // We only have 32 instructions to spend! If the PIO programs contain more than
    // 32 instructions, then an error message will get thrown at these lines of code.
    //
    // The program name comes from the .program part of the pio file
    // and is of the form <program name_program>
    uint hsync_offset = pio_add_program(pio, &hsync_program);
    uint vsync_offset = pio_add_program(pio, &vsync_program);
    uint pxl_offset = pio_add_program(pio, &pxl_program);
    uint clk_offset = pio_add_program(pio, &pxl_clk_program);

    // Manually select a few state machines from pio instance pio0.
    uint hsync_sm = 0;
    uint vsync_sm = 1;
    uint pxl_sm = 2;
    uint clk_sm = 3;

    // Call the initialization functions that are defined within each PIO file.
    // Why not create these programs here? By putting the initialization function in
    // the pio file, then all information about how to use/setup that state machine
    // is consolidated in one place. Here in the C, we then just import and use it.
    hsync_program_init(pio, hsync_sm, hsync_offset, HSYNC);
    vsync_program_init(pio, vsync_sm, vsync_offset, VSYNC);
    pxl_program_init(pio, pxl_sm, pxl_offset, BASE_PXL_PIN, DATAEN_CMD);
    pxl_clk_program_init(pio, clk_sm, clk_offset, PXL_CLK);


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ===========================-== DMA Data Channels =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    // DMA channels - 0 sends color data, 1 reconfigures and restarts 0
    int pxl_chan_0 = 0;
    int pxl_chan_1 = 1;

    // Channel Zero (sends color data to PIO VGA machine)
    dma_channel_config c0 = dma_channel_get_default_config(pxl_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              // 8-bit txfers
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing
    channel_config_set_dreq(&c0, DREQ_PIO0_TX2) ;                        // DREQ_PIO0_TX2 pacing (FIFO)
    channel_config_set_chain_to(&c0, pxl_chan_1);                        // chain to other channel

    dma_channel_configure(
        pxl_chan_0,                 // Channel to be configured
        &c0,                        // The configuration we just created
        &pio->txf[pxl_sm],          // write address (RGB PIO TX FIFO)
        &DLP_data_array,            // The initial read address (pixel color array)
        TXCOUNT,                    // Number of transfers; in this case each is 1 byte.
        false                       // Don't start immediately.
    );

    // Channel One (reconfigures the first channel)
    dma_channel_config c1 = dma_channel_get_default_config(pxl_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, pxl_chan_0);                         // chain to other channel

    dma_channel_configure(
        pxl_chan_1,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[pxl_chan_0].read_addr,  // Write address (channel 0 read address)
        &address_pointer,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    // Initialize PIO state machine counters. This passes the information to the state machines
    // that they retrieve in the first 'pull' instructions, before the .wrap_target directive
    // in the assembly. Each uses these values to initialize some counting registers.
    pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
    pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
    pio_sm_put_blocking(pio, pxl_sm, DATAEM_CMD_ACTIVE);
    pio_sm_put_blocking(pio, clk_sm, DATAEM_CMD_ACTIVE);


    // Start the two pio machine IN SYNC
    // Note that the RGB state machine is running at full speed,
    // so synchronization doesn't matter for that one. But, we'll
    // start them all simultaneously anyway.
    pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << pxl_sm) | (1u << clk_sm)));

    // Start DMA channel 0. Once started, the contents of the pixel color array
    // will be continously DMA's to the PIO machines that are driving the screen.
    // To change the contents of the screen, we need only change the contents
    // of that array.
    dma_start_channel_mask((1u << pxl_chan_0)) ;


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ===================================== DEMO SCREENS =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // simple checkerboard-like pattenr with grayscale blocks. 
    // due to memory constraints we only use 2 bits for each pixel

    uint64_t begin_time; 
    uint64_t curr_time; 
    uint64_t frametime = 1000000;  // time between frame transitions (in us) 
    uint32_t framenum = 0;

    begin_time = time_us_64() ;

    // grayscale block size
    int xsize = 160;
    int ysize = 80;
    int xprog = 0;  // just a counter
    int yprog = 0;  // just a counter


    while (true) {     
        int x = 0 ; 
        int y = 0 ; 

        for (x=0; x<1280; x++) {   
            if (xprog == xsize) {
                framenum = (framenum + 1) % 4;  // new grayscale value for block
                xprog=0;
            } 
            xprog++;
            for (y=0; y<720; y++) {         
                if (yprog == ysize) {
                    framenum = (framenum + 1) % 4;  // new grayscale value for block
                    yprog=0;
                } 
                yprog++;
                drawPixel(x, y, framenum) ;  // actually pack the pixel into DLP_data_array
            }
        }

        // every frametime we shift the pattern by one grayscale value.
        // possible demonstration of framerate or something
        // curr_time = time_us_64();
        // if  (curr_time - begin_time > frametime) {
        //     framenum = (framenum+1) % 4; 
        //     begin_time = curr_time;
        // }
    }
}
