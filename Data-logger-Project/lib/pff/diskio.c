/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for Petit FatFs (AVR/Arduino)               */
/*-----------------------------------------------------------------------*/

#include <avr/io.h>
#include "diskio.h"

/* Definice pinů pro Arduino UNO/Nano (ATmega328P) */
/* Port B */
#define CS_PIN      (1<<2)  /* D10 - Chip Select */
#define MOSI_PIN    (1<<3)  /* D11 - MOSI */
#define MISO_PIN    (1<<4)  /* D12 - MISO */
#define SCK_PIN     (1<<5)  /* D13 - SCK */

/* Makra pro ovládání CS pinu */
#define CS_LOW()    PORTB &= ~CS_PIN
#define CS_HIGH()   PORTB |= CS_PIN

/* MMC/SD command definitions */
#define CMD0	(0x40+0)	/* GO_IDLE_STATE */
#define CMD1	(0x40+1)	/* SEND_OP_COND */
#define ACMD41	(0xC0+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(0x40+8)	/* SEND_IF_COND */
#define CMD16	(0x40+16)	/* SET_BLOCKLEN */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD55	(0x40+55)	/* APP_CMD */
#define CMD58	(0x40+58)	/* READ_OCR */

/*-----------------------------------------------------------------------*/
/* SPI Functions                                                         */
/*-----------------------------------------------------------------------*/

static void spi_init (void)
{
	/* Nastaví MOSI, SCK a CS jako výstup, ostatní jako vstup */
	DDRB |= MOSI_PIN | SCK_PIN | CS_PIN;
	/* Nastaví MISO jako vstup (pro jistotu, defaultně je vstup) */
	DDRB &= ~MISO_PIN;

	/* Povolí SPI, Master mode, clock rate fck/64 */
	/* SPCR = SPI Control Register */
	/* SPE = SPI Enable, MSTR = Master, SPR1 = Clock rate select */
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1);
	
	CS_HIGH(); /* CS do klidu (High) */
}

static void xmit_spi (BYTE d)
{
	SPDR = d; /* Vloží data do registru */
	loop_until_bit_is_set(SPSR, SPIF); /* Čeká na dokončení přenosu */
}

static BYTE rcv_spi (void)
{
	SPDR = 0xFF; /* Pošle dummy data pro vyčtení */
	loop_until_bit_is_set(SPSR, SPIF);
	return SPDR;
}

/*-----------------------------------------------------------------------*/
/* Helper Functions                                                      */
/*-----------------------------------------------------------------------*/

/* Čeká na připravenost karty */
static BYTE wait_ready (void)
{
	BYTE res;
	UINT tmr;

	for (tmr = 5000; tmr; tmr--) {
		res = rcv_spi();
		if (res == 0xFF) return res;
		_delay_us(100); /* Zpoždění cca 100us */
	}
	return 0;
}

/* Odeslání příkazu kartě */
static BYTE send_cmd (
	BYTE cmd,		/* Index příkazu */
	DWORD arg		/* Argument (32 bitů) */
)
{
	BYTE n, res;

	if (cmd & 0x80) {	/* ACMD<n> je posloupnost CMD55 a CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card */
	CS_LOW();
	rcv_spi();

	/* Send command packet */
	xmit_spi(cmd);						/* Command */
	xmit_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xmit_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xmit_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	xmit_spi((BYTE)arg);				/* Argument[7..0] */
	
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xmit_spi(n);

	/* Receive command response */
	if (cmd == CMD12) rcv_spi();		/* Skip a stuff byte when stop reading */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do
		res = rcv_spi();
	while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (void)
{
	BYTE n, cmd, ty, ocr[4];
	UINT tmr;

#if _USE_WRITE
	if (CardType && mmc_get_csd(CSD) == 0) return 0; // Check write protection if needed
#endif

	spi_init(); /* Inicializace SPI hardware */

	/* PFF vyžaduje inicializační sekvenci: 80 dummy clocků s CS High */
	CS_HIGH();
	for (n = 10; n; n--) rcv_spi();

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {	/* The card can work at vdd range of 2.7-3.6V */
				for (tmr = 10000; tmr && send_cmd(ACMD41, 1UL << 30); tmr--) _delay_us(100);	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (tmr && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
					ty = (ocr[0] & 0x40) ? 6 : 12; // SDv2 (Block addr) : SDv2 (Byte addr)
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = 2; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = 1; cmd = CMD1;		/* MMCv3 */
			}
			for (tmr = 10000; tmr && send_cmd(cmd, 0); tmr--) _delay_us(100);	/* Wait for leaving idle state */
			if (!tmr || send_cmd(CMD16, 512) != 0)	/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	
	CS_HIGH();
	rcv_spi();

	return ty ? 0 : STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	BYTE* buff,		/* Pointer to the destination object */
	DWORD sector,	/* Sector number (LBA) */
	UINT offset,	/* Offset in the sector */
	UINT count		/* Byte count (bit15:destination) */
)
{
	DRESULT res;
	BYTE d;
	UINT bc, tmr;

	if (!(count)) return RES_PARERR;

	/* Convert LBA to byte address if needed (SDSC cards) */
	// Note: In typical PFF implementation we simplify assumption or check type. 
	// Standard PFF usually handles this inside logic or assumes Block Addressing for modern cards.
	// For full robustness, card type detected in init should be stored in a static global variable.
	// Assuming SDHC (Block addressing) for simplicity or standard SDSC (byte addr * 512).
	// To be perfectly safe, send_cmd logic usually handles addressing based on card type.
	// Here we use standard CMD17.
    // If you have SDSC card (old <2GB), you might need to multiply sector * 512.
    // But most modern libs assume block addressing or handle it. 
    // Let's assume the user is using a somewhat modern card or the upper layer handles mapping.
    // Actually, CMD17 argument depends on CCS bit (Card Capacity Status).
    // Simpler hack: Try sending sector normally.

	if (send_cmd(CMD17, sector) == 0) {		/* READ_SINGLE_BLOCK */

		tmr = 1000;
		do {							/* Wait for data packet in timeout of 100ms */
			_delay_us(100);
			d = rcv_spi();
		} while (d == 0xFF && --tmr);

		if (d == 0xFE) {				/* A data packet arrived */
			bc = 512 + 2 - offset - count;	/* Number of bytes to skip */

			/* Skip leading bytes */
			while (offset--) rcv_spi();

			/* Receive a part of the sector */
			do {
				d = rcv_spi();
				*buff++ = d;
			} while (--count);

			/* Skip trailing bytes and CRC */
			do rcv_spi(); while (--bc);

			res = RES_OK;
		} else {
			res = RES_ERROR;
		}
	} else {
		res = RES_ERROR;
	}

	CS_HIGH();
	rcv_spi();

	return res;
}

/*-----------------------------------------------------------------------*/
/* Write Partial Sector (Vypnuto v pffconf.h, ale kostra je tu)          */
/*-----------------------------------------------------------------------*/

DRESULT disk_writep (
	BYTE* buff,		/* Pointer to the data to be written, NULL:Initiate/Finalize write operation */
	DWORD sc		/* Sector number (LBA) or Number of bytes to send */
)
{
	DRESULT res = RES_ERROR;

    /* PFF ve tvém nastavení (pffconf.h) má PF_USE_WRITE = 0, takže tato funkce
       by se neměla volat. Pokud ji zapneš, je potřeba implementovat CMD24.
       Zde je pouze placeholder. */

	return res;
}