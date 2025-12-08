/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for ATmega328P                              */
/*-----------------------------------------------------------------------*/

/**
 * @file diskio.c
 * @brief Implementation of low-level disk I/O for SD Card via SPI.
 *
 * This file contains the hardware-specific implementation for checking,
 * reading, and writing to an SD card using the AVR's SPI peripheral.
 *
 * @author ChaN (Original Author)
 * @author Team DE2-Project (Port/Modifications)
 * @date 2025
 * @copyright MIT License
 *
 * @addtogroup pff_driver
 * @{
 */

#include <avr/io.h>
#include "diskio.h"

/* Optional: Include UART for debugging (uncomment if needed) */
/* #include "uart.h" */

/**
 * @name Pin Definitions
 * @brief Mapping of SD Card pins to AVR Port D and Port B.
 * @{
 */
#define CS_PORT PORTD   /**< Chip Select Port Data Register */
#define CS_DDR  DDRD    /**< Chip Select Data Direction Register */
#define CS_PIN  PD4     /**< Chip Select Pin (Active Low) */

#define SPI_PORT PORTB  /**< SPI Port Data Register */
#define SPI_DDR  DDRB   /**< SPI Data Direction Register */
#define SPI_SCK  PB5    /**< SPI Clock Pin */
#define SPI_MOSI PB3    /**< SPI Master Out Slave In Pin */
#define SPI_MISO PB4    /**< SPI Master In Slave Out Pin */
#define SPI_SS   PB2    /**< SPI Slave Select Pin (Must be Output for Master mode) */
/** @} */

/**
 * @name SD Card Commands
 * @brief Standard SD/MMC commands used during initialization and data transfer.
 * @{
 */
#define CMD0    (0)         /**< GO_IDLE_STATE: Reset the card to idle state */
#define CMD1    (1)         /**< SEND_OP_COND: Initiate initialization process (MMC) */
#define CMD8    (8)         /**< SEND_IF_COND: Check voltage range (SDv2) */
#define CMD12   (12)        /**< STOP_TRANSMISSION: Stop read/write transmission */
#define CMD16   (16)        /**< SET_BLOCKLEN: Set block length (for non-SDHC) */
#define CMD17   (17)        /**< READ_SINGLE_BLOCK: Read a single block (512 bytes) */
#define CMD24   (24)        /**< WRITE_BLOCK: Write a single block (512 bytes) */
#define CMD55   (55)        /**< APP_CMD: Leading command for ACMD commands */
#define CMD58   (58)        /**< READ_OCR: Read Operation Conditions Register */
#define ACMD41  (0x80+41)   /**< SEND_OP_COND: Initiate initialization process (SDC) */
/** @} */

/**
 * @name Card Types
 * @brief Bit flags indicating the type of the connected memory card.
 * @{
 */
#define CT_MMC      0x01    /**< MMC ver 3 */
#define CT_SD1      0x02    /**< SD ver 1 */
#define CT_SD2      0x04    /**< SD ver 2 */
#define CT_BLOCK    0x08    /**< Block addressing */
/** @} */

/**
 * @name Control Macros
 * @brief Macros for controlling the Chip Select (CS) line.
 * @{
 */
#define SELECT()    CS_PORT &= ~_BV(CS_PIN) /**< Assert CS (Set Low) to enable card */
#define DESELECT()  CS_PORT |=  _BV(CS_PIN) /**< Deassert CS (Set High) to disable card */
/** @} */


/** * @brief Global variable storing the detected card type.
 * Initialized by disk_initialize().
 */
static BYTE CardType;


/* -- SPI Helper Functions ------------------------------------------- */

/**
 * @brief Initialize SPI peripheral and pins.
 * * Configures the MOSI, SCK, and SS pins as outputs. 
 * Enables the SPI module in Master mode with a clock rate of f_osc/64.
 * * @note SS pin (PB2) must be defined as output for the SPI Master mode 
 * to work correctly, even if not used as the actual Slave Select.
 */
static void spi_init(void)
{
    /* Set CS as Output and Deselect */
    CS_DDR |= _BV(CS_PIN);
    DESELECT();
    
    /* Set MOSI, SCK, and SS as Output */
    SPI_DDR |= _BV(SPI_MOSI) | _BV(SPI_SCK) | _BV(SPI_SS);
    /* MISO is automatically configured as input */
    
    /* Enable SPI, Master Mode, Clock rate f_osc/64 */
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR1);
    SPSR = 0; 
}

/**
 * @brief Transmit a single byte via SPI.
 * * Writes the data to the SPI Data Register and waits until the 
 * transmission is complete.
 * * @param d Byte to transmit.
 */
