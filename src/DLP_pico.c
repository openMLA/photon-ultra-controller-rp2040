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
#include <assert.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// Our assembled programs:
// Each gets the name <pio_filename.pio.h>
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "pxl.pio.h"
#include "pxl_clk.pio.h"

const int PROJ_ON_GPIO = 20;  // physical pin nr 26
const int HOST_IRQ_GPIO = 21;  // physical pin nr 27  

const uint LED_PIN_G = 26;  // GPIO 26
const uint LED_PIN_R = 22;  // GPIO 26
const uint SDA_PIN = 6; // GPIO 6
const uint SCL_PIN = 7; // GPIO 7

static uint16_t exposure_time = 200;

enum ProjectorMode {
    TESTPATTERN,
    SPLASHSCREEN,
    EXTERNALPRINT,
    STANDBY
};

enum LightState {
    OFF,
    ON
};

//////// i2c config (max freq 100 kHz)

// we define the i2c address of the DLPC1438. For whatever reason (I guess conflict with other
// i2c devices on the mainboard) eViewTek have changed the i2c address to 0x1B, from the 
// factory setting of 36h. I assume this will be the same for all boards. 
const int DLPC_addr = 0x1B;  

void initialise_DLPC() {  
    gpio_init(PROJ_ON_GPIO);
    gpio_set_dir(PROJ_ON_GPIO, GPIO_OUT);
    gpio_init(HOST_IRQ_GPIO);
    gpio_set_dir(HOST_IRQ_GPIO, GPIO_IN);
    bool current_IRQ_state = 0;

    printf(">> Attempting to initialise the DLPC1438...\n\n");

    // HOST_IRQ will be pulled HIGH by the pullup on eViewTek board
    // but in theory it could also be LOW at this time
    current_IRQ_state = gpio_get(HOST_IRQ_GPIO);  
    printf("HOST_IRQ at startup is: %d\n", current_IRQ_state);
    

    // to signal to the DLPC1438 that we want to start up, we must set PROJ_ON high
    printf("Setting PROJ_ON (DLPC1436:GPIO8) HIGH...\n");
    gpio_put(PROJ_ON_GPIO, 1);  // this will let the DLPC1438 know we want to go, go go

    // now we must let the DLPC1438 run through its initialisation cycle
    // it will let us know it is done by pulling HOST_IRQ low (~500ms)
    printf("Waiting for HOST_IRQ LOW...\n");

    bool last_IRQ_state = 0;
    bool transition = 0;
    int count = 0;
    int sleeptime = 50;  // in ms 

    // TODO: the HOST_IRQ signal is 1.8V when high; hard to interpret on the pico; right
    // on the threshold of detectable voltage. Code below assumes we can reliably distinguish
    // between 0 and 1.8V. Alternative: wait for 1 s and assume it is ready.

    // while (!transition) {
    //     sleep_ms(sleeptime);
    //     current_IRQ_state = gpio_get(HOST_IRQ_GPIO);  
    //     //printf("HOST IRQ IS: %d\n", current_IRQ_state);
    //     if ((last_IRQ_state == 1) && (current_IRQ_state == 0)) { transition = 1; }
    //     last_IRQ_state = current_IRQ_state;
    //     count += 1;
    // }  // typically this takes ~650ms

    sleep_ms(1000);  // makeshift solution if the HOST_IRQ based approach does not work.

    printf("HOST_IRQ has been pulled low by DLPC1438, %dms after PROJ_ON signal\n", count*sleeptime);
    
    // now that HOST_IRQ is low we can start i2c communication. Lets give it 100ms to be safe
    sleep_ms(100);
    printf("DLPC1438 is ready for I2C communication!");
}

// set up i2c hardware on pico
void configure_i2c() {
    printf("\n>> setting up I2C...\n\n");
    
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C); 
    //gpio_pull_up(SDA_PIN);  // Don't need these since we have hardware pullup on the PCB
    //gpio_pull_up(SCL_PIN);  // Don't need these since we have hardware pullup on the PCB
    i2c_init(i2c1, 40 * 1000);  // DLPC1438 supports up to 100KHz; we use i2c HW block 0
    printf("i2c should be ready!\n");
}

