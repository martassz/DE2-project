# DE2 Data logger project

### Team members

* Matyas Heneberk (responsible for coding sensors & managing GitHub repository & poster creation)
* Vojtěch Šafařík (responsible for data logging & managing GitHub repository)
* Martin Zatloukal (responsible for coding sensors & Python visualisation scripts)

### Abstract
This project focuses on creating a portable environmental data logger implemented on an Arduino UNO board using the toolchains provided by PlatformIO. The data logger displays the measured environmental data (temperature, humidity, barometric pressure and light) on a display. By pressing an encoder button, the data logger starts collecting the data to an SD card, which can then be visualised by a Python script on a computer. The data logger also has a red and green LED that blink upon runtime to indicate whether the environmental data in the room fall within optimal ranges of values (eg. humidity 35-70 %)

[**Video demonstration of our project**](add link here later)

![Project poster](images/poster.png "A3 project poster") add here later

## List of hardware components

* BME280 module
  - air pressure, temperature and humidity sensor combined within one small module

* Fotoresistor
  - simple light sensor
 
* RTC module DS3231
  - accurate and low-cost solution for keeping time when the data logger is powered off
  
* I2C LCD Display 1602
  - indication of current measurements without the need of computer

* Rotary encoder KY040
  - provides rotary selection and a clickable button in one simple module

* SD Card module
  - used for connecting SD card

## Software design

TO-DO: **add here** system-level block diagrams, flowcharts, or pseudocode showing the planned software logic and control flow

## References

* Digital Electronics 2 Course (Brno University of Technology): https://github.com/tomas-fryza/avr-labs
* Petit FAT File System Module: https://elm-chan.org/fsw/ff/00index_p.html