static void xmit_spi(BYTE d)
{
    SPDR = d;
    while (!(SPSR & _BV(SPIF))); /* Wait for transmission complete */
}

/**
 * @brief Receive a single byte via SPI.
 * * Sends a dummy byte (0xFF) to generate clock pulses and waits 
 * for the received data.
 * * @return Received byte.
 */
static BYTE rcv_spi(void)
{
    SPDR = 0xFF; /* Send dummy byte to generate clock */
    while (!(SPSR & _BV(SPIF)));
    return SPDR;
}


/* -- SD Card Internal Functions ------------------------------------- */

/**
 * @brief Send a command packet to the SD card.
 * * Handles the low-level SPI transaction for sending a command. 
 * Automatically handles the `ACMD` prefix (CMD55) if the command 
 * has the high bit set (0x80).
 * * @param cmd Command index (e.g., CMD0, CMD17). 
 * If bit 7 is set, it is interpreted as an ACMD.
 * @param arg 32-bit argument for the command.
 * @return The response byte from the card (R1 format).
 */
static BYTE send_cmd(BYTE cmd, DWORD arg)
{
    BYTE n, res;

    /* ACMD<n> is the command sequense of CMD55-CMD<n> */
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }

    /* Select the card */
    DESELECT();
    rcv_spi();
    SELECT();
    rcv_spi();

    /* Send command packet */
    xmit_spi(0x40 | cmd);           /* Start + Command index */
    xmit_spi((BYTE)(arg >> 24));    /* Argument[31..24] */
    xmit_spi((BYTE)(arg >> 16));    /* Argument[23..16] */
    xmit_spi((BYTE)(arg >> 8));     /* Argument[15..8] */
    xmit_spi((BYTE)arg);            /* Argument[7..0] */
    
    /* Send CRC */
    n = 0x01;                       /* Dummy CRC + Stop */
    if (cmd == CMD0) n = 0x95;      /* Valid CRC for CMD0(0) */
    if (cmd == CMD8) n = 0x87;      /* Valid CRC for CMD8(0x1AA) */
    xmit_spi(n);

    /* Receive command response */
    if (cmd == CMD12) rcv_spi();    /* Skip a stuff byte when stop reading */
    n = 10;                         /* Wait for a valid response in timeout of 10 attempts */
    do {
        res = rcv_spi();
    } while ((res & 0x80) && --n);

    return res;
}


/* -- Public DiskIO Functions ---------------------------------------- */

/**
 * @brief Initialize the Disk Drive (SD Card).
 * * Performs the standard SD card initialization sequence:
 * 1. Initialize SPI.
 * 2. Send dummy clocks to wake up the card.
 * 3. Send CMD0 to enter Idle State.
 * 4. Determine card type (SDv2, SDv1, or MMC) and initialize.
 * 5. Configure block length to 512 bytes (if not SDHC).
 * 6. Set SPI to double speed (f_osc/2) on success.
 * * @return Status code:
 * @retval 0           Initialization succeeded.
 * @retval STA_NOINIT  Initialization failed.
 */
DSTATUS disk_initialize(void)
{
    BYTE n, cmd, ty, ocr[4];
    UINT tmr;

    /* uart_puts("DISK: Init start\r\n"); */
    spi_init();
    
    /* Extra delay for power stabilization */
    for(volatile long i=0; i<10000; i++); 

    DESELECT();
    /* Send 80+ dummy clocks to wake up the card */
    for (n = 100; n; n--) rcv_spi();

    ty = 0;
    
    /* Attempt CMD0 (Go Idle State) */
    /* uart_puts("CMD0... "); */
    n = send_cmd(CMD0, 0);
    /* debug_hex(n); uart_puts("\r\n"); */

    if (n == 1) {
        /* Attempt CMD8 (Check Voltage) - Check for SDv2 */
        /* uart_puts("CMD8... "); */
        if (send_cmd(CMD8, 0x1AA) == 1) {
            /* uart_puts("OK (SDv2)\r\n"); */
            for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
            
            /* Check if voltage range is valid (2.7-3.6V) */
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                /* Wait for initialization (ACMD41) */
                /* uart_puts("ACMD41 wait... "); */
                for (tmr = 20000; tmr && send_cmd(ACMD41, 1UL << 30); tmr--) ;
                /* if (tmr) uart_puts("OK\r\n"); else uart_puts("TIMEOUT\r\n"); */
                
                /* Check CCS bit in OCR (CMD58) */
                if (tmr && send_cmd(CMD58, 0) == 0) {
                    for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
                    ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
                    /* uart_puts("Card Type: "); debug_hex(ty); uart_puts("\r\n"); */
                }
            }
        } else {
            /* SDv1 or MMC card */
            /* uart_puts("Fail/OldSD\r\n"); */
            if (send_cmd(ACMD41, 0) <= 1) {
                ty = CT_SD1; cmd = ACMD41;
            } else {
                ty = CT_MMC; cmd = CMD1;
            }
            /* Wait for initialization */
            for (tmr = 20000; tmr && send_cmd(cmd, 0); tmr--) ;
            /* Set block length to 512 */
            if (!tmr || send_cmd(CMD16, 512) != 0) ty = 0;
        }
    } else {
        /* uart_puts("CMD0 Failed.\r\n"); */
    }

    CardType = ty;
    DESELECT();
    rcv_spi();
    
    if (ty) {
        /* Increase SPI speed for data transfer */
        SPCR &= ~_BV(SPR1);
        SPCR &= ~_BV(SPR0);
        SPSR |= _BV(SPI2X); /* Double speed mode */
        /* uart_puts("Init Success.\r\n"); */
    } else {
        /* uart_puts("Init Failed.\r\n"); */
    }

    return ty ? 0 : STA_NOINIT;
}