/// TEMPORARY TODO: remove
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void scan_i2c() {
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata[3];
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c1, addr, rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");
}

// can we actually see AND talk to DLPC1438 over i2c?
void check_i2c_communication() {
    // Can we see the DLPC1438?
    scan_i2c();
    sleep_ms(500);

    // Can we talk to the DLPC1438?
    int ret;
    uint8_t rxdata;
    ret = i2c_read_blocking(i2c1, DLPC_addr, &rxdata, 1, false); 
    printf(ret < 0 ? "Could not connect to DLPC1438 over i2c\n" : "Found DLPC1438 on i2c\n");
    if (ret >= 0) { 
        gpio_put(LED_PIN_R, 1);  // turn on LED so we know we are connected.
    }
}

void i2c_write(uint8_t addr, uint8_t *data, int length) {
    // note that length is here the length of data (so excluding the addr)
    uint8_t send_data[length+1];  // but we we must combine [addr, data...] to send over

    send_data[0] = addr;

    int i = 0;
    for (i = 0; i < length; i++) {
        send_data[i+1] = data[i];
    }

    printf("(i2c-send) raw data: ");  // just for my sanity

    for (int i = 0; i < sizeof send_data / sizeof send_data[0]; i++) {
        printf("%x ", send_data[i]);
    }
    printf("\n");

    i2c_write_blocking(i2c1, DLPC_addr, send_data, length+1, false);
}

// read I2C data back
// TODO: there is a strange issue where the first byte is a mystery byte
// and it is not clear to me atm why this is returned, or what it represents.
// for now I just ask for data at length+1 and print  only data[1:end]
void i2c_read(uint8_t addr, uint8_t length, char* message) {
    uint8_t data_in[length+1];

    i2c_write_blocking(i2c1, DLPC_addr, &addr, 1, true);
    i2c_read_blocking(i2c1, DLPC_addr, data_in, length+1, false);

    printf(message);
    for (int i = 0; i < length; i++) {
        printf("%x \n", data_in[i]);
    }
}

void switch_projector_mode(enum ProjectorMode mode) {
    
    uint8_t addr = 0x05;  // i2c addr for mode switch
    uint8_t mode_value;
    switch(mode) {
        case(0):
            printf("> Switching to Test Pattern mode (0x01)\n");
            mode_value = 0x01;
            break;
        case(1):
            printf("> Switching to Splash-Screen mode (0x02)\n");
            mode_value = 0x02;    
            break;
        case(2):
            printf("> Switching to External Print mode (0x06)\n");
            mode_value = 0x06;
            break;
        case(3):
            printf("> Switching to Standby mode (0xFF)\n");
            mode_value = 0xFF;
            break;
    }

    // checking initial state:
    i2c_read(0x06, 1, "Mode before switching: ");

    uint8_t data[] = {mode_value};
    i2c_write(addr, data, 1);

    // temporary: check that we switched ok  --> works fine
    i2c_read(0x06, 1, "Mode after switching: ");
}

void curtain_flood_exposure(int duration, int count) {
    // simple utility function that will do a flood exposure (all pixels on max)
    // for a certain <duration> (in ms) and repeated a <count> number of times.
    // useful for testing communication.

    // note that projector cannot be in standby mode for this to work. Try Testpattern.

    i2c_read(0x17, 1, "Base curtain settings: "); //TODO: remove
    uint8_t curtain[1];

    for (int i = 0; i < count; i++) {
        // flood exposure ON
        curtain[0] = 0b00001111;
        i2c_write(0x16, curtain, 1);
        sleep_ms(duration);
        // flood exposure OFF
        curtain[0] = 0b00000000;
        i2c_write(0x16, curtain, 1);
        sleep_ms(duration);
    }
}

