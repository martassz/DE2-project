/*
 * utils.c
 */

#include "utils.h"
#include <stdio.h>      // For sprintf
#include "uart.h"       // Your UART library
#include "twi.h"        // Your TWI/I2C library

void i2c_scan(void) {
    uart_puts("I2C Scan: Start...\r\n");
    
    // Scan standard 7-bit addresses (1-127)
    for(uint8_t addr = 1; addr < 127; addr++) {
        // twi_test_address returns 0 if device acknowledges
        if(twi_test_address(addr) == 0) {
            char buf[32];
            sprintf(buf, " -> Device found at: 0x%02X\r\n", addr);
            uart_puts(buf);
        }
    }
    
    uart_puts("I2C Scan: Done.\r\n");
}