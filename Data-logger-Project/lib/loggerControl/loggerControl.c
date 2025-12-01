/*
 * loggerControl.c - Complete Implementation
 * * Vlastnosti:
 * - Ovládání rotačního enkodéru pomocí stavového automatu (odolné proti zákmětům)
 * - I2C LCD výstup
 * - Čtení času z DS1302/DS3231
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

#include "loggerControl.h"
#include "lcd_i2c.h" 
#include "twi.h"
#include "ds1302.h"
#include "sdlog.h"   

/* --- KONFIGURACE PINŮ ENKODÉRU (PORTC) --- */
#define ENC_SW   PD7
#define ENC_DT   PD6
#define ENC_CLK  PD5

#define ENC_PORT_REG  PORTD
#define ENC_DDR_REG   DDRD
#define ENC_PIN_REG   PIND

/* --- DEFINICE ADRES RTC (které chyběly) --- */
#define RTC_ADR     0x68
#define RTC_SEC_MEM 0x00

/* --- EXTERNÍ PROMĚNNÉ --- */
// Potřebujeme čas systému z main.c pro debounce tlačítka
extern volatile uint32_t g_millis; 

/* --- LOKÁLNÍ PROMĚNNÉ PRO ENKODÉR (STAVOVÝ AUTOMAT) --- */
// Tabulka platných přechodů (Grayův kód). 
// Eliminuje zákmity kontaktů lépe než prosté čtení hrany.
static const int8_t encoder_table[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};

static uint8_t old_AB = 0;         // Minulý stav pinů CLK a DT
static int8_t enc_counter = 0;     // Počítadlo kroků
static uint32_t last_btn_time = 0; // Čas posledního stisku tlačítka

/* --- GLOBÁLNÍ PROMĚNNÉ PRO DISPLEJ --- */
/* 0=Temp, 1=Pressure, 2=Humidity, 3=Light */
volatile uint8_t lcdValue = 0;      
volatile uint8_t flag_update_lcd = 0; // 1 = Žádost o překreslení

/* Pomocná funkce pro BCD */
static uint8_t bcd2dec(uint8_t v) { return ((v>>4)*10 + (v & 0x0F)); }

/* ==========================================
 * INICIALIZACE A UI
 * ========================================== */

void logger_display_init(void)
{
    lcd_i2c_init(); 
    lcd_i2c_clrscr();
    
    lcd_i2c_gotoxy(0,0);
    lcd_i2c_puts("  DATA LOGGER  ");
    lcd_i2c_gotoxy(0,1);
    lcd_i2c_puts("   VUT FEKT    ");
}

void logger_encoder_init(void)
{
    // Nastavit piny jako vstup (0)
    ENC_DDR_REG  &= ~((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));
    // Zapnout interní pull-up rezistory (1)
    ENC_PORT_REG |=  ((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));

    // Načíst počáteční stav pinů pro stavový automat
    uint8_t clk = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;
    uint8_t dt  = (ENC_PIN_REG & (1 << ENC_DT))  ? 1 : 0;
    
    // Uložíme si počáteční stav (bit 1 = CLK, bit 0 = DT)
    old_AB = (clk << 1) | dt;
}

/* * Hlavní funkce pro čtení enkodéru.
 * Volat co nejčastěji v main loop.
 */