void configure_external_print(){  // see section 3.3.6 (and 3.3.1) of programming guide
    // in this step we set both the transfer fuction gamma and select which LED to use
    printf("\n>> Configuring External Print mode...\n\n");

    // temporary: checking initial state:
    i2c_read(0xA9, 2, "intial gamma/led config settings: \n");
    sleep_ms(100);

    // write the new values to register (actually configure the DLPC1438)
    int gamma = 0x00;  // we want the linear kind (also used by Anycubic)
    // Select which LED to use. b(7:2) are reserved. The final 3 bits are toggles for LED 3,2,1 
    int led_select = 0b00000100;  // one LED at a time. Anyubic board says we need LED3.
    uint8_t data[] = {gamma, led_select};  
    i2c_write(0xA8, data, 2);

    i2c_read(0xA9, 2, "new gamma/led config settings: \n");
    sleep_ms(100);
}

// programming guide section 3.3.8 & 9
// exposure values taken from Anycubic Board's I2C commands during test patterns
void switch_light_state(enum LightState light_on) { 
    uint8_t print_control, dark_frames_LSB, dark_frames_MSB, exp_frames_LSB, exp_frames_MSB;

    if (light_on) {
        printf("[!UV!] Switching Projector Light ON\n");
        // "External Print Control" byte; b(7:1) reserved; 0 is START, 1 is STOP.
        print_control   = 0b00000000;  // 1 for light off, 0 for light on
        dark_frames_LSB = 0x03; 
        dark_frames_MSB = 0x00; 
        exp_frames_LSB  = 0xFF;  // if both exp_frames are 0xFF the LED will stay on indefinitely
        exp_frames_MSB  = 0xFF;  // and turning off is then done by sending this command with 
                                        // print_control being 0b00000001 
    } else {
        printf("Switching Projector Light OFF\n");
        print_control   = 0b00000001;  // 1 for light off, 0 for light on
        dark_frames_LSB = 0x00; 
        dark_frames_MSB = 0x00; 
        exp_frames_LSB  = 0x00;  
        exp_frames_MSB  = 0x00;  
    }

    uint8_t data[] = {print_control, dark_frames_LSB, dark_frames_MSB, exp_frames_LSB, exp_frames_MSB}; 
    sleep_ms(30);  // give it a little moment 
    i2c_write(0xC1, data, 5);

    // i2c_read the 0xC2 register never seems to actually return logical numbers
    // I assume there is something wrong on the firmware side. The effect of the write command
    // is okay though.
    // i2c_read(0xC2, 5, "New Dark and Exposed frame settings: \n");
}

void set_illumination_PWM(unsigned short PWM_value) {
    // Set the PWM for the LED source. Note that the resolution is only 10 bit (so 6 unused bits)
    // We only consider LED 3 (byte 5,6) since that is the LED source we use

    // The anycubic board uses 0xB4 (quite far from the max) during setup.

    uint8_t LSByte = PWM_value & 0xFF;
    uint8_t MSByte = (PWM_value >> 8) & 0xFF;

    uint8_t data[] = {0, 0, 0, 0, LSByte, MSByte};
    i2c_write(0x54, data, 6);
    i2c_read(0x55, 6, "Set LED PWM settings: \n");
}

void set_image_orientation(bool flip_short_axis, bool flip_long_axis) {
    uint8_t shifted_sa = (flip_short_axis << 2);
    uint8_t shifted_la = (flip_long_axis << 1); 
    uint8_t orientation_setting = (shifted_sa | shifted_la);
    uint8_t data[1] = {orientation_setting};

    i2c_write(0x14, data, 1);
    i2c_read(0x15, 1, "Set image orientation settings: \n");
}

