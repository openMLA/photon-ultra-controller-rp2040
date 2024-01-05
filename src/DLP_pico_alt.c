/**
 * Nemo Andrea (nemoandrea@outlook.com)
 * 
 * Simple test code that sets the DLPC1438 to test mode
 * and allows you to cycle through some patterns with the board buttons
 * not a very useful script but it can be useful to:
 * 1. check that you have assembled the board correctly (for the most part; it doesnt check everything)
 * 2. check that the projector  is working before you have some patterns ready for the PIO units
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "DLP_pico.h"


// 3D print mode. For operating mode select (0x05). See section 3.1.1.1 of DLPC1438 prog.manual.
const int EXTERNAL_PRINT_MODE = 0x06;  

void configure_i2c() {
    printf("setting up i2c...\n");
    i2c_init(i2c1, 38 * 1000);  // DLPC1438 supports up to 100KHz; we use i2c HW block 0
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C); 
    gpio_pull_up(SDA_PIN);  // prob not needed since we have external pullup
    gpio_pull_up(SCL_PIN);  // prob not needed since we have external pullup
    printf("i2c should be ready!\n");
}

/// TEMPORARY TODO: remove
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void show_DLP_test_pattern(int8_t pattern_number) {
    printf("querying current device mode....\n");

    int ret;
    uint8_t rxdata;
    ret = i2c_read_blocking(i2c1, DLPC_addr, &rxdata, 1, false);

    printf(ret < 0 ? "did not find DLPC1438 :(\n" : "Hello DLPC1438\n");
    uint8_t mode[3];
    uint8_t modequery = 0x06;
    i2c_write_blocking(i2c1, DLPC_addr, &modequery, 1, true); // true to keep master control of bus
    i2c_read_blocking(i2c1, DLPC_addr, mode, 1, false);
    printf("We are in mode: %x\n", mode[0]);
    printf("We are in mode: %u\n", mode[0]);
    printf("We are in mode: %d\n", mode[0]);

    sleep_ms(500);

    // printf("Attempting to enter test pattern mode....\n");
    // uint8_t modeset[] = {0x05, 0x01};
    // i2c_write_blocking(i2c1, DLPC_addr, &modeset, 2, false); // true to keep master control of bus

    // sleep_ms(500);

    // uint8_t modenew[3];
    // i2c_write_blocking(i2c1, DLPC_addr, &modequery, 1, true); // true to keep master control of bus
    // i2c_read_blocking(i2c1, DLPC_addr, modenew, 1, false);
    // printf("We are NOW in mode: %x\n", modenew[0]);
    // printf("We are NOW in mode: %u\n", modenew[0]);
    // printf("We are NOW in mode: %d\n", modenew[0]);
}

void initialise_3D_print_mode_and_expose_frame(uint16_t exposure_time) {  // see section 3.3.1 of guide
    // set external print config

    // send image data to video buffer (via PIO)
    // WAIT for SYSTEM_READY flag of DLPC1438 //  we dont seem to have a hardware pin for this; so maybe just wait a bit?
        // "General purpose I/O 06 (hysteresis buffer). Reserved for System Ready signal (Output).
        // Indicates when system is configured and ready for first print layer command after being 
        //commanded to go into External Print Mode. Applicable to External Print Mode only"
    // Set External Print Layer Control (starts exposure)
}


void expose_new_frame(uint16_t exposure_time) {
    // send image data to video buffer (via PIO)
    // Set External Print Layer Control (starts exposure)
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
            ret = i2c_read_blocking(i2c1, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");
}

int main() {

    // Initialize stdio
    stdio_init_all();

    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    for (int i = 0; i < 10; i++) {
        printf("Blinking!\r\n");
        gpio_put(LED_PIN_G, 0);
        sleep_ms(250);
        gpio_put(LED_PIN_G, 1);
        sleep_ms(1000);
    }

    configure_i2c();

    //while(1) {
        scan_i2c();
        sleep_ms(500);
    //}

    show_DLP_test_pattern(2);

}
