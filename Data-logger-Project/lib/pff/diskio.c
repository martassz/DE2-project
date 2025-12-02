/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for ATmega328P (PlatformIO) - DEBUG VERZE   */
/*-----------------------------------------------------------------------*/

#include <avr/io.h>
#include "diskio.h"
#include "pff.h"
#include "uart.h" // Přidáno pro debug výpisy

/* --- DEFINICE PINŮ --- */
#define CS_PORT PORTD
#define CS_DDR  DDRD
#define CS_PIN  PD4

#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_SCK  PB5
#define SPI_MOSI PB3
#define SPI_MISO PB4
#define SPI_SS   PB2

/* --- PŘÍKAZY PRO SD KARTU --- */
#define CMD0    (0)         /* GO_IDLE_STATE */
#define CMD1    (1)         /* SEND_OP_COND (MMC) */
#define CMD8    (8)         /* SEND_IF_COND */
#define CMD12   (12)        /* STOP_TRANSMISSION */
#define CMD16   (16)        /* SET_BLOCKLEN */
#define CMD17   (17)        /* READ_SINGLE_BLOCK */
#define CMD24   (24)        /* WRITE_BLOCK */
#define CMD55   (55)        /* APP_CMD */
#define CMD58   (58)        /* READ_OCR */
#define ACMD41  (0x80+41)   /* SEND_OP_COND (SDC) */

/* --- TYPY KARET --- */
#define CT_MMC      0x01
#define CT_SD1      0x02
#define CT_SD2      0x04
#define CT_BLOCK    0x08

/* --- MAKRA --- */
#define SELECT()    CS_PORT &= ~_BV(CS_PIN)
#define DESELECT()  CS_PORT |=  _BV(CS_PIN)

static BYTE CardType;

/* --- POMOCNÁ FUNKCE PRO VÝPIS HEX ČÍSLA --- */
void debug_hex(BYTE h) {
    BYTE d = h >> 4;
    uart_putc(d > 9 ? d + 55 : d + '0');
    d = h & 0x0F;
    uart_putc(d > 9 ? d + 55 : d + '0');
    uart_putc(' ');
}

/* --- SPI FUNKCE --- */
static void spi_init(void)
{
    CS_DDR |= _BV(CS_PIN);
    DESELECT();
    
    // Nastavíme MOSI, SCK a SS jako výstup
    SPI_DDR |= _BV(SPI_MOSI) | _BV(SPI_SCK) | _BV(SPI_SS);
    // MISO je automaticky vstup
    
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR1); // f_osc/64
    SPSR = 0; 
}

static void xmit_spi(BYTE d)
{
    SPDR = d;
    while (!(SPSR & _BV(SPIF)));
}

static BYTE rcv_spi(void)
{
    SPDR = 0xFF;
    while (!(SPSR & _BV(SPIF)));
    return SPDR;
}

