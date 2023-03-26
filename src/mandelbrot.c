/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 * Mandelbrot set calculation and visualization
 * Uses PIO-assembly VGA driver
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 330 ohm resistor ---> VGA Red
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - RP2040 GND ---> VGA GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels 0 and 1
 *  - 153.6 kBytes of RAM (for pixel color data)
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
#include "rgb.pio.h"


// VGA timing constants
#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
#define V_ACTIVE   479    // (active - 1)
#define RGB_ACTIVE 319    // (horizontal active)/2 - 1
// #define RGB_ACTIVE 639 // change to this if 1 pixel/byte

// Length of the pixel array, and number of DMA transfers
#define TXCOUNT 153600 // Total pixels/2 (since we have 2 pixels per byte)

// Pixel color array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)
unsigned char vga_data_array[TXCOUNT];
char * address_pointer = &vga_data_array[0] ;

// Give the I/O pins that we're using some names that make sense
#define HSYNC     13
#define VSYNC     11
#define RED_PIN   18
#define GREEN_PIN 19
#define BLUE_PIN  20

// We can only produce 8 colors, so let's give them readable names
#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7


// A function for drawing a pixel with a specified color.
// Note that because information is passed to the PIO state machines through
// a DMA channel, we only need to modify the contents of the array and the
// pixels will be automatically updated on the screen.
void drawPixel(int x, int y, char color) {
    // Range checks
    if (x > 639) x = 639 ;
    if (x < 0) x = 0 ;
    if (y < 0) y = 0 ;
    if (y > 479) y = 479 ;

    // Which pixel is it?
    int pixel = ((640 * y) + x) ;

    // Is this pixel stored in the first 3 bits
    // of the vga data array index, or the second
    // 3 bits? Check, then mask.
    if (pixel & 1) {
        vga_data_array[pixel>>1] |= (color << 3) ;
    }
    else {
        vga_data_array[pixel>>1] |= (color) ;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////// Stuff for Mandelbrot ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Fixed point data type
typedef signed int fix28 ;
#define multfix28(a,b) ((fix28)(((( signed long long)(a))*(( signed long long)(b)))>>28)) 
#define float2fix28(a) ((fix28)((a)*268435456.0f)) // 2^28
#define fix2float28(a) ((float)(a)/268435456.0f) 
#define int2fix28(a) ((a)<<28)
// the fixed point value 4
#define FOURfix28 0x40000000 
#define SIXTEENTHfix28 0x01000000
#define ONEfix28 0x10000000

// Maximum number of iterations
#define max_count 1000

// Mandelbrot values
fix28 Zre, Zim, Cre, Cim ;
fix28 Zre_sq, Zim_sq ;

int i, j, count, total_count ;

fix28 x[640] ;
fix28 y[480] ;

////////////////////////////////////////////////////////////////////////////////////////////////////

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
    uint rgb_offset = pio_add_program(pio, &rgb_program);

    // Manually select a few state machines from pio instance pio0.
    uint hsync_sm = 0;
    uint vsync_sm = 1;
    uint rgb_sm = 2;

    // Call the initialization functions that are defined within each PIO file.
    // Why not create these programs here? By putting the initialization function in
    // the pio file, then all information about how to use/setup that state machine
    // is consolidated in one place. Here in the C, we then just import and use it.
    hsync_program_init(pio, hsync_sm, hsync_offset, HSYNC);
    vsync_program_init(pio, vsync_sm, vsync_offset, VSYNC);
    rgb_program_init(pio, rgb_sm, rgb_offset, RED_PIN);


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ===========================-== DMA Data Channels =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    // DMA channels - 0 sends color data, 1 reconfigures and restarts 0
    int rgb_chan_0 = 0;
    int rgb_chan_1 = 1;

    // Channel Zero (sends color data to PIO VGA machine)
    dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              // 8-bit txfers
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing
    channel_config_set_dreq(&c0, DREQ_PIO0_TX2) ;                        // DREQ_PIO0_TX2 pacing (FIFO)
    channel_config_set_chain_to(&c0, rgb_chan_1);                        // chain to other channel

    dma_channel_configure(
        rgb_chan_0,                 // Channel to be configured
        &c0,                        // The configuration we just created
        &pio->txf[rgb_sm],          // write address (RGB PIO TX FIFO)
        &vga_data_array,            // The initial read address (pixel color array)
        TXCOUNT,                    // Number of transfers; in this case each is 1 byte.
        false                       // Don't start immediately.
    );

    // Channel One (reconfigures the first channel)
    dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, rgb_chan_0);                         // chain to other channel

    dma_channel_configure(
        rgb_chan_1,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[rgb_chan_0].read_addr,  // Write address (channel 0 read address)
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
    pio_sm_put_blocking(pio, rgb_sm, RGB_ACTIVE);


    // Start the two pio machine IN SYNC
    // Note that the RGB state machine is running at full speed,
    // so synchronization doesn't matter for that one. But, we'll
    // start them all simultaneously anyway.
    pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm)));

    // Start DMA channel 0. Once started, the contents of the pixel color array
    // will be continously DMA's to the PIO machines that are driving the screen.
    // To change the contents of the screen, we need only change the contents
    // of that array.
    dma_start_channel_mask((1u << rgb_chan_0)) ;


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ===================================== Mandelbrot =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    uint64_t begin_time ;
    uint64_t end_time ;
    while (true) {

        // x values
        for (i=0; i<640; i++) {
            x[i] = float2fix28(-2.0f + 3.0f * (float)i/640.0f) ;
        }
        
        // y values
        for (j=0; j<480; j++) {
            y[j] = float2fix28( 1.0f - 2.0f * (float)j/480.0f) ;
        }

        total_count = 0 ;
        fix28 center = float2fix28(-0.25f);
        fix28 radius2 = float2fix28(0.25f);

        begin_time = time_us_64() ;

        for (i=0; i<640; i++) {
            
            for (j=0; j<480; j++) {

                Zre = Zre_sq = Zim = Zim_sq = 0 ;

                Cre = x[i] ;
                Cim = y[j] ;

                // detect secondary bulb
                // if ((multfix28(Cre+ONEfix28,Cre+ONEfix28)+multfix28(Cim,Cim))<SIXTEENTHfix28) {
                //     count=max_count;
                // }
                // // detect big circle
                // else if ((multfix28(Cre-center,Cre-center)+multfix28(Cim,Cim))<radius2) {
                //     count=max_count;
                // }
                // // otherwise get ready to iterate
                // else count = 0;
                count = 0 ;

                // Mandelbrot iteration
                while (count++ < max_count) {
                    Zim = (multfix28(Zre, Zim)<<1) + Cim ;
                    Zre = Zre_sq - Zim_sq + Cre ;
                    Zre_sq = multfix28(Zre, Zre) ;
                    Zim_sq = multfix28(Zim, Zim) ;

                    if ((Zre_sq + Zim_sq) >= FOURfix28) break ;
                }
                // Increment total count
                total_count += count ;

                // Draw the pixel
                if (count >= max_count) drawPixel(i, j, BLACK) ;
                else if (count >= (max_count>>1)) drawPixel(i, j, WHITE) ;
                else if (count >= (max_count>>2)) drawPixel(i, j, CYAN) ;
                else if (count >= (max_count>>3)) drawPixel(i, j, BLUE) ;
                else if (count >= (max_count>>4)) drawPixel(i, j, RED) ;
                else if (count >= (max_count>>5)) drawPixel(i, j, YELLOW) ;
                else if (count >= (max_count>>6)) drawPixel(i, j, MAGENTA) ;
                else drawPixel(i, j, RED) ;

            }
        }

        end_time = time_us_64() ;
        printf("Total time: %3.6f seconds \n", (float)(end_time - begin_time)*(1./1000000.)) ;
        printf("Total iterations: %d", total_count) ;
    }
}
