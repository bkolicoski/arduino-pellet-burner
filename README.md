# Arduino Pellet Burner
Arduino (ESP32) based pellet burner controller for a DIY pellet burner

**_The code of the controller is still work in progress and it is not ready yet to be used unattended._**

## Intro 
A friend of mine started working on a pellet burner so I jumped in and offered to make a controller for him. 

When finilized, the controller will be able to control:
- the ignition of the pellets, 
- the different feed rates required for stabilazing the burn,
- working with different power outputs, 
- controlling the blower fan speed depending on the state and conditions
- and controlling the flow pump based on the water temperature. 

All of the controller parameters are adjustable so the controller can be customized for different style of burners and stoves and it might sometime serve as a DIY replacement even for commercial stoves. 

The current version of the controller is made with an ESP32 microcontroller. 

## Contributing

Anyone is more than welcomed to clone the repo, try it out on their own and work on improvements to the code, as well as the entire setup and schematic. 

## Demo

The basic operation of the controller can be seen on the video below. 

[![Basic prototype of STM32 based pellet burner controller - Part 1](https://img.youtube.com/vi/7mLaWWiaeIQ/original.jpg)](https://www.youtube.com/watch?v=7mLaWWiaeIQ)


### Components and materials used in the pellet controller:
* ESP32 Microcontroller - https://s.click.aliexpress.com/e/_AeeJrb
* DS1302 Real time clock - https://s.click.aliexpress.com/e/_AFFBKD
* Rotary Encoder - https://s.click.aliexpress.com/e/_AeKzkz
* 4CH Relay Board - https://s.click.aliexpress.com/e/_9v1uXB
* 10K NTC Thermistor - https://s.click.aliexpress.com/e/_AFbVR3
* 20x4 LCD Display - https://s.click.aliexpress.com/e/_9xaXG9
* Mini breadboards - https://s.click.aliexpress.com/e/_ArbxHX
* Jumper Wires - https://s.click.aliexpress.com/e/_AYYJ9R
* Dupont wires - https://s.click.aliexpress.com/e/_AdZktL
* Resistors - https://s.click.aliexpress.com/e/_AWCi3R
* Micro USB cable - https://s.click.aliexpress.com/e/_AlE0TB