/* --- SD FUNKCE --- */
static BYTE send_cmd (BYTE cmd, DWORD arg)
{
    BYTE n, res;
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }
    DESELECT();
    rcv_spi();
    SELECT();
    rcv_spi();
    xmit_spi(0x40 | cmd);
    xmit_spi((BYTE)(arg >> 24));
    xmit_spi((BYTE)(arg >> 16));
    xmit_spi((BYTE)(arg >> 8));
    xmit_spi((BYTE)arg);
    
    n = 0x01;
    if (cmd == CMD0) n = 0x95;
    if (cmd == CMD8) n = 0x87;
    xmit_spi(n);

    if (cmd == CMD12) rcv_spi();
    n = 10;
    do res = rcv_spi();
    while ((res & 0x80) && --n);
    return res;
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive (DEBUG VERZE)                                   */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (void)
{
    BYTE n, cmd, ty, ocr[4];
    UINT tmr;

    uart_puts("DISK_INIT: Start\r\n");
    spi_init();
    
    // Přidáme extra delay pro stabilizaci napájení karty
    for(volatile long i=0; i<10000; i++); 

    DESELECT();
    // Posíláme 80+ dummy clocks, aby se karta probrala
    for (n = 100; n; n--) rcv_spi();

    ty = 0;
    
    // Zkusíme CMD0 (Go Idle State)
    uart_puts("CMD0... ");
    n = send_cmd(CMD0, 0);
    debug_hex(n); // Vypíše návratový kód (očekáváme 01)
    uart_puts("\r\n");

    if (n == 1) {
        // Zkusíme CMD8 (Check Voltage)
        uart_puts("CMD8... ");
        if (send_cmd(CMD8, 0x1AA) == 1) {
            uart_puts("OK (SDv2)\r\n");
            for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
            debug_hex(ocr[2]); debug_hex(ocr[3]); uart_puts("\r\n"); // Očekáváme 01 AA
            
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                uart_puts("ACMD41 wait... ");
                // Čekáme na dokončení inicializace
                for (tmr = 20000; tmr && send_cmd(ACMD41, 1UL << 30); tmr--) ;
                if (tmr) uart_puts("OK\r\n"); else uart_puts("TIMEOUT\r\n");
                
                if (tmr && send_cmd(CMD58, 0) == 0) {
                    for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
                    ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
                    uart_puts("Card Type: "); debug_hex(ty); uart_puts("\r\n");
                }
            }
        } else {
            uart_puts("Fail/OldSD\r\n");
            // Starší karty (SDv1 nebo MMC)
            if (send_cmd(ACMD41, 0) <= 1) {
                ty = CT_SD1; cmd = ACMD41;
            } else {
                ty = CT_MMC; cmd = CMD1;
            }
            for (tmr = 20000; tmr && send_cmd(cmd, 0); tmr--) ;
            if (!tmr || send_cmd(CMD16, 512) != 0) ty = 0;
        }
    } else {
        uart_puts("CMD0 Failed. Check wiring!\r\n");
    }

    CardType = ty;
    DESELECT();
    rcv_spi();
    
    if (ty) {
        SPCR &= ~_BV(SPR1);
        SPCR &= ~_BV(SPR0);
        SPSR |= _BV(SPI2X);
        uart_puts("Init Success.\r\n");
    } else {
        uart_puts("Init Failed.\r\n");
    }

    return ty ? 0 : STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/
DRESULT disk_readp (BYTE* buff, DWORD sector, UINT offset, UINT count)
{
    DRESULT res;
    BYTE rc;
    UINT bc;

    if (!(count)) return RES_PARERR;
    if (!(CardType & CT_BLOCK)) sector *= 512;

    res = RES_ERROR;
    if (send_cmd(CMD17, sector) == 0) {
        bc = 30000;
        do { rc = rcv_spi(); } while (rc == 0xFF && --bc);

        if (rc == 0xFE) {
            bc = 512 + 2 - offset - count;
            while (offset--) rcv_spi();
            if (buff) {
                do { *buff++ = rcv_spi(); } while (--count);
            } else {
                do { rcv_spi(); } while (--count);
            }
            do rcv_spi(); while (--bc);
            res = RES_OK;
        }
    }
    DESELECT();
    rcv_spi();
    return res;
}

/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/
DRESULT disk_writep (const BYTE* buff, DWORD sc)
{
    DRESULT res;
    UINT bc;
    static UINT wc;

    res = RES_ERROR;
    if (buff) {
        bc = (UINT)sc;
        while (bc && wc) {
            xmit_spi(*buff++);
            wc--; bc--;
        }
        res = RES_OK;
    } else {
        if (sc) {
            if (!(CardType & CT_BLOCK)) sc *= 512;
            if (send_cmd(CMD24, sc) == 0) {
                xmit_spi(0xFF); xmit_spi(0xFE);
                wc = 512;
                res = RES_OK;
            }
        } else {
            bc = wc + 2;
            while (bc--) xmit_spi(0);
            if ((rcv_spi() & 0x1F) == 0x05) {
                for (bc = 65000; rcv_spi() != 0xFF && bc; bc--) ;
                if (bc) res = RES_OK;
            }
            DESELECT();
            rcv_spi();
        }
    }
    return res;
}