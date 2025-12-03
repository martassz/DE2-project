/**
 * @mainpage Environmental Data Logger Project
 *
 * @section intro_sec Introduction
 *
 * This project implements a **Portable Environmental Data Logger** using an AVR ATmega328P microcontroller.
 * It is designed to measure, display, and log key environmental parameters with accurate timestamping.
 *
 * The system is built using the **PlatformIO** ecosystem and strictly adheres to **pure C** programming
 * for the AVR architecture, avoiding the Arduino framework and libraries.
 *
 * @section features_sec Key Features
 *
 * - **Sensors:**
 * - **BME280:** Measures Temperature, Humidity, and Atmospheric Pressure (I2C).
 * - **Photoresistor:** Measures ambient light intensity (Analog ADC).
 * - **User Interface:**
 * - **LCD Display (16x2):** Connected via I2C (PCF8574) to visualize real-time data.
 * - **Rotary Encoder (KY-040):** User input for switching display screens and controlling logging.
 * - **Data Logging:**
 * - **SD Card:** Logs measurement data to `DATA.TXT` in CSV format.
 * - **File System:** Uses the lightweight **Petit FatFs** library.
 * - **Timekeeping:**
 * - **DS1302 RTC:** Provides accurate date and time for timestamps.
 *
 * @section structure_sec Software Architecture
 *
 * The project follows a non-blocking "Super-Loop" architecture with modular drivers:
 *
 * - **Application Layer:**
 * - `main.c`: Central loop handling timing, sensor polling, and task scheduling.
 * - `loggerControl`: Manages the UI state machine and high-level control logic.
 * - `sdlog`: High-level wrapper for SD card operations.
 *
 * - **Driver Layer (lib/):**
 * - `bme280`: I2C driver for the Bosch BME280 sensor.
 * - `ds1302`: Bit-banged 3-wire driver for the Real-Time Clock.
 * - `lcd`: HD44780 LCD controller driver.
 * - `uart`: Interrupt-driven UART library for debug output.
 * - `twi`: I2C/TWI Master driver.
 * - `gpio`, `timer`: Low-level AVR peripheral abstractions.
 * - `pff`: Petit FatFs library for SD card file system access.
 *
 * @section circuit_sec Circuit Connection
 *
 * | Component | AVR Port | Arduino Pin | Description |
 * | :--- | :--- | :--- | :--- |
 * | **BME280** | PC4/PC5 | A4/A5 | I2C SDA/SCL |
 * | **LCD** | PC4/PC5 | A4/A5 | I2C SDA/SCL |
 * | **SD Card** | PB2-PB5 | D10-D13 | SPI (SS, MOSI, MISO, SCK) |
 * | **DS1302** | PB0-PB2 | D8-D10 | CE, IO, SCLK |
 * | **Encoder** | PD5-PD7 | D5-D7 | CLK, DT, SW |
 * | **Light** | PC0 | A0 | Analog Input |
 *
 * @section authors_sec Authors
 *
 * - **Team DE2-Project** (2025)
 * - *Based on libraries by Peter Fleury, ChaN, and Tomas Fryza.*
 *
 * @copyright MIT License
 */