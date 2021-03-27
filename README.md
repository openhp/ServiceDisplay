## ServiceDisplay

<b>The Service Display is a quickly assembled tool, that allows to connect [Valden Heat Pump Controller](https://github.com/openhp/HeatPumpController/) to get maximum available information from sensors and show it graphically.</b>

## Preamble
One day i'v realised that netbook with a serial console is very good diagnostic tool, but i want more compact tool to get maximum available information from heat pump. So this "Quickly Assembled Service Display" appeared. It fits everywhere and with a good power bank it can works 2-3 days long, without any additional power source. The diagnostic display is build from scratch, no PCB and housing here (and no plans to create it), because i do not see this device as a permanent-mounted display for the end user.<br>
It you want a compact and visual tool for your diagnostic purposes this device is for you.<br>
if you not needed expert info from sensors check [Remote Display page](https://github.com/openhp/Display/), there you'll find end-user device.<br><br>

{-Main look powered on picture here-}<br><br>

## Bill Of Materials
BOM is very short:
<br><br>
| Part | Quantity |
| ------------- | ------------- |
| 100Ω 0.25 Watt resistor	| 2	|
| Arduino rs485 converter module	| 2	|
| 2.4" Tft Display	| 1	|
| Arduino Mega	| 1	|

Display is as pictured below. Usually selled as "Arduino UNO TFT LCD". LCD driver chip is ILI9325.<br>This is the cheapest one and widely available touhscreen LCD you can find.<br>
<img src="./m_tft_photo_back.jpg" width="400"> <img src="./m_tft_photo_front.jpg" width="400"><br><br>

## Assembly and wiring

Mount converter modules, LCD and Arduino Mega.<br>
+5V, GND and signal ground wiring: 
- power source from both rs485 converters to 5V power at Mega board,
- GND from both GND,
- solder 100Ω resistors for both "internal" and "external": first out to ground, second to hole
- place a short tail of wire to nearest hole and solder to resistor output at back side

<br>
<img src="./m_tft_power_wiring.jpg" width="400"><br>

Signal connections wiring: <br>
- solder DE to RE (both pins together)
- Internal DI to A8
- Internal DE+RE to A9
- Internal RO to A10
- External DI to A11
- External DE+RE to A12
- External RO to A13
- A and B wires

<br>
<img src="./m_tft_485_wiring.jpg" width="400"><br>

Finally add spring connectors. Device ready to fast installation will looks like this:<br>
{-final result picture here-}<br><br>


For example of connection and usage at server sige [look here](https://github.com/openhp/HP-integration-example).<br>
You can ommit "to server" converter and connection if you'll use this device only as portable-only tool.<br> 

## Firmware upload
The process is the same as for others Arduinos:
- connect USB,
- start Arduino IDE,
- open the firmware file,
- select board and MCU in the Tools menu (hint: we are using "Mega" board),
- press the "Upload" button in the interface.

For successful compilation, you must have "Adafruit_GFX" and "Adafruit touch screen" installed (see Tools -> Manage Libraries).<br>
Also all library files (*.cpp* and *.h*) from Service Display repository must me downloaded and locatet at the same directory as a main *.ino* file.<br>
That's all, easy and fast.<br><br>

## Start and usage
Just power on the device. Next video shows you how it works.<br>
You can find full disgnostic messages and abbrevations list [at the Valden Heat Pump Controller page](https://github.com/openhp/HeatPumpController/) appendexes.<br>
{-video here-}

## License
GPLv3. <br>
This product is distributed in the hope that it will be useful,	but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.<br>

## Author
<br>
gonzho АТ web.de (c) 2018-2021<br>

