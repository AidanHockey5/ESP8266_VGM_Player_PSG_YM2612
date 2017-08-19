# ESP8266_VGM_Player_PSG_YM2612

Video of this project in action can be seen here: https://youtu.be/KguRHrbGrXg

A VGM (Video Game Music) player featuring the YM2612 and SN76489 PSG from the Sega Genesis. I use a NodeMCU board to host the ESP8266

This project is an extention of my previous [PSG-only VGM player.](https://github.com/AidanHockey5/ESP8266_VGM_Player) 

# Information about the main sound chips and VGM

The YM2612, AKA OPN2 (FM Operator, type N), is a frequency modulation synthesizer that was found in the Sega Genesis. With the exception of a few scraps of information online, this chip has mainly been lost to time. There is no official data sheet and Yamaha semiconductor no longer holds records of them. It was quite a challenge to get anything out of this chip, much less a full VGM file player.

The SN76489, AKA PSG (Programmable Sound Generator), is a simple 3-square wave channel, 1-noise channel sound chip that was a staple in early home computers and video game consoles. It was most notably found in the Sega Master System - the predecessor of the Sega Genesis/Megadrive.

VGM stands for Video Game Music, and it is a 44.1KHz logging format that stores real soundchip register information. My player will parse these files and send the data to the appropriate chips. You can learn more about the VGM file format here: http://www.smspower.org/Music/VGMFileFormat

http://www.smspower.org/uploads/Music/vgmspec170.txt?sid=58da937e68300c059412b536d4db2ca0

# Hooking Everything Up
Because of limited IO on the ESP8266, I needed to control the sound chips via SN74HC595 shift registers. There are three shift registers: One for PSG data, one for YM2612 data, and one for controlling both chips. There is also two LTC6903 programmable oscillators that are programmed on power-up by a PIC16F690 chip. You don't need to use these specific chips for clocking, you just need to generate a 7.67 MHz signal for the YM2612 and a 3.58 MHz signal for the PSG. Crystals in these frequencies may be tricky to find, hence the LTC6903's.
I also reccomend heavy power filtering with large value capacitors (1000uF-2200uF), especially near the super sensitive YM2612 power pins. These old chips are power hogs, so external power will be required on top of plugging in the ESP8266 to USB.

Here is how to connect the ESP8266 to the shift registers:

ESP8266 | YM2612 Data Shift Register
------------ | -------------
D0 | Latch (12)
D1 | SRClock (11)
D2 | SERdata (14)

ESP8266 | Control Shift Register
------------ | -------------
D3 | Latch (12)
D4 | SRClock (11)
D5 | SERdata (14)

ESP8266 | PSG Data Shift Register
------------ | -------------
D6 | Latch (12)
D7 | SRClock (11)
D8 | SERdata (14)


---------------------------------------------------------------------------------------------------------------

Connect the data registers like this:

YM2612 Data Shift Register | YM2612
------------ | -------------
QA-QH | D0-D7

PSG Data Shift Register | SN76489 PSG
------------ | -------------
QA-QH | D0-D7

---------------------------------------------------------------------------------------------------------------

Connect the control register like this:

Control Shift Register | PSG & YM2612
------------ | -------------
QA | PSG WE
QB | YM2612 IC
QC | YM2612 CS
QD | YM2612 WR
QE | YM2612 RD
QF | YM2612 A0
QG | YM2612 A1
QH | NO CONNECTION

Clock hookups and other supporting connections can be viewed in the schematic below.

---------------------------------------------------------------------------------------------------------------

# Schematic
![Schematic](https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612/blob/master/SchematicsAndInfo/ESP8266_YM2612_SN76489_VGM_Player.sch.png)

[Vector version here](https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612/blob/master/SchematicsAndInfo/ESP8266_YM2612_SN76489_VGM_Player.sch.svg)

[PDF](https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612/blob/master/SchematicsAndInfo/ESP8266_YM2612_SN76489_VGM_Player.pdf)

I did not include the LEDs in this schematic for simplicities sake. If you want them, just wire up the LEDs on the same pins as the shift register outputs with 220 OHM resistors in series. 

# ESP8266 Compile Settings
Board -> NodeMCU 1.0 (ESP-12E Module)

Flash Size -> 4M (3M SPIFFS)

CPU Frequency -> 160MHz

# How to Upload New VGM Files

This project will only parse uncompressed VGM format files. Directly sticking in a .VGZ file WILL NOT WORK! Instead, use [7Zip](http://www.7-zip.org/download.html) to extract the file from VGZ archives. You can find VGM music on several sites, http://project2612.org/list.php is my favorite.

Once you have your VGM files of choice, enter the sketch folder of this project and look for the "data" folder. Add your uncompressed VGM files here. Make sure to rename them to "1.vgm, 2.vgm, 3.vgm... etc." Make sure to change 
    const int NUMBER_OF_FILES
to match the number of files you have.
The ESP8266 can only hold about 2.5MB worth of data in the SPIFFS file system, so don't go too nuts! (Usually around 7 songs works).

After that, upload your files using the SPIFFS upload tool which you can find here: http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html

# Pause, Play, Forward Song, Previous Song
You can simply use the serial port for these commands. Consult the table below:

Action | Command
------------ | -------------
Pause/Play | \*
Next Song | +
Previous Song | -

...Or, you could wire up an external UART device to send those characters over to the RX pin of the ESP8266. Transmitting device is connected via TX, ESP8266 is connected via RX. Make sure to remove these UART connections while programming/uploading songs.

# Limitations
Because this project runs off of three shift registers, songs with heavy amounts of PCM sample usage may sound slow or distorted. The only way around this would be to have more direct IO on the ESP8266... which we can't have. I plan to release a faster version of this project based on the ESP32 module once SPIFFS is released for it.

# Shout-outs
Thank you so much for the help, [Jarek](https://hackaday.io/Jarek)! I wouldn't have been able to do this without your amazing documentation and assistance! [Check out his version of a hardware VGM player here!](https://hackaday.io/project/13361-sega-genesis-native-hardware-chiptune-synthesizer)

