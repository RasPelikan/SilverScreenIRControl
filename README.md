# SilverScreenIRControl
A controller used to move in or move out a silver screen (used for home cinema) by an IR remote control. It is adaptable for different remote controls and programable for any two keys (one for moving up and one for moving down).

## Functionality
The controller receives IR signals. If the potentionmeter is at position 0% the next signal received will be stored as "down button". Move the position a little bit away from 0%. Now the next signal received will be stored as "up button". Next move the position to 100%. The next recieved down button signal will enable JP1 for 46 seconds. JP1 has to be connected to a relais which is wired to the silver screen's down line. If 46 seconds is to long to stop at your matching height then move the potentiometer to something below 100% (50% means 23 seconds). Pressing the up button will always enable JP6 for 46 seconds. JP6 has to be connected to a relais which is wired to the silver screen's up line. Pressing the up button during down movement interrupts the configured period of time. Now every down button signal moves the silver screen only for 0.5 seconds. Now it is necessary to press the up button and wait until 46 seconds are over. Afterwards the controller can be sure that the silver screen is entirely hidden and the next down button signal will force the controller to move down for the period of time configured by the potentiometer's position.

Hint: The IR signal processed is limited to NEC and Sony remote controls. You might adapt the file irmpconfig.h if your remote control does not work. At this point I want to give a Kudos (https://en.wikipedia.org/wiki/Kudos) to the guys of https://www.mikrocontroller.net/articles/IRMP who implemented the IR multiprotocol decoder I used in this project. They have done a very good job, thank you!

## Firmware
The firmware is for the ATtiny45 MCU. It can be built using the Eclipse and the "AVR Eclipse Plugin" (http://avr-eclipse.sourceforge.net/wiki/index.php/The_AVR_Eclipse_Plugin).

## Hardware
Schema:
![schema](https://raw.githubusercontent.com/RasPelikan/SilverScreenIRControl/gh-pages/schema.png)
Breadboard:
![breadboard](https://raw.githubusercontent.com/RasPelikan/SilverScreenIRControl/gh-pages/breadboard.png)
PC:
![PCB](https://raw.githubusercontent.com/RasPelikan/SilverScreenIRControl/gh-pages/pcb.png)
(https://raw.githubusercontent.com/RasPelikan/SilverScreenIRControl/gh-pages/etching.pdf)