void configure_test_pattern_settings(uint8_t pattern_idx) {

    i2c_read(0x0C, 6, "old test pattern settings: \n");

    uint8_t pattern_select = 0b10000111;  // b(6:4) reserved
    // parameters (effect depends on selected pattern)
    // note that if a parameter is not used by pattern then the bytes will not be set
    // and reading back the value after setting will yield 0x00
    uint8_t param_1 = 0; 
    uint8_t param_2 = 240;
    uint8_t param_3 = 0;  
    uint8_t param_4 = 240;

    // TODO: decipher the meaning of byte 2; its not clear if it is actually reserved...
    // will set to 0x00 for now.

    uint8_t test_pattern_settings[6] = {pattern_select, 0x00, param_1, param_2, param_3, param_4};

    i2c_write(0x0B, test_pattern_settings, 6);

    i2c_read(0x0C, 6, "new test pattern settings: \n");
}

void intialise_DLP_test_pattern() {
    // for some reason the intial pattern seems to be a checkerboard,
    // but the i2c registers all read 0. Might be a special case...
    switch_projector_mode(TESTPATTERN);
    sleep_ms(200);

    // first we must configure the test pattern mode BEFORE activating it
    configure_test_pattern_settings(0);

    // now we can switch to test pattern mode
    printf(">>> MODE AT STARTUP\n");
    sleep_ms(200);

    switch_projector_mode(TESTPATTERN);
}


// void expose_new_frame(uint16_t exposure_time) {
//     // send image data to video buffer (via PIO)
//     // Set External Print Layer Control (starts exposure)
// }


//////// PIO stuff


// VGA timing constants
#define H_ACTIVE   1300    // (active + frontporch - 1) - one cycle delay for mov //TODO Tune
#define V_ACTIVE   720    // (active - 1) //TODO Tune
#define DATAEM_CMD_ACTIVE 320    // horizontal_pixels/pixels_per_byte = 1280/4 = 320


// Length of the pixel array, and number of DMA transfers
#define TXCOUNT 115200 // Total number of chars/8bit numbers we need. (for DLP300/301 in normal mode)
                       // We run 1280x720 pixels and 1 bit per pixel so we need 1280*720*(1/8) 

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

// NOTE: for now we constrain brightness to be 0 or 1 (1-bit)
void drawPixel(int x, int y, enum LightState lit) {
    
    // we first find the linear index of the pixel. This is just a unique number for each 
    // pixel that runs from 0 to x_range*y_range (1280*720)
    long pixel_idx = ((1280 * y) + x) ;

    // our data array contains fewer element than the range of our pixel index
    // since we store the 1-bit info for 8 pixels in a single byte of that data array
    // we need to to map our pixel index into array address. Note that 8 pixel indices should
    // map to the same address.
    
    // so we do 2 things: first determine what pixel are we looking at (at our set of 8?)
    // that one is not so hard: simply pixel_idx % 8.

    // To efficiently find the array index, we can simply divide (and round) the pixel_index by 8. 
    // A triple right bit shift should do the trick (pixel_idx >> 3)

    // We then bit shift the pixel brightness (lit; 0 or 1) to the appropriate location in the byte
    // by using lit << (7 - pixel_idx % 8). This means the lower pixel_index will be "to the left" in
    // the byte.

    DLP_data_array[pixel_idx>>3] |= lit << (7 - (pixel_idx % 8));

    // switch (pixel_idx % 8)
    // {
    //     case 0:
    //         DLP_data_array[pixel_idx>>3] |= lit << 7;
    //         break;
    //     case 1:
    //         DLP_data_array[pixel_idx>>3] |= lit << 6;
    //         break;
    //     case 2:
    //         DLP_data_array[pixel_idx>>3] |= lit << 5;
    //         break;
    //     case 3:
    //         DLP_data_array[pixel_idx>>3] |= lit << 4;
    //         break;
    //     case 4:
    //         DLP_data_array[pixel_idx>>3] |= lit << 3;
    //         break;
    //     case 5:
    //         DLP_data_array[pixel_idx>>3] |= lit << 2;
    //         break;
    //     case 6:
    //         DLP_data_array[pixel_idx>>3] |= lit << 1;
    //         break;
    //     default:
    //         DLP_data_array[pixel_idx>>3] |= lit;
    //         break;
    // }
}

