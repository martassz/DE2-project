# DE2 Data logger project

### Team members

* Matyas Heneberk (responsible for coding sensors & creating block diagrams, flowcharts & poster creation)
* Vojtěch Šafařík (responsible for data logging & RTC implementation & Python visualisation script & managing GitHub repository)
* Martin Zatloukal (responsible for coding sensors & data logging & RTC implementation & Python visualisation scripts)

### Abstract
This project focuses on creating a portable environmental data logger implemented on an Arduino UNO board using the toolchains provided by PlatformIO. The data logger displays the measured environmental data (temperature, humidity, barometric pressure and light) on a display. By pressing an encoder button, the data logger starts collecting the data to an SD card, which can then be visualised by a Python script on a computer. The SD card data logging uses the Petit FAT File System Module, a lightweight subset of FatFs written in ANSI C.

[**Video demonstration of our project**](add link here later)

![Project poster](images/poster.png "A3 project poster") add here later

## List of hardware components

* BME280 module
  - air pressure, temperature and humidity sensor combined within one small module

* Fotoresistor
  - simple light sensor
 
* RTC module DS1302
  - accurate and low-cost solution for keeping time when the data logger is powered off
  
* I2C LCD Display 1602
  - indication of current measurements without the need of computer

* Rotary encoder KY040
  - provides rotary selection and a clickable button in one simple module

* SD Card module
  - used for connecting SD card

## Components schematics
![Components schematics](images/Components_schematics.png "Components schematics") add here TODAY

## Pinout Configuration

| Interface Group | Pin Name | Arduino Pin | AVR Port | Connected Components |
| :--- | :--- | :--- | :--- | :--- |
| **I2C Bus** | **SDA** | **A4** | PC4 | **LCD 1602** (Addr: `0x27`) <br> **BME280** (Addr: `0x76`) |
| | **SCL** | **A5** | PC5 | |
| **SPI Bus** | **SCK** | **D13** | PB5 | **SD Card Module** |
| | **MISO** | **D12** | PB4 | |
| | **MOSI** | **D11** | PB3 | |
| | **CS** | **D4** | PD4 | |
| **RTC (DS1302)** | CLK | **D8** | PB0 | **RTC** |
| | DAT | **D9** | PB1 | |
| | RST | **D10** | PB2 | |
| **Controls & UI** | CLK | **D5** | PD5 | **Rotary Encoder** |
| | DT | **D6** | PD6 | |
| | SW | **D7** | PD7 | |
| **Analog Sensors** | Analog | **A0** | PC0 | **Photoresistor** |

## Software design

### Block diagram
![Block diagram](images/Block_diagram.png "Block_diagram") add here TODAY

### Flowchart
![Flowchart](images/Flowchart.png "Flowchart") add here TODAY

## Project demonstration
TODAY -- add photos here of the project, comment whats happening and then display the graphs on the computer, also add figure PNGs.

## References

* Digital Electronics 2 Course (Brno University of Technology): https://github.com/tomas-fryza/avr-labs
* Petit FAT File System Module: https://elm-chan.org/fsw/ff/00index_p.html