void logger_encoder_poll(void)
{
    // --- 1. ČTENÍ OTÁČENÍ (Tabulková metoda) ---
    
    // Přečteme aktuální piny
    uint8_t clk = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;
    uint8_t dt  = (ENC_PIN_REG & (1 << ENC_DT))  ? 1 : 0;

    // Vytvoříme novou hodnotu stavu (0-3)
    uint8_t current_AB = (clk << 1) | dt;

    // Pouze pokud se stav změnil
    if (current_AB != (old_AB & 0x03)) {
        
        // Vytvoříme index do tabulky: (Starý_Stav << 2) | Nový_Stav
        old_AB <<= 2;
        old_AB |= current_AB;

        // Přičteme pohyb z tabulky
        enc_counter += encoder_table[old_AB & 0x0F];

        // POZOR: Zde se ladí citlivost.
        // Standardní KY-040 má 4 fáze na 1 fyzické cvaknutí.
        // Pokud to chodí moc ztuha, změň 4 na 2.
        if (enc_counter >= 4) { 
            // KROK VPRAVO (CW)
            lcdValue++;
            if (lcdValue > 3) lcdValue = 0;
            
            flag_update_lcd = 1;
            enc_counter = 0; 
        }
        else if (enc_counter <= -4) {
            // KROK VLEVO (CCW)
            if (lcdValue == 0) lcdValue = 3;
            else lcdValue--;
            
            flag_update_lcd = 1;
            enc_counter = 0; 
        }
    }
    // Uložíme stav pro příští průchod (maskování na 2 bity)
    old_AB &= 0x03; 

    // --- 2. ČTENÍ TLAČÍTKA (S časovým zámkem) ---
    
    if ((ENC_PIN_REG & (1 << ENC_SW)) == 0) {
        // Tlačítko je stisknuté (Active Low)
        uint32_t now = g_millis; 
        
        // Debounce: reagujeme maximálně jednou za 250 ms
        if ((now - last_btn_time) > 250) {
            flag_sd_toggle = 1;      // Požadavek na start/stop logování
            flag_update_lcd = 1;     // Překreslit, aby se objevila hvězdička
            last_btn_time = now;
        }
    }
}

/* ==========================================
 * RTC A DISPLEJ
 * ========================================== */

void logger_rtc_read_time(void)
{
    uint8_t buf[3];
    // Přečíst 3 byty (sec, min, hour) z RTC přes I2C
    twi_readfrom_mem_into(RTC_ADR, RTC_SEC_MEM, buf, 3);

    uint8_t sec  = bcd2dec(buf[0] & 0x7F);
    uint8_t min  = bcd2dec(buf[1]);
    uint8_t hour = bcd2dec(buf[2] & 0x3F); 

    // Atomický zápis do globální struktury
    uint8_t sreg = SREG; cli();
    g_time.hh = hour;
    g_time.mm = min;
    g_time.ss = sec;
    SREG = sreg;
}

void logger_display_draw(void)
{
    // Vynulujeme flag, protože právě kreslíme
    flag_update_lcd = 0;

    // --- ŘÁDEK 1: Hlavička + Čas + SD ikona ---
    lcd_i2c_gotoxy(0,0);
    
    char sd_icon = sd_logging ? '*' : ' '; 
    char timeStr[9];
    snprintf(timeStr, 9, "%02d:%02d:%02d", g_time.hh, g_time.mm, g_time.ss);

    // Výpis názvu veličiny
    switch (lcdValue) {
        case 0: lcd_i2c_puts("TEMP   "); break;
        case 1: lcd_i2c_puts("PRESS  "); break;
        case 2: lcd_i2c_puts("HUMID  "); break;
        case 3: lcd_i2c_puts("LIGHT  "); break;
    }
    
    // Výpis SD ikony a času
    lcd_i2c_putc(sd_icon);
    lcd_i2c_puts(timeStr);

    // --- ŘÁDEK 2: Hodnota + Jednotka ---
    lcd_i2c_gotoxy(0,1);
    char valStr[16];
    
    switch (lcdValue)
    {
        case 0: // Teplota
            dtostrf(g_T, 6, 1, valStr); 
            lcd_i2c_puts(valStr); 
            lcd_i2c_puts(" \xDF""C   "); // \xDF je znak stupně na LCD
            break;

        case 1: // Tlak
            dtostrf(g_P, 7, 1, valStr);
            lcd_i2c_puts(valStr); 
            lcd_i2c_puts(" hPa  ");
            break;

        case 2: // Vlhkost
            dtostrf(g_H, 6, 1, valStr);
            lcd_i2c_puts(valStr); 
            lcd_i2c_puts(" %    ");
            break;

        case 3: // Světlo (celé číslo)
            sprintf(valStr, "%u", g_Light);
            lcd_i2c_puts(valStr);
            lcd_i2c_puts(" %      "); 
            break;

        default:
            lcd_i2c_puts("Error           ");
            break;
    }
}