void checkerboard_PIO() {
    // simple checkerboard-like pattenr with grayscale blocks. 
    // due to memory constraints we only use 2 bits for each pixel

    printf("{{ making checkerboard }}\n");

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
 

    while((time_us_64() - begin_time) < 5000000) {  
        
        int x = 0 ; 
        int y = 0 ; 
        int yoink = 0;
        int drawstate = 1;

        for (x=0; x<1280; x++) { 
            //printf("... line x=%d.\n", x);
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
                
                drawPixel(x, y, drawstate) ;  // actually pack the pixel into DLP_data_array //TODO make dyanmic
                if (yoink > 10) {
                    yoink = 0;
                    drawstate = drawstate ^ 1; // should flip 0->1 and 1->0
                }
                yoink++;
            }
        }
    }

    printf("Stopped sending checkerboard...\n");
}


int main() {
    // Initialize stdio
    stdio_init_all();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // ===========================-== PIO Stuff ====================================================
    ////////////////////////////////////////////////////////////////////////////////////////////////

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
    // Note that the PXL state machine is running at full speed,
    // so synchronization doesn't matter for that one. But, we'll
    // start them all simultaneously anyway.
    pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << pxl_sm) | (1u << clk_sm)));

    // Start DMA channel 0. Once started, the contents of the pixel data array
    // will be continously DMA's to the PIO machines that are driving the screen.
    // To change the contents of the screen, we need only change the contents
    // of that array.
    dma_start_channel_mask((1u << pxl_chan_0)) ;


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // ===================================== DLPC1438 stuff ========================================
    ////////////////////////////////////////////////////////////////////////////////////////////////
    
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    for (int i = 0; i < 5; i++) {
        printf("Blinking!\r\n");
        gpio_put(LED_PIN_G, 0);
        sleep_ms(250);
        gpio_put(LED_PIN_G, 1);
        sleep_ms(1000);
    }
    
    // initialise_DLPC();

    configure_i2c();  // set up i2c hardware
    check_i2c_communication();  // check that we can talk to DLPC1438

    // optional test commands
    intialise_DLP_test_pattern();
    sleep_ms(1000);  // just wait and check if test pattern appears
    //curtain_flood_exposure(100, 3);  // flash at max brightness a few times (for testing)
    switch_projector_mode(STANDBY); // stop illumination and be in long term stable mode
    sleep_ms(800);

    // prepare printer for external print mode

    configure_external_print();
    switch_projector_mode(EXTERNALPRINT);
    set_illumination_PWM(0xB4);
    set_image_orientation(false, false);

    ///////// EXTERNAL PRINT MODE SECTION
    // programming guide section 3.3.1 ("3D Print Procedure Without FPGA Front-End")


    // -- setup phase -----------
    printf("\n>> EXTERNAL PRINT SETUP <<\n\n");
    configure_external_print(); 
    switch_projector_mode(EXTERNALPRINT); 
    sleep_ms(200); // For now just wait a bit. Ideally: detect SYSTEM_READY (GPIO_06; not connected)
    set_illumination_PWM(0xB4);  
    set_image_orientation(false, false);

    printf("Checking light state...");
    switch_light_state(ON);   // turn on the projector and show whatever is in image buffer


    // -- loop phase ---------
    printf("\n>> EXTERNAL PRINT LOOP <<\n\n");
    checkerboard_PIO(); // send video data [A]
    // // TODO: remove the sanity check below
    printf("%x:%x:%x:%x:%x\n", DLP_data_array[0], DLP_data_array[12], DLP_data_array[6000], DLP_data_array[100000], DLP_data_array[72]);
    // switch_light_state(ON);   // turn on the projector and show whatever is in image buffer
    // // -> go to line above loop back to [A]
    

    sleep_ms(500); // TODO: remove
    switch_light_state(OFF);
    sleep_ms(1000);
    
    switch_projector_mode(STANDBY); // stop illumination and be in long term stable mode
    
    printf("\ndone.");
}