/**
 * @brief Read partial sector from the disk.
 * * Reads a specified number of bytes from a sector on the SD card.
 * * @param buff   Pointer to the buffer to store read data.
 * @param sector Sector number (LBA).
 * @param offset Byte offset within the sector to start reading from.
 * @param count  Number of bytes to read.
 * @return Operation result:
 * @retval RES_OK      Success.
 * @retval RES_ERROR   Read error or timeout.
 * @retval RES_PARERR  Invalid parameter.
 */
DRESULT disk_readp(BYTE* buff, DWORD sector, UINT offset, UINT count)
{
    DRESULT res;
    BYTE rc;
    UINT bc;

    if (!(count)) return RES_PARERR;
    if (!(CardType & CT_BLOCK)) sector *= 512; /* Convert to byte address if not block addressing */

    res = RES_ERROR;
    if (send_cmd(CMD17, sector) == 0) { /* READ_SINGLE_BLOCK */
        bc = 30000;
        do { rc = rcv_spi(); } while (rc == 0xFF && --bc);

        if (rc == 0xFE) { /* Data Token received */
            bc = 512 + 2 - offset - count;
            
            /* Skip leading bytes */
            while (offset--) rcv_spi();
            
            /* Read requested data */
            if (buff) {
                do { *buff++ = rcv_spi(); } while (--count);
            } else {
                do { rcv_spi(); } while (--count);
            }
            
            /* Skip trailing bytes and CRC */
            do rcv_spi(); while (--bc);
            res = RES_OK;
        }
    }
    DESELECT();
    rcv_spi();
    return res;
}

/**
 * @brief Write partial sector to the disk.
 * * Writes data to a sector on the SD card. If `buff` is provided, data is written.
 * If `buff` is NULL and `sc` is non-zero, a Write Block command is initiated.
 * If `buff` is NULL and `sc` is zero, the write transaction is finalized.
 * * @param buff Pointer to the data to be written.
 * @param sc   Sector number (LBA) or control flag.
 * @return Operation result:
 * @retval RES_OK      Success.
 * @retval RES_ERROR   Write error.
 */
DRESULT disk_writep(const BYTE* buff, DWORD sc)
{
    DRESULT res;
    UINT bc;
    static UINT wc;

    res = RES_ERROR;
    
    if (buff) {
        /* Send data bytes */
        bc = (UINT)sc;
        while (bc && wc) {
            xmit_spi(*buff++);
            wc--; bc--;
        }
        res = RES_OK;
    } else {
        if (sc) {
            /* Initiate Write Sector */
            if (!(CardType & CT_BLOCK)) sc *= 512;
            if (send_cmd(CMD24, sc) == 0) {
                xmit_spi(0xFF); xmit_spi(0xFE); /* Start Token */
                wc = 512;
                res = RES_OK;
            }
        } else {
            /* Finalize Write */
            bc = wc + 2;
            while (bc--) xmit_spi(0); /* Fill remaining bytes with 0 */
            
            /* Check data response (xxxx0101 = accepted) */
            if ((rcv_spi() & 0x1F) == 0x05) {
                /* Wait for busy state to end */
                for (bc = 65000; rcv_spi() != 0xFF && bc; bc--) ;
                if (bc) res = RES_OK;
            }
            DESELECT();
            rcv_spi();
        }
    }
    return res;
}

/** @} */ // End of addtogroup pff_driver