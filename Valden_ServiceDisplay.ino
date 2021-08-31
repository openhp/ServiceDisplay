/*

	Valden Heat Pump.
	
	Remote Display firmware.
	
	https://github.com/OpenHP/
	
	Copyright (C) 2019-2021 gonzho@web.de
	
	

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


//-----------------------USER OPTIONS-----------------------
const int i2c_DisplayAddr 	= 0x3F; 	//if 1602: i2c address, if no text on screen: use i2c scanner sketch, typical 0x3f, 0x27, if address ok: tune 1602 contrast

#define T_SETPOINT_MAX 		48.0;  		//maximum "setpoint" temperature that an ordinary user can set
#define T_SETPOINT_MIN		10.0;		//min. "setpoint" temperature that an ordinary user can set, lower values are not recommended until antifreeze fluids at hot side are used.

#define T_SETPOINT_DAY 		22.00;	
#define T_SETPOINT_NIGHT	24.00;
#define T_EEV_SETPOINT 		04.00;		//ordinary user CANNOT set this value via display! you can change it only from host, value will be transmitted to controller.

#define WATCHDOG          			//disable for older bootloaders
//-----------------------USER OPTIONS END-----------------------

//#define A_MEGA		2			//service display
#define A_MINI		1		//user display

//-----------------------ADVANCED OPTIONS -----------------------
#define RS485_HP_MODBUS 		3 	//use this option
//#define RS485_HP_JSON 		13 	//not implemented

//#define RS485_HOST_MODBUS 		11 	//implemented but untested, please send bug reports if you'll test this option
#define RS485_HOST_JSON		1		//use this option

const char devID  		= 0x40;		//this device, external net, json communication, see example at https://github.com/openhp/HP-integration-example 
const char hostID 		= 0x30;		//host/master, external net, not used if modbus
const char hpID  		= 0x00;		//internal net: do not change if heat pump controller https://github.com/OpenHP/  HP responses to broadcast via Modbus to simplify installation process

#define MAX_SEQUENTIAL_ERRORS 		7 	//max cycles to wait, before "no connection" message

const int i2c_dsAddr		= 0x68;		//RTC address, used directly only to check if RTC alive

#define MAGIC     			0x45;  	//change this value if you want to rewrite target T values
//-----------------------ADVANCED OPTIONS END-----------------------

//#define wifi
//#define wifi_ap		"ap_q0"
//#define wifi_password	"eiyafiafjioucudl"
//-----------------------changelog-----------------------
/*
v1.0, 15 jun 2019:
+ initial version
v1.1, 04 jan 2020:
+ wifi prototyped
v1.2, 06 jan 2019:
+ modbus
v1.3, 06 jun 2020:
+ buttons
+ conditional day/night
+ display,modbus full functionality done
+ poss. DoS: infinite read to nowhere, fix it, set finite counter (ex: 200)
v1.4, 01 dec 2020:
+ "mega" scheme support
v1.5: 04 Feb 2021
+ new "mini" hardware revision (1.6), older ones treated as development branch and not supported anymore
+ auto Ts1 view on main display, if not -127.0
+ local copy of SoftSerial with increased buffer
+ mini: version at startup, credits at startup
v1.6: 27 Feb 2021
+ partial old code (Tsl and so on) cleanup
v1.7: 20 Mar 2021
+ RTC check
v1.8: 21 Mar 2021
+ artefacts from long values prevention at tft
v1.9: 26 Mar 2021:
+ rounding error via Modbus found and fixed
v1.10: 27 Mar 2021:
+ LSM faster redraw

TODO:
- display: rewrite to interrupts
- proto: implement in translated stats get EEV MANUAL mode
*/

//-----------------------changelog END-----------------------

//Connections:
//DS18B20 Pinout (Left to Right, pins down, flat side toward you)
//- Left   = Ground
//- Center = Signal (Pin N of arduino):  (with 3.3K to 4.7K resistor to +5 or 3.3 )
//- Right  = +5 or +3.3 V   
//


//
// high volume scheme:        +---- +5V (12V not tested)
//                            |
//                       +----+
//                    1MOhm   piezo
//                       +----+
//                            |(C)
// pin -> 1.6 kOhms -> (B) 2n2222        < front here
//                            |(E)
//                            +--- GND
//

//MAX 485 voltage - 5V
//
// use resistor at RS-485 GND
// 1st test: 10k result lot of issues
// 2nd test: 1k, issues
// 3rd test: 100, see discussions

String fw_version = "1.10";
#ifdef A_MINI
	String hw_version = "1.6+";
#endif

#define DISP_ADJ_MENUS			0
#define DISP_ADJ_HOURS			1
#define DISP_ADJ_MINUTES		2
#define DISP_ADJ_DAYTEMP		3
#define DISP_ADJ_NIGHTTEMP		4


#ifdef A_MINI
	#define  but_left 		3	//buton pin for decrease/prev
	#define  but_mid 		A2 	//buton pin for ok/set
	#define  but_right 		2 	//buton pin for increase/next

	//external - to server
	#define Serial1_TxControl       5   	//RS485 Direction control DE and RE to this pin
	#define Serial1_RX        	6	//RX connected to RO - Receiver Output  
	#define Serial1_TX        	4	//TX connected to DI - Driver Output Pin
	
	//internal - to HP controller
	#define Serial2_TxControl       A1   	//RS485 Direction control DE and RE to this pin
	#define Serial2_RX        	A3	//RX connected to RO - Receiver Output  
	#define Serial2_TX        	A0	//TX connected to DI - Driver Output Pin
#endif

#ifdef A_MEGA
	#include <Adafruit_GFX.h>    	// Core graphics library
	#include "SWTFT.h" 		// Hardware-specific library, !!! slightly modified, upload to GitLab with this source !!!
	#include <TouchScreen.h>
	
	//external - to server
	#define Serial1_TxControl       A12   	//RS485 Direction control DE and RE to this pin
	#define Serial1_RX        	A13	//RX connected to RO - Receiver Output  
	#define Serial1_TX        	A11	//TX connected to DI - Driver Output Pin
	
	//internal - to HP controller
	#define Serial2_TxControl       A9   	//RS485 Direction control DE and RE to this pin
	#define Serial2_RX        	A10	//RX connected to RO - Receiver Output  
	#define Serial2_TX        	A8	//TX connected to DI - Driver Output Pin
	
#endif

#define RS485Transmit    	HIGH
#define RS485Receive     	LOW

#define speakerOut            	9

#ifdef A_MINI
	#include <DS3231.h>
	#include "LiquidCrystal_I2C.h"
	#include <Wire.h>
#endif
#ifdef WATCHDOG
	#include <avr/wdt.h>
#endif
#include <EEPROM.h>

#include <stdio.h>
#include <math.h>

#define SEED 	0xFFFF
#define POLY	0xA001
unsigned int 	crc16;
int 		cf;
#define MODBUS_MR 50	//50 ok now

const unsigned char regsToReq = 0x22;	//max reg N + 1 last was 0x1c now last 0x21

#include "SoftwareSerial.h"

#ifdef A_MINI
	LiquidCrystal_I2C lcd(i2c_DisplayAddr,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
	
	//RTClib 		rtc;
	//DateTime 	datetime;
	DS3231 		Clock;
	//char 		_char_temp[30];
	char		minute	= 0;
	char		hour	= 0;
	bool 		h12;
	bool 		PM;
	//bool adjust = false; // adjust clock mode yes/no
	//bool ahour, aminute, aday, amonth, ayear, rfin = false; // adjust hour, minute, day ... mode
#endif

#ifdef A_MEGA
	double Tci_prev = 0.0;
	double Tco_prev = 0.0;
	double Tbe_prev = 0.0;
	double Tae_prev = 0.0;
	double Tho_prev = 0.0;
	double Thi_prev = 0.0;
	double Tac_prev = 0.0;
	double Tbc_prev = 0.0;
	double Tcrc_prev = 0.0;
	double Ts1_prev = 0.0;
	double async_wattage_prev = -100.0;
	int EEV_cur_pos_prev = -10;
	
	char 	heatpump_state_prev    		= -1;
	char 	hotside_circle_state_prev  	= -1;
	char 	coldside_circle_state_prev 	= -1;
	char 	crc_heater_state_prev    	= -1; 
	
	char	HP_conn_state_prev 		= -1;
	
	char lastStopCauseTxt_prev[20];
	char lastStartMsgTxt_prev[20];
	
	int errorcode_prev			= -1;
	
	double disp_T_setpoint_night_prev 	= 0.0;
	
	/*bool heatpump_redraw    		= 0;
	bool hotside_circle_redraw  		= 0;
	bool coldside_circle_redraw 		= 0;
	bool crc_heater_state_prev    		= 0;	//!!!*/
	
	char		minute	= 0;
	char		hour	= 0;
	
	int           x1, y1, x2, y2;

	#define BACKGROUND      0x0020
	#define BLACK           0x0000      /*   0,   0,   0 */
	#define NAVY            0x000F      /*   0,   0, 128 */
	#define DARKGREEN       0x03E0      /*   0, 128,   0 */
	#define DARKCYAN        0x03EF      /*   0, 128, 128 */
	#define MAROON          0x7800      /* 128,   0,   0 */
	#define PURPLE          0x780F      /* 128,   0, 128 */
	#define OLIVE           0x7BE0      /* 128, 128,   0 */
	#define LIGHTGREY       0xC618      /* 192, 192, 192 */
	#define DARKGREY        0x7BEF      /* 128, 128, 128 */
	#define VERYDARKGREY	0x10A2
	#define BLUE            0x001F      /*   0,   0, 255 */
	#define LIGHTBLUE 	0xAEDC
	#define GREEN           0x07E0      /*   0, 255,   0 */
	#define CYAN            0x07FF      /*   0, 255, 255 */
	#define RED             0xF800      /* 255,   0,   0 */
	#define DARKRED 	0x7800
	#define MAGENTA         0xF81F      /* 255,   0, 255 */
	#define YELLOW          0xFFE0      /* 255, 255,   0 */
	#define WHITE           0xFFFF      /* 255, 255, 255 */
	#define ORANGE          0xFD20      /* 255, 165,   0 */
	#define GREENYELLOW     0xAFE5      /* 173, 255,  47 */
	#define PINK            0xF8C3
	#define VERYLIGHTRED	0xFC51
	#define VERYLIGHTBLUE	0xBDFF
	SWTFT tft;
	
	int curr_color = 0;
	int curr_color2 = 0;
	int curr_color3 = 0;
	
	int prec	= 1;	//precision
	
	#define YP A1  // must be an analog pin, use "An" notation!
	#define XM A2  // must be an analog pin, use "An" notation!
	#define YM 7   // can be a digital pin
	#define XP 6   // can be a digital pin

	#define TS_MINX 150
	#define TS_MINY 120
	#define TS_MAXX 920
	#define TS_MAXY 940

	// For better pressure precision, we need to know the resistance
	// between X+ and X- Use any multimeter to read it
	// For the one we're using, its 300 ohms across the X plate
	TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
	
	#define MINPRESSURE 10
	#define MAXPRESSURE 1000
	// Recently Point was renamed TSPoint in the TouchScreen library
	// If you are using an older version of the library, use the
	// commented definition instead.
	// Point p = ts.getPoint();
	TSPoint p;
	
	void drawPieSlice(int x, int y, int radius, int color, int startAngle, int EndAngle){
		for (int i=startAngle; i<EndAngle; i++){
			double radians = i * PI / 180;
			double px = x + radius * cos(radians);
			double py = y + radius * sin(radians);
			tft.drawPixel(px, py, color);
		}
	}
#endif

//------------------------ Modbus values, readed from HP --------------------------------
double Tci = 	-127.0;
double Tco = 	-127.0;
double Tbe = 	-127.0;
double Tae = 	-127.0;
double Tsg = 	-127.0;
double Tsl = 	-127.0;
double Tbv = 	-127.0;
double Tsuc = 	-127.0;
double Ts1 = 	-127.0;
double Ts2 = -127.0;	
double Tcrc = 	-127.0;
double Treg = 	-127.0;
double Tac = 	-127.0;
double Tbc = 	-127.0;
double Tho = 	-127.0;
double Thi = 	-127.0;

double  T_setpoint 		= 0.0;
double  T_EEV_setpoint 		= 0.0;

bool 	heatpump_state    	= 0;
bool 	hotside_circle_state  	= 0;
bool 	coldside_circle_state 	= 0;
bool 	crc_heater_state    	= 0;
bool 	reg_heater_state	= 0;
int 	EEV_cur_pos		= 0;
bool 	EEV_manual		= 0;		//todo: implement in stats get!!!!
int 	errorcode 		= 0;
double	async_wattage 		= 0;
//------------------------ Local values, used to drive HP --------------------------------
double  disp_T_setpoint_day 		= T_SETPOINT_DAY;
double  disp_T_setpoint_night 		= T_SETPOINT_NIGHT;
double  disp_T_EEV_setpoint 		= T_EEV_SETPOINT;

bool	disp_T_setpoint_sent		=	0;
bool	disp_T_EEV_setpoint_sent	=	0;

double	disp_T_setpoint_day_lastsaved	= disp_T_setpoint_day;
double	disp_T_setpoint_night_lastsaved	= disp_T_setpoint_night;
double  disp_T_EEV_setpoint_lastsaved	= disp_T_EEV_setpoint;
//------------------------  --------------------------------
const char	c_day_start_H 		= 06;
const char	c_day_start_M 		= 55;
const char	c_night_start_H 		= 23;
const char	c_night_start_M 		= 05;

const double cT_setpoint_max 		= T_SETPOINT_MAX;
const double cT_setpoint_min 		= T_SETPOINT_MIN;

#define RS485_1         0
#define RS485_2         1
SoftwareSerial 	RS485Serial_1(Serial1_RX, Serial1_TX); // RX, TX
SoftwareSerial 	RS485Serial_2(Serial2_RX, Serial2_TX); // RX, TX

SoftwareSerial * multiSerial[2] = { &RS485Serial_1, &RS485Serial_2 };
char iface       = RS485_1;

#ifdef wifi
	#define SerialWIFI_RX        	11	//RX connected to RO - Receiver Output  
	#define SerialWIFI_TX        	8	//TX connected to DI - Driver Output Pin
	SoftwareSerial 	SerialWIFI(SerialWIFI_RX, SerialWIFI_TX); // RX, TX
	String _espspeed = "AT+UART_CUR=9600,8,1,0,0\r\n";
	String _espdefspeed = "AT+UART_DEF=115200,8,1,0,0\r\n"; 
	//String _espspeed = "AT+UART=9600\r\n";
	bool wifierror = 0;
#endif
//main cycle vars
unsigned long 	millis_prev	= 0;
unsigned long 	millis_now	= 0;
unsigned long 	millis_cycle	= 50;

unsigned long 	millis_lasteesave	=  0;  

#ifdef A_MINI
	unsigned long millis_modbus03_cycle	= 3500;
#endif
#ifdef A_MEGA
	unsigned long millis_modbus03_cycle	= 3500;
#endif

unsigned long millis_modbus03_prev	= 0;

unsigned char sequential_errors = 0;

bool deb_mod = 0;

#define MOD_READY	0
#define MOD_03_SENT	1

unsigned char mod_485_2_state = 0;

unsigned long tmic1    =  0;
unsigned long tmic2    =  0;

const unsigned long 	c_resp_timeout   = 2500;  //milliseconds
unsigned long 		time_sent      =  0;

#define BUFSIZE 	90
unsigned char 	dataBuf[BUFSIZE+1];	// Allocate some space for the string
char 	inChar= -1;      	// space to store the character read
byte 	index = 0;		// Index into array; where to store the character

unsigned char	HP_conn_state = 0;	//0 == connection lost 1 == connection OK
unsigned char	disp_adj_state = DISP_ADJ_MENUS;

//-------------EEPROM
int eeprom_magic_read = 0x00;
int eeprom_addr       = 0x00;   
const int eeprom_magic = MAGIC;

//-------------temporary variables
char temp[10];
int i	= 0;
int u	= 0;
int z	= 0;
int x	= 0;
int y	= 0;
double tempdouble	= 0.0;
double tempdouble_intpart	= 0.0;

int tempint       	= 0;
char tempchar		= 0;

char		fp_integer	= 0;
char		fp_fraction	= 0;

char convBuf[13];

char lastStopCauseTxt[20];
char lastStartMsgTxt[20];

String outString;
String outString_S2;
//----------------------------------------------------------

byte custom_DayChar[] = {
  B00000,
  B00000,
  B00101,
  B00101,
  B11100,
  B10101,
  B11101,
  B00000
};

byte custom_NightChar[] = {
  B00000,
  B00000,
  B11001,
  B10101,
  B10100,
  B10101,
  B10101,
  B00000
};

//---------------------------memory debug
#ifdef __arm__
	// should use uinstd.h to define sbrk but Due causes a conflict
	extern "C" char* sbrk(int incr);
#else // __ARM__
	extern char *__brkval;
#endif // __arm__

int freeMemory() {
	char top;
	#ifdef __arm__
		return &top - reinterpret_cast<char*>(sbrk(0));
	#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
		return &top - __brkval;
	#else // __arm__
		return __brkval ? &top - __brkval : &top - __malloc_heap_start;
	#endif // __arm__
}

void Calc_CRC(unsigned char b) {	//uses/changes y
	crc16 ^= b & 0xFF;
	for (y=0; y<8; y++) {
		cf = crc16 & 0x0001;
		crc16>>=1;
		if (cf) { crc16 ^= POLY; }
	}
}

char Check_CRC() {	//uses/changes x
	if (index < 3) { 
		Serial.println(F("Mod.ShortMsg"));
		return 1;
	}
	crc16 = SEED;
	for (x = 0; x < (index-2); x++) {
		Calc_CRC(dataBuf[x]); 
	}
	if (( dataBuf[index - 2] !=  (crc16 & 0xFF )) || ( dataBuf[index - 1] != (crc16 >> 8))) {
		Serial.println(F("Mod.MsgCRCerr"));
		return 1;
		
	}
	return 0;
}

void CalcAdd_BufCRC(){
	crc16 = SEED;
	for (x = 0; x < i; x++) {   
		Calc_CRC(dataBuf[x]); 
	}
	dataBuf[i] = crc16 & 0xFF;
	i++;
	dataBuf[i] = crc16 >> 8;
	i++;
}

void Add_Double_To_Buf_IntFract (double float_to_convert) {	//uses tempdouble tempdouble_intpart fp_integer fp_fraction
	if (float_to_convert > 255.0 || float_to_convert < -127.0) {
		fp_integer	= -127;
		fp_fraction	= 0;
	} else {
		tempdouble = modf (float_to_convert , &tempdouble_intpart);
		fp_integer = trunc(tempdouble_intpart);
		tempdouble  = tempdouble * 100;
		fp_fraction = round(tempdouble);
	}
	dataBuf[i]  =  fp_integer;
	i++;
	dataBuf[i]  =  fp_fraction;
	i++;
	/*Serial.println(float_to_convert);
	Serial.println(fp_integer, DEC);
	Serial.println(fp_fraction, DEC);*/
}


void IntFract_to_tempdouble (char _int_to_convert, char _fract_to_convert) {	//fract is also signed now!
	tempdouble = (double) _fract_to_convert / 100;
	tempdouble += _int_to_convert;
	/*
	Serial.println(_int_to_convert);
	Serial.println(_fract_to_convert);
	Serial.println(tempdouble);*/
}

char ReadModbusMsg(){	//uses index, z
	//Serial.println("ReadModbusMsg");
	index = 0;
	z = 0;  //error flag
	while ( 1 == 1 ) {//9600
		//read
		//Serial.println("-");
		if (multiSerial[iface]->available()) {
			if(index < BUFSIZE) {
				inChar = multiSerial[iface]->read();
				//Serial.print(inChar, HEX);
				//Serial.print(" ");
				dataBuf[index] = inChar;      	
				index++;                   	
				dataBuf[index] = '\0';        	
				delayMicroseconds(80);       	//yep, 80, HERE
			} else {
				z = 1;
				while (multiSerial[iface]->available()) {
					inChar = multiSerial[iface]->read();
					delayMicroseconds(1800);
				}
				break;
			}
		} else {
			tmic1 = micros();
			for (i = 0; i < 10; i++) {    
				delayMicroseconds(180);
				if (multiSerial[iface]->available()){
					//Serial.print("babaika");
					//Serial.println(i);
					tmic2 = micros();
					break;
				}
				tmic2 = micros();
				if ( (unsigned long)(tmic2 - tmic1) > 1800 ) {
					i = 10;
					break;
				}
			}
			if (i == 10 && multiSerial[iface]->available()) {
				z = 2;
				i = 0;
				while (multiSerial[iface]->available()) {
					if (i > 200){ 
						break; 
					}
					inChar = multiSerial[iface]->read();
					delayMicroseconds(1800);
					i++;
				}
				break;
			} else if (!multiSerial[iface]->available()) {
				break;
			} else if (multiSerial[iface]->available()) {
				continue;
			} else {
				Serial.println(F("e2245"));
			}
		}
	}	
	return z;
}
#ifdef A_MINI
	void Print_D1 () {
		lcd.setCursor(0, 0);
		//lcd.backlight();     
		//lcd.clear();
		for (i=outString.length(); i<16; i++){
			outString += " ";
		}
		lcd.print(outString);
	}



	void Print_D2 () {
		for (i=outString.length(); i<16; i++){
			outString += " ";
		}
		lcd.setCursor(0, 1);
		lcd.print(outString);
	}
#endif

#ifdef A_MEGA
	void set_precision ( double tocheck ) {
		if ( (tocheck > 99.9) || (tocheck < -9.9) ) {
			prec = 0;
		} else {
			prec = 1;
		}
	}
#endif

void WriteFloatEEPROM(int addr, float val) { 
	byte *x = (byte *)&val;
	for(byte u = 0; u < 4; u++) EEPROM.write(u+addr, x[u]);
}

float ReadFloatEEPROM(int addr) {   
	byte x[4];
	for(byte u = 0; u < 4; u++) x[u] = EEPROM.read(u+addr);
	float *y = (float *)&x;
	return y[0];
}

void SaveDayNightEE(void) {
	if(  	(disp_T_setpoint_day_lastsaved != disp_T_setpoint_day) && 
		( ((unsigned long)(millis_now - millis_lasteesave) > 15*60*1000 )  ||  (millis_lasteesave == 0) )  ) {
			Serial.println(F("EE_Day"));
			eeprom_addr = 1;
			WriteFloatEEPROM(eeprom_addr, disp_T_setpoint_day);
			millis_lasteesave = millis_now;
			disp_T_setpoint_day_lastsaved = disp_T_setpoint_day;
	}
	if(  	(disp_T_setpoint_night_lastsaved != disp_T_setpoint_night) && 
		( ((unsigned long)(millis_now - millis_lasteesave) > 15*60*1000 )  ||  (millis_lasteesave == 0) )  ) {
			Serial.println(F("EE_Night"));
			eeprom_addr = 5;
			WriteFloatEEPROM(eeprom_addr, disp_T_setpoint_night);
			millis_lasteesave = millis_now;
			disp_T_setpoint_night_lastsaved = disp_T_setpoint_night;
	}
	if(  	(disp_T_EEV_setpoint_lastsaved != disp_T_EEV_setpoint) && 
		( ((unsigned long)(millis_now - millis_lasteesave) > 15*60*1000 )  ||  (millis_lasteesave == 0) )  ) {
			Serial.println(F("EE_EEV"));
			eeprom_addr = 9;
			WriteFloatEEPROM(eeprom_addr, disp_T_EEV_setpoint);
			millis_lasteesave = millis_now;
			disp_T_EEV_setpoint_lastsaved = disp_T_EEV_setpoint;
	}
}

#ifdef wifi
	void Read_WIFI (bool _readln = 0, bool _longrun = 0) {	//saves data to dataBuf, counter to index, cuts all > 49 symbols
		index = 0;
		if (SerialWIFI.available() > 0) {
			//Serial.println("something on WiFi..");	//!!!debug
			i = 0;
			while (SerialWIFI.available() > 0) { // Don't read unless you know there is data
				if(index < (BUFSIZE)) {   //  size of the array minus 1
					inChar = SerialWIFI.read(); 	// Read a character
					//Serial.print("-");
					//Serial.print(inChar);
					//Serial.print(inChar, HEX);
					dataBuf[index] = inChar;      	// Store it
					index++;                     	// Increment where to write next
					dataBuf[index] = '\0';        	// clear next symbol, null terminate the string
					if (_readln == 1 && dataBuf[index-2] == 0x0D && dataBuf[index-1] == 0x0A){
						Serial.print("GotLn:");
						Serial.flush();
						Serial.write(dataBuf,index);
						/*Serial.print(dataBuf[index-1-2], HEX);
						Serial.print(dataBuf[index-1-1], HEX);*/
						return;
					}
					delayMicroseconds(80);       	//80 microseconds - the best choice at 9600, "no answer"disappeared
									//40(20??) microseconds seems to be good, 9600, 49 symbols
									//
				} else {            //too long message! read it to nowhere
					if ( ((_longrun == 1) && (i > 10000)) || ((_longrun == 0) && (i > 100)) ){
						break;
					}
					inChar = SerialWIFI.read();
					//!!!
					//Serial.print("+");
					//Serial.print(inChar);
					delayMicroseconds(80);
					i++;
				}
			}
		}
		if (index > 0){
			Serial.print("Got:");
			Serial.print(index);
			Serial.flush();
			Serial.write(dataBuf,index);
		}
	}
	void Read_WIFI_ln(){
		Read_WIFI(1);
	}
	
	bool check_AT_OK() {
		//Serial.println(index);
		if ( index > BUFSIZE || index < 4  || dataBuf[index-1-3] != 0x4F || dataBuf[index-1-2] != 0x4B ) {
			return 0; 
		} else {
			return 1;
		}
	}
	
	void wait_AT_OK() {
		index = 0;
		z = 0;
		while (!check_AT_OK()){
			if (z > 20000){
				wifierror = 1;
				break;
			}
			delay(1);
			index = 0;
			Read_WIFI_ln();
			//check_AT_OK();
			z++;
		}
	}
	
	void set_WiFi_9600() {	//set 9600 or wifierror flag
		SerialWIFI.print(F("AT\r\n"));
		delay(200);
		Read_WIFI();
		z = 0;
		while ( !check_AT_OK() ) {
			Serial.println(F("WiFi set 9600"));
			if (z > 200){
				wifierror = 1;
				break;
			}
			SerialWIFI.begin(57600);
			cli();
			SerialWIFI.print(_espspeed);
			SerialWIFI.flush();
			sei();
			delay(250);
			Read_WIFI();
			SerialWIFI.begin(115200);
			cli();
			SerialWIFI.print(_espspeed);
			SerialWIFI.flush();
			sei();
			delay(250);
			Read_WIFI();
			SerialWIFI.begin(9600);
			SerialWIFI.print(_espspeed);
			delay(250);
			Read_WIFI();
			SerialWIFI.print(F("AT\r\n"));
			delay(200);
			Read_WIFI();
			z++;
		}
	}
#endif

void setup() { 
	#ifdef WATCHDOG
		wdt_disable();
	#endif
	delay(2000);
	Serial.begin(9600);
	Serial.print(F("Starting, dev_id:"));
	Serial.println(devID);
	#ifdef A_MINI
		Serial.println(F("Valden Remote Display, https://github.com/OpenHP/Display/"));
	#endif
	#ifdef A_MEGA
		Serial.println(F("Valden Service Display, https://github.com/OpenHP/ServiceDisplay/"));
	#endif
	iface = RS485_1;
	multiSerial[iface]->begin(9600);
	iface = RS485_2;
	multiSerial[iface]->begin(9600);
	#ifdef wifi
		//tests only
		SerialWIFI.begin(9600);
		SerialWIFI.listen();
		Read_WIFI(0, 1);

		set_WiFi_9600();
		
		SerialWIFI.print(F("AT+CWMODE_CUR=2\r\n")); 
		wait_AT_OK();
		delay(10000);
		Read_WIFI_ln();
		//cnange SSID and MAC
		SerialWIFI.print("AT+CIPSTART=0,\"UDP\",\"192.168.0.140\",4445,4445,2\r\n");
		while (1 == 1){
			for (x = 10; x < 99; x++){
				SerialWIFI.print("AT+CWSAP_CUR=\"" + String(x) + "\",\"1\","+ String(random(6,6)) + ",0\r\n");
				SerialWIFI.flush();
				wait_AT_OK();
				SerialWIFI.print("AT+CIPAPMAC_CUR=\"00:00:00:00:00:" + String(x) +"\"\r\n");
				SerialWIFI.flush();
				wait_AT_OK();
				delay(5);
			}
		}
		
		//client
		SerialWIFI.print(F("AT+CWMODE_CUR=1\r\n")); 
		wait_AT_OK();
		
		//scan APs example
		SerialWIFI.print(F("AT+CWLAP\r\n"));
		index = 0;
		z = 0;
		while ( !check_AT_OK() ){
			index = 0;
			delay(1);
			Read_WIFI_ln();
			z++;
			if (z > 10000){
				wifierror = 1;
				break;
			}
		}
		//connect to ap example
		SerialWIFI.print("AT+CWJAP_CUR=\""+String(wifi_ap)+"\",\""+String(wifi_password)+"\"\r\n");
		delay(200);
		wait_AT_OK();
		
		SerialWIFI.print("AT+CIPSTA_CUR?\r\n");	//get current IP
		//?!?! incompeted answer for unknown reason
		delay(200);
		wait_AT_OK();
		
		//server start example
		SerialWIFI.print("AT+CIPMUX=1\r\n");
		delay(200);
		wait_AT_OK();
		
		SerialWIFI.print("AT+CIPSERVER=0\r\n");
		delay(200);
		wait_AT_OK();
		
		SerialWIFI.print("AT+CIPSERVER=1,666\r\n");
		delay(200);
		wait_AT_OK();
		
		//client example
		/*SerialWIFI.print("AT+CIPSEND=0,7,\"192.168.0.140\",4445\r\n");
		SerialWIFI.flush();
		delay(50);
		SerialWIFI.print("hello\r\n");
		SerialWIFI.flush();
		wait_AT_OK();
		*/
		
	#endif 
	//
	eeprom_magic_read = EEPROM.read(eeprom_addr);
	eeprom_addr += 1;
	
	if (eeprom_magic_read == eeprom_magic){
		Serial.println(F("EEPROM->mem"));
	} else {
		Serial.println(F("mem->EEPROM"));
		WriteFloatEEPROM(eeprom_addr, disp_T_setpoint_day);
		eeprom_addr += 4;
		WriteFloatEEPROM(eeprom_addr, disp_T_setpoint_night);
		eeprom_addr += 4;
		WriteFloatEEPROM(eeprom_addr, disp_T_EEV_setpoint);
		EEPROM.write(0x00, eeprom_magic);
		eeprom_addr = 1;
	}
	disp_T_setpoint_day = ReadFloatEEPROM(eeprom_addr);
	eeprom_addr += 4;
	disp_T_setpoint_night = ReadFloatEEPROM(eeprom_addr);
	eeprom_addr += 4;
	disp_T_EEV_setpoint = ReadFloatEEPROM(eeprom_addr);
	//eeprom_addr += 4;
	
	disp_T_setpoint_day_lastsaved = disp_T_setpoint_day;
	disp_T_setpoint_night_lastsaved = disp_T_setpoint_night;
	disp_T_EEV_setpoint_lastsaved = disp_T_EEV_setpoint;
	#ifdef A_MINI
		lcd.init();            
		lcd.noBacklight();     
		delay(300);
		lcd.backlight();  
		lcd.createChar(0x01, custom_DayChar);	
		lcd.createChar(0x02, custom_NightChar);	
	
		pinMode (but_left, 	INPUT);
		pinMode (but_mid, 	INPUT);	
		pinMode (but_right, 	INPUT);	
		outString = F("ValdenDisplay");
		Print_D1();
		outString = F("starting...");
		Print_D2();
		delay(2500);
		outString = "ID: 0x" + String(devID, HEX);
		Print_D1();
		outString = "Fw:" + fw_version + " HW:" + hw_version;
		Print_D2();
		delay(2500);
		
		Wire.begin(); //clock
		
		//test
		//BAD way, not working
		/*while ( 1 == 1 ){
			Wire.beginTransmission (i2c_dsAddr);
			Wire.write(0x0F); //Control/Status Register to check OSF flag
			Wire.endTransmission();

			Wire.requestFrom(i2c_dsAddr, 1);
			tempchar = Wire.read();
			if((tempchar & 0x80) != 0x80)  { 
				break;
			} else {
				outString = F("ERR: no RTC");
				Print_D1();
				outString = F("");
				Print_D2();
				Serial.println("No RTC found! check display hardware!");
				tone(speakerOut, 2250);
				delay (500);
				noTone(speakerOut);
				delay (5000);
			}
		}*/
		
		Clock.setClockMode(false);	//24h
	#endif
	#ifdef A_MEGA
		tft.reset();
		uint16_t identifier = tft.readID();
		Serial.print(F("LCD driver chip: "));
		Serial.println(identifier, HEX);
		
		tft.begin(identifier);
		tft.setRotation(1);
		tft.fillScreen(BACKGROUND);
		delay(100);
		tft.fillScreen(BACKGROUND);
		//defaults
		tft.setTextColor(YELLOW); 
		tft.setTextSize(2);
		
		
		//creds
		tft.setCursor(1, 50);
		tft.println(F("Valden Service Display"));
		tft.setCursor(1, 100);
		tft.println(F("https://github.com/OpenHP/"));
		tft.setCursor(1, 150);
		tft.println("Firmware: v" + fw_version);
		delay(5000);
		
		tft.fillScreen(BACKGROUND);
		delay(100);
		tft.fillScreen(BACKGROUND);
		
		
		//buttons
		//
		#define buttons_x 80
		#define buttons_y 195
		#define buttons_xsize 75
		#define buttons_ysize 45
		#define buttons_sep 10
		
		tft.fillRoundRect(buttons_x, 					buttons_y, 	buttons_xsize, buttons_ysize, 5, VERYDARKGREY);
		tft.fillRoundRect(buttons_x + buttons_xsize + buttons_sep, 	buttons_y, 	buttons_xsize, buttons_ysize, 5, VERYDARKGREY);
		
		tft.drawRoundRect(buttons_x, 					buttons_y, 	buttons_xsize, buttons_ysize, 5, LIGHTGREY);
		tft.drawRoundRect(buttons_x + buttons_xsize + buttons_sep, 	buttons_y, 	buttons_xsize, buttons_ysize, 5, LIGHTGREY);
		//tft.drawRoundRect(buttons_x + buttons_xsize*2 + buttons_sep*2, 	buttons_y, 	buttons_xsize, buttons_ysize, 5, DARKGREY);
		
		tft.setTextColor(ORANGE); 
		tft.setTextSize(3);
		tft.setCursor(110, 207);
		tft.println("-");
		tft.setCursor(195, 207);
		tft.println("+");
		tft.setTextColor(YELLOW); 
		tft.setTextSize(2);
		

		
	#endif
	pinMode(Serial1_TxControl, OUTPUT);    
	digitalWrite(Serial1_TxControl, RS485Receive);
	pinMode(Serial2_TxControl, OUTPUT);    
	digitalWrite(Serial2_TxControl, RS485Receive);
	
	#ifdef WATCHDOG
		wdt_enable (WDTO_8S);
	#endif
	outString.reserve(120);
	outString_S2.reserve(10);
	
	tone(speakerOut, 2250);
	delay (50);
	noTone(speakerOut);
	//Serial.println(String(freeMemory()));
	
}

void loop() { 
	#ifdef WATCHDOG
		wdt_reset();
	#endif
	millis_now = millis();
	digitalWrite(Serial1_TxControl, RS485Receive);
	digitalWrite(Serial2_TxControl, RS485Receive);
	SaveDayNightEE();
	//Serial.println(mod_485_2_state);
	if ( mod_485_2_state == MOD_03_SENT ) {
		if (iface != RS485_2){ 
			iface = RS485_2;
			multiSerial[iface]->listen();
			//Serial.println(F("Set485_2"));
		}
	} else {
		if (iface != RS485_1){ 
			iface = RS485_1;
			multiSerial[iface]->listen();
			//Serial.println(F("Set485_1"));
		}
	}
	//--------------------------------- display ---------------------------------------------
	#ifdef A_MINI
		//datetime = rtc.now();
		//hour = datetime.hour();
		//minute = datetime.minute();
		hour = Clock.getHour(h12, PM);
		minute = Clock.getMinute();
		outString = "";
		//outString += "Time: ";
		if(hour<10) {	outString += F("0");	}
		outString += String(hour,DEC);
		outString += ":";
		if(minute<10) {	outString += F("0");	}
		outString += String(minute,DEC);
		/*outString += ":";
		if(datetime.second()<10) {	outString += "0";	}
		outString += datetime.second();
		outString += "  ";*/
		outString += F(" \x01");
		outString += String(disp_T_setpoint_day,1);
		outString += F("\x02");
		outString += String(disp_T_setpoint_night,1);
		Print_D1();
	
	
		/*
		#define DISP_ADJ_MENUS			1
		#define DISP_ADJ_HOURS			2
		#define DISP_ADJ_MINUTES		3
		#define DISP_ADJ_DAYTEMP		4
		#define DISP_ADJ_NIGHTTEMP		5
		*/
		
		if (disp_adj_state == DISP_ADJ_MENUS) {
			lcd.noCursor();
			
			outString = "";
			if (HP_conn_state == 0){
				outString += F("NO CONNECTION");
			} else {
				if (Ts1 != -127.0) {
					outString += "W:" + String(Ts1,2);
				} else if (Tho != -127.0) {
					outString += "F:" + String(Tho,2);
				} else {
					outString += "Terr" + String(Tho,2);
				}
				outString += F(" P:");
				if (heatpump_state != 0) {
					outString += F("Run");
				} else {
					if (errorcode != 0) {
						outString += "Err" + String(errorcode);
					} else if (hotside_circle_state == 0) {
						outString += F("Stop");
					} else {
						outString += F("Chkg");
					}
				}
			}
			Print_D2();
		} else {
			lcd.cursor();
			outString = "";
			Print_D2();
			if (disp_adj_state == DISP_ADJ_MENUS) {
				lcd.noCursor();
			}
			if (disp_adj_state == DISP_ADJ_HOURS) {
				lcd.setCursor(1,0);
			}
			if (disp_adj_state == DISP_ADJ_MINUTES) {
				lcd.setCursor(4,0);
			}
			if (disp_adj_state == DISP_ADJ_DAYTEMP) {
				lcd.setCursor(10,0);
			}
			if (disp_adj_state == DISP_ADJ_NIGHTTEMP) {
				lcd.setCursor(15,0);
			}
		}
		/*outString += datetime.year();
		outString += "/";
		if(datetime.month()<10) {	outString += "0";	}
		outString += datetime.month();
		outString += "/";
		if(datetime.day()<10) {		outString += "0";	}
		outString += datetime.day();*/
		
		delay(100);
		if (digitalRead(but_left)){
			if (disp_adj_state == DISP_ADJ_MENUS) {
				if (digitalRead(but_right)) {
					disp_adj_state = DISP_ADJ_HOURS;
				}
			} else if (disp_adj_state == DISP_ADJ_HOURS) {
				hour -= 1;
				if (hour == -1) hour = 23;
				Clock.setHour(hour);
			} else if (disp_adj_state == DISP_ADJ_MINUTES) {
				minute -= 1;
				if (minute == -1) minute = 59;
				Clock.setMinute(minute);
			} else if (disp_adj_state == DISP_ADJ_DAYTEMP) {
				disp_T_setpoint_day -= 0.5;
				if (disp_T_setpoint_day < cT_setpoint_min) disp_T_setpoint_day = cT_setpoint_min;
			} else if (disp_adj_state == DISP_ADJ_NIGHTTEMP) {
				disp_T_setpoint_night -= 0.5;
				if (disp_T_setpoint_night < cT_setpoint_min) disp_T_setpoint_night = cT_setpoint_min;
			}
			delay(50);
		}
		if (digitalRead(but_mid)){
			if (disp_adj_state == DISP_ADJ_MENUS) {
				disp_adj_state = DISP_ADJ_DAYTEMP;
			} else if (disp_adj_state == DISP_ADJ_HOURS) {
				disp_adj_state = DISP_ADJ_MINUTES;
			} else if (disp_adj_state == DISP_ADJ_MINUTES) {
				disp_adj_state = DISP_ADJ_MENUS;
			} else if (disp_adj_state == DISP_ADJ_DAYTEMP) {
				disp_adj_state = DISP_ADJ_NIGHTTEMP;
			} else if (disp_adj_state == DISP_ADJ_NIGHTTEMP) {
				disp_adj_state = DISP_ADJ_MENUS;
			}
			delay(50);
		}
		if (digitalRead(but_right)){
			if (disp_adj_state == DISP_ADJ_MENUS) {
				if (digitalRead(but_left)){
					disp_adj_state = DISP_ADJ_HOURS;
				}
			} else if (disp_adj_state == DISP_ADJ_HOURS) {
				hour += 1;
				if (hour == 24) hour = 0;
				Clock.setHour(hour);
			} else if (disp_adj_state == DISP_ADJ_MINUTES) {
				minute += 1;
				if (minute == 60) minute = 0;
				Clock.setMinute(minute);
			} else if (disp_adj_state == DISP_ADJ_DAYTEMP) {
				disp_T_setpoint_day += 0.5;
				if (disp_T_setpoint_day > cT_setpoint_max) disp_T_setpoint_day = cT_setpoint_max;
			} else if (disp_adj_state == DISP_ADJ_NIGHTTEMP) {
				disp_T_setpoint_night += 0.5;
				if (disp_T_setpoint_night > cT_setpoint_max) disp_T_setpoint_night = cT_setpoint_max;
			}
			delay(50);
		}
	#endif
	
	#ifdef A_MEGA
		digitalWrite(13, HIGH);
		p = ts.getPoint();
		digitalWrite(13, LOW);
		
		// if sharing pins, you'll need to fix the directions of the touchscreen pins
		pinMode(XM, OUTPUT);
		pinMode(YP, OUTPUT);
		
		if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
			//Serial.print("X = "); Serial.print(p.x);
			//Serial.print("\tY = "); Serial.print(p.y);
			//Serial.print("\tPressure = "); Serial.println(p.z);
			if ( p.y > 240 && p.y < 480 && p.x < 320 ){ //-
				disp_T_setpoint_night -= 0.5;
				if (disp_T_setpoint_night < cT_setpoint_min) disp_T_setpoint_night = cT_setpoint_min;
			}
			if ( p.y > 510 && p.y < 720 && p.x < 320 ){ //+
				disp_T_setpoint_night += 0.5;
				if (disp_T_setpoint_night > cT_setpoint_max) disp_T_setpoint_night = cT_setpoint_max;
			}
		}
	
		#define center_x 160
		#define base_y 36
		
		//cold circle
		#define coldcircle_x center_x - 130
		#define coldcircle_y base_y + 82
		#define circle_radius 60
		#define circle_pipes_ydiff 51
		#define circle_pipes_len 61
		
		//clean "no connection" text at start (if connection appeared), but print at the end of proc
		if ( HP_conn_state == 1 && HP_conn_state_prev != HP_conn_state ) {
			tft.fillRect(35, 18, 2,14, BACKGROUND);
			tft.fillRect(35, 18, 235,14, BACKGROUND);
		}
		
		if (coldside_circle_state_prev != coldside_circle_state || heatpump_state_prev != heatpump_state || HP_conn_state_prev != HP_conn_state) {
			if (heatpump_state == 1) {
				curr_color = WHITE;
			} else {
				curr_color = DARKGREY;
			}

			if (coldside_circle_state == 1){
				curr_color2 = CYAN;
				curr_color3 = BLUE;
			} else {
				curr_color2 = VERYLIGHTBLUE;
				curr_color3 = VERYLIGHTBLUE;
			}
			if (HP_conn_state == 0) {
				curr_color = DARKGREY;
				curr_color2 = DARKGREY;
				curr_color3 = DARKGREY;
			}
				
			drawPieSlice(coldcircle_x,coldcircle_y,circle_radius,curr_color2,300,359);
			drawPieSlice(coldcircle_x,coldcircle_y,circle_radius,curr_color3,0,60);
			drawPieSlice(coldcircle_x,coldcircle_y,circle_radius+1,curr_color2,300,359);
			drawPieSlice(coldcircle_x,coldcircle_y,circle_radius+1,curr_color3,0,60);
			drawPieSlice(coldcircle_x,coldcircle_y,circle_radius+2,curr_color2,300,359);
			drawPieSlice(coldcircle_x,coldcircle_y,circle_radius+2,curr_color3,0,60);
			
			if (coldside_circle_state == 1){
				tft.fillTriangle(0, coldcircle_y+circle_pipes_ydiff,  		10,coldcircle_y+circle_pipes_ydiff-7,		10,coldcircle_y+circle_pipes_ydiff+8,	 curr_color3);	
			} else {
				tft.fillTriangle(0, coldcircle_y+circle_pipes_ydiff,  		10,coldcircle_y+circle_pipes_ydiff-7,		10,coldcircle_y+circle_pipes_ydiff+8,	 BACKGROUND);		
				tft.drawTriangle(0, coldcircle_y+circle_pipes_ydiff,  		10,coldcircle_y+circle_pipes_ydiff-7,		10,coldcircle_y+circle_pipes_ydiff+8,	 BACKGROUND);		//to prevent artefacts
			}
			
			tft.drawFastHLine(0, coldcircle_y-circle_pipes_ydiff-2, circle_pipes_len, curr_color2);
			tft.drawFastHLine(0, coldcircle_y-circle_pipes_ydiff-1, circle_pipes_len, curr_color2);
			tft.drawFastHLine(0, coldcircle_y-circle_pipes_ydiff, circle_pipes_len, curr_color2);
			
			tft.drawFastHLine(0, coldcircle_y+circle_pipes_ydiff, circle_pipes_len, curr_color3);
			tft.drawFastHLine(0, coldcircle_y+circle_pipes_ydiff+1, circle_pipes_len, curr_color3);
			tft.drawFastHLine(0, coldcircle_y+circle_pipes_ydiff+2, circle_pipes_len, curr_color3);
			
			
			//cold heat exchanger
			#define coldx center_x - 80
			#define coldy base_y + 52
			#define coldxsize 20
			#define coldysize 50
			#define coldystep 10
			#define coldpieces 5
			#define cold_pipelen 20
			y=coldy;
			tft.drawFastVLine(coldx + coldxsize/2-1, 	y-cold_pipelen, cold_pipelen-2, curr_color);
			tft.drawFastVLine(coldx + coldxsize/2, 		y-cold_pipelen, cold_pipelen-2, curr_color);
			tft.drawFastVLine(coldx + coldxsize/2+1, 	y-cold_pipelen, cold_pipelen-2, curr_color);
			
			tft.drawLine(coldx+coldxsize/2, y-(coldystep/2), coldx, y, curr_color);
			tft.drawLine(coldx+coldxsize/2, y-(coldystep/2)+1, coldx, y+1, curr_color);
			tft.drawLine(coldx+coldxsize/2, y-(coldystep/2)+2, coldx, y+2, curr_color);
			
			for(i = 0; i < coldpieces; i++){
				tft.drawFastHLine(coldx, y, coldxsize, curr_color);
				tft.drawFastHLine(coldx, y+1, coldxsize, curr_color);
				tft.drawFastHLine(coldx, y+2, coldxsize, curr_color);
				tft.drawLine(coldx+coldxsize, y, coldx, y+coldystep, curr_color);
				tft.drawLine(coldx+coldxsize, y+1, coldx, y+coldystep+1, curr_color);
				tft.drawLine(coldx+coldxsize, y+2, coldx, y+coldystep+2, curr_color);
				y+=coldystep;
			}
			tft.drawFastHLine(coldx, y, coldxsize/2, curr_color);
			tft.drawFastHLine(coldx, y+1, coldxsize/2, curr_color);
			tft.drawFastHLine(coldx, y+2, coldxsize/2, curr_color);
			tft.drawFastVLine(coldx + coldxsize/2-1, 	y, cold_pipelen, curr_color);
			tft.drawFastVLine(coldx + coldxsize/2, 		y, cold_pipelen, curr_color);
			tft.drawFastVLine(coldx + coldxsize/2+1, 	y, cold_pipelen, curr_color);
		}
	
		//compressor
		#define compressor_x center_x
		#define compressor_y base_y + 32
		#define compressor_radius 28
		if ( heatpump_state_prev != heatpump_state || HP_conn_state_prev != HP_conn_state ){
			if (heatpump_state == 1) {
				curr_color = WHITE;
			} else {
				curr_color = DARKGREY;
			}
			if (HP_conn_state == 0) {
				curr_color = DARKGREY;
			}
			
			tft.drawCircle(compressor_x, compressor_y, compressor_radius, curr_color);
			tft.drawCircle(compressor_x, compressor_y, compressor_radius+1, curr_color);
			tft.drawCircle(compressor_x, compressor_y, compressor_radius+2, curr_color);
			tft.drawFastVLine(compressor_x - (compressor_radius/2.5), compressor_y - (compressor_radius/1.5), compressor_radius*1.4, curr_color);
			tft.drawFastVLine(compressor_x + (compressor_radius/1.5), compressor_y - (compressor_radius/4), compressor_radius/2, curr_color);
			tft.drawLine(compressor_x - (compressor_radius/2.5), compressor_y - (compressor_radius/1.5), 				compressor_x + (compressor_radius/1.5), compressor_y - (compressor_radius/4), 				curr_color);
			tft.drawLine(compressor_x - (compressor_radius/2.5), compressor_y - (compressor_radius/1.5) + compressor_radius*1.4, 	compressor_x + (compressor_radius/1.5), compressor_y - (compressor_radius/4) + compressor_radius/2, 	curr_color);
			//power line
			/*
			tft.drawFastVLine(compressor_x-1, 0, compressor_radius*1.5, YELLOW);
			tft.drawFastVLine(compressor_x, 0, compressor_radius*1.5, YELLOW);
			tft.drawFastVLine(compressor_x+1, 0, compressor_radius*1.5, YELLOW);
			tft.fillTriangle(compressor_x,	compressor_y-compressor_radius*1.2,  		compressor_x + compressor_radius/5,compressor_y- compressor_radius*1.6,		compressor_x - compressor_radius/5,compressor_y- compressor_radius*1.6,	 YELLOW);
			*/
			//pipes
			#define compressor_pipeslen 40
			if (heatpump_state == 1) {
				tft.fillTriangle(compressor_x - compressor_radius - 3, compressor_y,  		compressor_x - compressor_radius - 13,compressor_y- 7,		compressor_x - compressor_radius - 13,compressor_y + 8,	 curr_color);	
			} else {
				tft.fillTriangle(compressor_x - compressor_radius - 3, compressor_y,  		compressor_x - compressor_radius - 13,compressor_y- 7,		compressor_x - compressor_radius - 13,compressor_y + 8,	 BACKGROUND);
				tft.drawTriangle(compressor_x - compressor_radius - 3, compressor_y,  		compressor_x - compressor_radius - 13,compressor_y- 7,		compressor_x - compressor_radius - 13,compressor_y + 8,	 BACKGROUND);	//to prevent artefacts
			}
			tft.drawFastHLine(compressor_x-compressor_radius-compressor_pipeslen-3, compressor_y,	compressor_pipeslen, curr_color);
			tft.drawFastHLine(compressor_x+compressor_radius+3, 			compressor_y,	compressor_pipeslen, curr_color);
			tft.drawFastHLine(compressor_x-compressor_radius-compressor_pipeslen-3, compressor_y-1,	compressor_pipeslen, curr_color);
			tft.drawFastHLine(compressor_x+compressor_radius+3, 			compressor_y-1,	compressor_pipeslen, curr_color);
			tft.drawFastHLine(compressor_x-compressor_radius-compressor_pipeslen-3, compressor_y+1,	compressor_pipeslen, curr_color);
			tft.drawFastHLine(compressor_x+compressor_radius+3, 			compressor_y+1,	compressor_pipeslen, curr_color);
			
		}
		
		//compressor heater
		#define crcheater_x center_x
		#define crcheater_y compressor_y
		#define crcheater_radius compressor_radius + 5
		if ( crc_heater_state_prev != crc_heater_state || HP_conn_state_prev != HP_conn_state ) {
			if (crc_heater_state == 1) {
				curr_color = PINK;
			} else {
				curr_color = BACKGROUND;
			}
			if ( HP_conn_state == 0 ){
				curr_color = BACKGROUND;
			}
			drawPieSlice(crcheater_x,crcheater_y,crcheater_radius,curr_color,15,165);
			drawPieSlice(crcheater_x,crcheater_y,crcheater_radius+1,curr_color,15,165);
			drawPieSlice(crcheater_x,crcheater_y,crcheater_radius+2,curr_color,15,165);
		}
		
		//hot circle
		#define hotcircle_x center_x + 130
		#define hotcircle_y base_y + 82
		#define hot_xdiff -30
		if ( hotside_circle_state_prev != hotside_circle_state || heatpump_state_prev != heatpump_state || HP_conn_state_prev != HP_conn_state ) {
			if (heatpump_state == 1) {
				curr_color = WHITE;
			} else {
				curr_color = DARKGREY;
			}
			if (hotside_circle_state == 1) {
				curr_color2 = MAGENTA;
				curr_color3 = RED;
			} else {
				curr_color2 = VERYLIGHTRED;
				curr_color3 = VERYLIGHTRED;
			}
			if (HP_conn_state == 0) {
				curr_color = DARKGREY;
				curr_color2 = DARKGREY;
				curr_color3 = DARKGREY;
			}
			
			drawPieSlice(hotcircle_x,hotcircle_y,circle_radius,curr_color2,120,180);
			drawPieSlice(hotcircle_x,hotcircle_y,circle_radius,curr_color3,180,240);
			drawPieSlice(hotcircle_x,hotcircle_y,circle_radius+1,curr_color2,120,180);
			drawPieSlice(hotcircle_x,hotcircle_y,circle_radius+1,curr_color3,180,240);
			drawPieSlice(hotcircle_x,hotcircle_y,circle_radius+2,curr_color2,120,180);
			drawPieSlice(hotcircle_x,hotcircle_y,circle_radius+2,curr_color3,180,240);
			
			if (hotside_circle_state == 1) {
				tft.fillTriangle(hotcircle_x + hot_xdiff+ circle_pipes_len - 11, hotcircle_y-circle_pipes_ydiff+7,  		hotcircle_x + hot_xdiff + circle_pipes_len - 11,hotcircle_y - circle_pipes_ydiff - 7,		hotcircle_x + hot_xdiff + circle_pipes_len-2,hotcircle_y - circle_pipes_ydiff,	 curr_color3);	
			} else {
				tft.fillTriangle(hotcircle_x + hot_xdiff+ circle_pipes_len - 11, hotcircle_y-circle_pipes_ydiff+7,  		hotcircle_x + hot_xdiff + circle_pipes_len - 11,hotcircle_y - circle_pipes_ydiff - 7,		hotcircle_x + hot_xdiff + circle_pipes_len-2,hotcircle_y - circle_pipes_ydiff,	 BACKGROUND);	
				tft.drawTriangle(hotcircle_x + hot_xdiff+ circle_pipes_len - 11, hotcircle_y-circle_pipes_ydiff+7,  		hotcircle_x + hot_xdiff + circle_pipes_len - 11,hotcircle_y - circle_pipes_ydiff - 7,		hotcircle_x + hot_xdiff + circle_pipes_len-2,hotcircle_y - circle_pipes_ydiff,	 BACKGROUND);	 //to prevent artefacts
			}
			tft.drawFastHLine(hotcircle_x + hot_xdiff, hotcircle_y-circle_pipes_ydiff-2, circle_pipes_len-3, curr_color3);
			tft.drawFastHLine(hotcircle_x + hot_xdiff, hotcircle_y-circle_pipes_ydiff-1, circle_pipes_len-3, curr_color3);
			tft.drawFastHLine(hotcircle_x + hot_xdiff, hotcircle_y-circle_pipes_ydiff, circle_pipes_len-3, curr_color3);
			
			tft.drawFastHLine(hotcircle_x + hot_xdiff, hotcircle_y+circle_pipes_ydiff, circle_pipes_len, curr_color2);
			tft.drawFastHLine(hotcircle_x + hot_xdiff, hotcircle_y+circle_pipes_ydiff+1, circle_pipes_len, curr_color2);
			tft.drawFastHLine(hotcircle_x + hot_xdiff, hotcircle_y+circle_pipes_ydiff+2, circle_pipes_len, curr_color2);
		
			//hot heat exchanger
			#define hotxsize 20
			#define hotysize 50
			#define hotystep 10
			#define hotx center_x + 80 - hotxsize
			#define hoty base_y + 52
			#define hotpieces 5
			#define hot_pipelen 20
			y=hoty;
			tft.drawFastVLine(hotx + hotxsize/2-1, 	y-hot_pipelen, hot_pipelen-2, curr_color);
			tft.drawFastVLine(hotx + hotxsize/2, 	y-hot_pipelen, hot_pipelen-2, curr_color);
			tft.drawFastVLine(hotx + hotxsize/2+1, 	y-hot_pipelen, hot_pipelen-2, curr_color);
			
			tft.drawLine(hotx+hotxsize/2, y-(hotystep/2), hotx, y, curr_color);
			tft.drawLine(hotx+hotxsize/2, y-(hotystep/2)+1, hotx, y+1, curr_color);
			tft.drawLine(hotx+hotxsize/2, y-(hotystep/2)+2, hotx, y+2, curr_color);
			for(i = 0; i < hotpieces; i++){
				tft.drawFastHLine(hotx, y, hotxsize, curr_color);
				tft.drawFastHLine(hotx, y+1, hotxsize, curr_color);
				tft.drawFastHLine(hotx, y+2, hotxsize, curr_color);
				tft.drawLine(hotx+hotxsize, y, hotx, y+hotystep, curr_color);
				tft.drawLine(hotx+hotxsize, y+1, hotx, y+hotystep+1, curr_color);
				tft.drawLine(hotx+hotxsize, y+2, hotx, y+hotystep+2, curr_color);
				y+=hotystep;
			}
		
			tft.drawFastHLine(hotx, y, hotxsize/2, curr_color);
			tft.drawFastHLine(hotx, y+1, hotxsize/2, curr_color);
			tft.drawFastHLine(hotx, y+2, hotxsize/2, curr_color);
			tft.drawFastVLine(hotx + hotxsize/2-1, 	y, hot_pipelen, curr_color);
			tft.drawFastVLine(hotx + hotxsize/2, 	y, hot_pipelen, curr_color);
			tft.drawFastVLine(hotx + hotxsize/2+1, 	y, hot_pipelen, curr_color);
		}
		
		//eev
		#define ee_x center_x
		#define ee_y base_y + 122
		#define ee_radius 10
		#define ee_circle_radius ee_radius + 6
		if ( heatpump_state_prev != heatpump_state || HP_conn_state_prev != HP_conn_state ){
			if (heatpump_state == 1) {
				curr_color = WHITE;
			} else {
				curr_color = DARKGREY;
			}
			tft.drawTriangle(ee_x-ee_radius,ee_y-ee_radius,  ee_x,ee_y,	ee_x-ee_radius,ee_y+ee_radius,	 curr_color);
			tft.drawTriangle(ee_x+ee_radius,ee_y-ee_radius,  ee_x,ee_y,	ee_x+ee_radius,ee_y+ee_radius,	 curr_color);
			tft.drawCircle(ee_x, ee_y, ee_circle_radius, curr_color);
			tft.drawCircle(ee_x, ee_y, ee_circle_radius + 1, curr_color);
			//eev pipes
			#define eev_pipeslen 53
			if (heatpump_state == 1) {
				tft.fillTriangle(ee_x + ee_radius * 2,	ee_y,  		ee_x + ee_radius*3,ee_y- ee_radius/1.5,		ee_x + ee_radius*3,ee_y + ee_radius/1.5,	 curr_color);	//define ee_circle_radius won't work correctly
			} else {
				tft.fillTriangle(ee_x + ee_radius * 2,	ee_y,  		ee_x + ee_radius*3,ee_y- ee_radius/1.5,		ee_x + ee_radius*3,ee_y + ee_radius/1.5,	 BACKGROUND);	//define ee_circle_radius won't work correctly
				tft.drawTriangle(ee_x + ee_radius * 2,	ee_y,  		ee_x + ee_radius*3,ee_y- ee_radius/1.5,		ee_x + ee_radius*3,ee_y + ee_radius/1.5,	 BACKGROUND);	//to prevent artefacts
			}
			tft.drawFastHLine(ee_x-ee_radius-6-eev_pipeslen-3, 	ee_y,	eev_pipeslen, curr_color);		//define won't work correctly here, so ee_radius-6
			tft.drawFastHLine(ee_x+ee_circle_radius+3, 		ee_y,	eev_pipeslen, curr_color);
			tft.drawFastHLine(ee_x-ee_radius-6-eev_pipeslen-3, ee_y-1,	eev_pipeslen, curr_color);		//define won't work correctly here, so ee_radius-6
			tft.drawFastHLine(ee_x+ee_circle_radius+3, 		ee_y-1,	eev_pipeslen, curr_color);
			tft.drawFastHLine(ee_x-ee_radius-6-eev_pipeslen-3, ee_y+1,	eev_pipeslen, curr_color);		//define won't work correctly here, so ee_radius-6
			tft.drawFastHLine(ee_x+ee_circle_radius+3, 		ee_y+1,	eev_pipeslen, curr_color);
			
		}
			
		//print text
		#define cleanTx 50
		#define cleanTxA 2
		#define cleanTy 14
		//LSC
		for (i = 0; i<20; i++){
			if (lastStopCauseTxt_prev[i] != lastStopCauseTxt[i]){
				break;
			}
		}
		if ( i < 20 	|| HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(0, 0, 2,16, BACKGROUND);
			tft.fillRect(0, 0, 300,16, BACKGROUND);
			if (lastStopCauseTxt[0] != 0x00 && HP_conn_state == 1) {
				tft.setCursor(0, 0);
				tft.setTextColor(DARKGREY); 
				tft.println("Last Stop: ");
				tft.setCursor(120, 0);
				tft.println(lastStopCauseTxt);
				tft.setTextColor(YELLOW); 
			}
			for (i = 0; i<20; i++){
				lastStopCauseTxt_prev[i] = lastStopCauseTxt[i];
			}
		}
		/*if ( errorcode != errorcode_prev || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(0, 18, 2,16, BACKGROUND);
			tft.fillRect(0, 18, 300,16, BACKGROUND);
			if (errorcode != 0 && HP_conn_state == 1) {
				tft.setCursor(0, 18);
				tft.setTextColor(WHITE); 
				tft.println("errorcode: ");
				tft.setCursor(120, 18);
				tft.println(errorcode);
				tft.setTextColor(YELLOW); 
			}
			errorcode_prev = errorcode;
		}*/
		for (i = 0; i<20; i++){
			if (lastStartMsgTxt_prev[i] != lastStartMsgTxt[i]){
				break;
			}
		}
		if ( i < 20 || HP_conn_state != HP_conn_state_prev ){
			//if (errorcode == 0) { //else: error was printed
			if (HP_conn_state != HP_conn_state_prev) {
				tft.fillRect(0, 18, 2,16, BACKGROUND);
				tft.fillRect(0, 18, 300,16, BACKGROUND);
				tft.setCursor(0, 18);
				if ( HP_conn_state == 1 ) {
					tft.setTextColor(DARKGREY); 
					tft.println("StartMsg: ");
					tft.setTextColor(YELLOW);
				}
			}
			if ( lastStartMsgTxt[0] != 0x00 && HP_conn_state == 1) {
				//tft.fillRect(120, 18, 2,16, BACKGROUND);
				tft.fillRect(120+(i*12), 18, 2,16, BACKGROUND);
				tft.fillRect(120+(i*12), 18, 320-120-2-(i*12),16, BACKGROUND);
				tft.setCursor(120, 18);
				tft.setTextColor(DARKGREY);
				tft.println(lastStartMsgTxt);
				tft.setTextColor(YELLOW);
			}
			for (i = 0; i<20; i++){
				lastStartMsgTxt_prev[i] = lastStartMsgTxt[i];
			}
			//}
		}
		
		//2nd string place
		//tft.setCursor(0, 18);
		//tft.println(lastStopCauseTxt);

		if (Tci_prev != Tci || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(2, base_y + 40, cleanTxA,cleanTy, BACKGROUND);	//to prevent artefacts
			tft.fillRect(2, base_y + 40, cleanTx,cleanTy, BACKGROUND);	
			if (Tci != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(2, base_y + 40);
				set_precision(Tci);
				tft.println(Tci,prec);
			}
			Tci_prev = Tci;
		}
		if (Tco_prev != Tco || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(2, base_y + 110, cleanTxA,cleanTy, BACKGROUND);	//to prevent artefacts
			tft.fillRect(2, base_y + 110, cleanTx,cleanTy, BACKGROUND);	
			if (Tco != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(2, base_y + 110);
				set_precision(Tco);
				tft.println(Tco,prec);
			}
			Tco_prev = Tco;
		}
		if (Tbe_prev != Tbe || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(75, base_y + 126, cleanTxA,cleanTy, BACKGROUND);	//and so on
			tft.fillRect(75, base_y + 126, cleanTx,cleanTy, BACKGROUND);	
			if (Tbe != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(75, base_y + 126);
				set_precision(Tbe);
				tft.println(Tbe,prec);
			}
			Tbe_prev = Tbe;
		}
		if (Tae_prev != Tae || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(62, base_y + 5, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(62, base_y + 5, cleanTx,cleanTy, BACKGROUND);
			if (Tae != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(62, base_y + 5);
				set_precision(Tae);
				tft.println(Tae,prec);
			}
			Tae_prev = Tae;
		}
		if (Tho_prev != Tho || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(252, base_y + 40, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(252, base_y + 40, cleanTx,cleanTy, BACKGROUND);
			if (Tho != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(252, base_y + 40);
				set_precision(Tho);
				tft.println(Tho,prec);
			}
			Tho_prev = Tho;
		}
		if (Thi_prev != Thi || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(252, base_y + 110, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(252, base_y + 110, 115,cleanTy, BACKGROUND);
			if (Thi != -127.0 && HP_conn_state == 1 ){ 
				tft.setCursor(252, base_y + 110);
				set_precision(Thi);
				tft.println(Thi,prec);
				tft.setTextColor(WHITE); 
				tft.setCursor(252, base_y + 94);
				if (Ts1 == -127.0) {			//if Ts1 here: we use it as setpoint, dirty way
					tft.println("Thi:");
					tft.setTextColor(YELLOW);
				}
			}
			Thi_prev = Thi;
		}
		
		if (Ts1_prev != Ts1 || HP_conn_state != HP_conn_state_prev){
			tft.fillRect(202, base_y + 144, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(202, base_y + 144, cleanTx,cleanTy, BACKGROUND);
			tft.fillRect(252, base_y + 144, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(252, base_y + 144, cleanTx,cleanTy, BACKGROUND);
			if (Ts1 != -127.0 && HP_conn_state == 1){ //if Ts1 here: we use it as setpoint, dirty way
				tft.setTextColor(WHITE); 
				tft.setCursor(202, base_y + 144);
				tft.println("Ts1:");
				tft.setTextColor(YELLOW); 
				tft.setCursor(252, base_y + 144);
				set_precision(Ts1);
				tft.println(Ts1,prec);
			}
			Ts1_prev = Ts1;
		}
		
		if (Tac_prev != Tac || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(192, base_y + 126, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(192, base_y + 126, cleanTx,cleanTy, BACKGROUND);
			if (Tac != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(192, base_y + 126);
				set_precision(Tac);
				tft.println(Tac,prec);
			}
			Tac_prev = Tac;
		}
		if (Tbc_prev != Tbc || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(190, base_y + 5, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(190, base_y + 5, cleanTx,cleanTy, BACKGROUND);
			if (Tbc != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(190, base_y + 5);
				set_precision(Tbc);
				tft.println(Tbc,prec);
			}
			Tbc_prev = Tbc;
		}
		if (Tcrc_prev != Tcrc || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(138, base_y + 68, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(138, base_y + 68, cleanTx,cleanTy, BACKGROUND);
			if (Tcrc != -127.0 && HP_conn_state == 1 ){
				tft.setCursor(138, base_y + 68);
				set_precision(Tcrc);
				tft.println(Tcrc,prec);
			}
			Tcrc_prev = Tcrc;
		}
		if (async_wattage_prev != async_wattage || HP_conn_state != HP_conn_state_prev ){
			if ( HP_conn_state == 0 ){
				tft.fillRect(1, base_y + 160, cleanTxA,cleanTy, BACKGROUND);
				tft.fillRect(1, base_y + 160, 66,cleanTy, BACKGROUND);
			}
			tft.fillRect(1, base_y + 180, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(1, base_y + 180, cleanTx,cleanTy, BACKGROUND);
			if ( HP_conn_state == 1 ) {
				tft.setTextColor(OLIVE);
				tft.setCursor(1, base_y + 160);
				tft.println("Watts:");
				tft.setCursor(1, base_y + 180);
				tft.setTextColor(OLIVE);
				tft.println(async_wattage,0);
				tft.setTextColor(YELLOW); 
				async_wattage_prev = async_wattage;
			}
		}	
		if (EEV_cur_pos_prev != EEV_cur_pos || HP_conn_state != HP_conn_state_prev ){
			tft.fillRect(142, base_y + 90, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(142, base_y + 90, 36,cleanTy, BACKGROUND);
			if ( HP_conn_state == 1 ) {
				tft.setCursor(142, base_y + 90);
				tft.setTextColor(OLIVE);
				tft.println(EEV_cur_pos);
				tft.setTextColor(YELLOW); 
			}
			EEV_cur_pos_prev = EEV_cur_pos;
		}
		//clean "no connection" text at start (if connection appeared), but print at the end of proc
		if ( HP_conn_state == 0 && HP_conn_state_prev != HP_conn_state ) {
			tft.setTextColor(MAGENTA); 
			tft.setTextSize(2);
			tft.setCursor(35, 18);
			tft.println("No connection to HP!");
			tft.setTextColor(YELLOW); 
			tft.setTextSize(2);
		}
		
		//Setpoint
		if ( disp_T_setpoint_night_prev != disp_T_setpoint_night ) {	//no day/night due to no RTC, night value always used
			tft.fillRect(252, 218, cleanTxA,cleanTy, BACKGROUND);
			tft.fillRect(252, 218, cleanTx,cleanTy, BACKGROUND);
			tft.setTextSize(2);
			tft.setCursor(252, 201);
			tft.setTextColor(WHITE); 
			tft.println("Set:");
			tft.setCursor(252, 218);
			tft.setTextColor(GREEN);
			set_precision(disp_T_setpoint_night);			
			tft.println(disp_T_setpoint_night,prec);
			tft.setTextColor(YELLOW); 
			disp_T_setpoint_night_prev = disp_T_setpoint_night;
			delay(100); 	//to prevent very fast pressing
		}
		
		heatpump_state_prev 		= heatpump_state;
		coldside_circle_state_prev 	= coldside_circle_state;
		hotside_circle_state_prev 	= hotside_circle_state;
		crc_heater_state_prev 		= crc_heater_state;
		HP_conn_state_prev 		= HP_conn_state;
		//delay(5000);
	#endif
	//------------------------------	serial proc	------------------------------	

	if (iface == RS485_1 && multiSerial[iface]->available() > 0) {
		//Serial.println(String(freeMemory()));
		//Serial.println("something on serial..");	//debug
		#ifdef RS485_HOST_JSON
			iface = RS485_1;
			multiSerial[iface]->listen();
			index = 0;
			i = 0; 
			while (multiSerial[iface]->available() > 0) { 
				if(index < (BUFSIZE)) { 
					inChar = multiSerial[iface]->read(); 
					//Serial.print(inChar);
					dataBuf[index] = inChar;      
					index++;                    
					dataBuf[index] = '\0';        	// clear next symbol, null terminate the string
					delayMicroseconds(120);       	//80 microseconds - the best choice at 9600, "no answer"disappeared
									//40(20??) microseconds seems to be good, 9600, 49 symbols
									//
				} else {
					inChar = multiSerial[iface]->read();
					delayMicroseconds(120);
					i++;
				}
				if (i > 200) {
						break;		//infinite dirt
				}
			}
			
			//!debug, be carefull, can cause strange results
			/*
			if (dataBuf[0] != 0x00) {
				Serial.println("-");
				Serial.println(dataBuf);
				Serial.println("-");
			}*/
			
			
			//ALL lines must be terminated with \n!
			if ( (dataBuf[0] == hostID) && (dataBuf[1] == devID) ) {             
				//  COMMANDS:
				// G (0x47): (G)et main data
				// TNN.NN (0x54): set aim (T)emperature
				// ENN.NN (0x45): set (E)EV difference aim
				// T and E commands translated to Heat Pump immidiately
				// commented: P (0x50): (P)ythonic protocol test: sends (G) via seconary 485, get answer, use for debug, can not send string by parts! 
				digitalWrite(Serial1_TxControl, RS485Transmit);
				delay(3);
				//char *outChar=&outString[0];
				outString = "";
				outString += devID;
				outString += hostID;
				outString +=  "A ";  //where A is Answer, space after header			
				/*} else if ( dataBuf[2] == 0x50 ) {	// P (0x50): (P)ythonic protocol test: sends (G) via seconary 485, get answer, use for debug, can not send string by parts! 	
					outString = "";		
					iface = RS485_2;
					multiSerial[iface]->listen();
					digitalWrite(Serial1_TxControl, RS485Transmit);
					outString_S2 = "";
					outString_S2 += String(devID) + String(hpID) + "G";
					char *outChar=&outString_S2[0];
					multiSerial[iface]->write(outChar);
					multiSerial[iface]->flush();
					delayMicroseconds(250);						//fixes input 1st character dirt, 150 - min
					digitalWrite(Serial2_TxControl, RS485Receive);
					for (i=0; i<1000; i++)	{					//!! use only for test, can cause buffer overflow !!
						delay(1);
						if (multiSerial[iface]->available() > 0) {
							inChar = multiSerial[iface]->read();		//cleans up inChar !
							outString += inChar;
							if (inChar == 0x0a) {
								Serial.println("Success");
								break;
							}
						}
					}
					if (i == 1000){
						Serial.println(F("Timeout"));
					}
					Serial.println(outString);
					iface = RS485_1;
					multiSerial[iface]->listen();*/
				char *outChar=&outString[0];
				if ( (dataBuf[2] == 0x47 ) ) {
					//Serial.println("G");
					//WARNING: this procedure can cause "NO answer" effect if no or few T sensors connected
					outString += F("{");
					outString += "\"HPC\":" + String(HP_conn_state);
					//Serial.println(outString);
					multiSerial[iface]->write(outChar);            	//dirty hack to transfer long string
					multiSerial[iface]->flush();
					delay (3);                                      //lot of errors without delay, 3 in Arty - ok
					outString = "";
					if (HP_conn_state == 1) {
						outString += ",\"E1\":" + String(errorcode);  
						outString += ",\"TCI\":" + String(Tci);
						outString += ",\"TCO\":" + String(Tco);
						outString += ",\"TBE\":" + String(Tbe);
						outString += ",\"TAE\":" + String(Tae);
						multiSerial[iface]->write(outChar);           	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay, 3 in Arty - ok
						//outString = "";
						//outString += ",\"TSG\":" + String(Tsg);
						//outString += ",\"TSL\":" + String(Tsl);
						//outString += ",\"TBV\":" + String(Tbv);
						//outString += ",\"TSUC\":" + String(Tsuc);
						//multiSerial[iface]->write(outChar);           	//dirty hack to transfer long string
						//multiSerial[iface]->flush();
						//delay (3);                                      //lot of errors without delay
						outString = "";
						outString += ",\"TS1\":" + String(Ts1);
						outString += ",\"TS2\":" + String(Ts2);
						outString += ",\"TCRC\":" + String(Tcrc);
						outString += ",\"TR\":" + String(Treg);
						multiSerial[iface]->write(outChar);          	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay
						outString = "";
						outString += ",\"TAC\":" + String(Tac);
						outString += ",\"TBC\":" + String(Tbc);
						outString += ",\"THO\":" + String(Tho);
						outString += ",\"THI\":" + String(Thi);
						multiSerial[iface]->write(outChar);         	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay
						outString = "";
						
						outString += ",\"W1\":" + String(async_wattage);
						outString += ",\"EEVP\":" + String(EEV_cur_pos);
						outString += ",\"EEVA\":" + String(T_EEV_setpoint);
						multiSerial[iface]->write(outChar);            	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay
						outString = "";
	
						outString += ",\"A1\":" + String(T_setpoint);  //(A)im (target)
						outString += ",\"RP\":" + String(heatpump_state*10);  
						outString += ",\"RH\":" + String(hotside_circle_state*8);
						outString += ",\"RC\":" + String(coldside_circle_state*2);  
						multiSerial[iface]->write(outChar);            	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay
						outString = "";
						outString += ",\"RCRCH\":" + String(crc_heater_state*1);                 
						//outString += ",\"RRH\":" + String(reg_heater_state*4);
						multiSerial[iface]->write(outChar);         	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay
						outString = "";
						
						outString += F(",\"LSC\": \"");
						outString += lastStopCauseTxt;
						outString += ("\"");
						multiSerial[iface]->write(outChar);         	//dirty hack to transfer long string
						multiSerial[iface]->flush();
						delay (3);                                      //lot of errors without delay
						
						outString += F(",\"LSM\": \"");
						outString += lastStartMsgTxt;
						outString += ("\"");
						
					}
					outString += F("}");
					
				} else if ( (dataBuf[2] == 0x54 ) || (dataBuf[2] == 0x45 )) {  //(T)arget or (E)EV target format NN.NN, text
					if ( isDigit(dataBuf[ 3 ]) && isDigit(dataBuf[ 4 ]) && (dataBuf[ 5 ] == 0x2e)  && isDigit(dataBuf[ 6 ]) && isDigit(dataBuf[ 7 ]) && ( ! isDigit(dataBuf[ 8 ])) ) {
						
						analogWrite(speakerOut, 10);
						delay (100); 
						analogWrite(speakerOut, 0);
						
						char * carray = &dataBuf[ 3 ];
						tempdouble = atof(carray);                
						
						if (dataBuf[2] == 0x54 ){
							if (tempdouble > cT_setpoint_max) {
								outString += F("{\"r\":\"too hot!\"}");
							} else if (tempdouble < cT_setpoint_min) {
								outString += F("{\"r\":\"too cold!\"}");
							} else {
								disp_T_setpoint_day = tempdouble;
								disp_T_setpoint_night = tempdouble;
								outString += F("{\"r\":\"ok, new value for D/N is: ");
								outString += String(disp_T_setpoint_day);
								outString += F("\"}");
							}
						}
						if (dataBuf[2] == 0x45 ) {
							if (tempdouble > 10.0) {		//!! hardcode !!
								outString += F("{\"r\":\"too hot!\"}");
							} else if (tempdouble < 0.1) {		//!! hardcode !!
								outString += F("{\"r\":\"too cold!\"}");
							} else {
								disp_T_EEV_setpoint = tempdouble;
								outString += F("{\"r\":\"ok, new EEV value is: ");
								outString += String(disp_T_EEV_setpoint);
								outString += F("\"}");
							}
						}
					} else {
						outString += F("{\"r\":\"NaN, format: NN.NN\"}");
					}
				} else {
					//default, just for example
					outString += F("{\"r\":\"no_command\"}");
				}
				//crc.integer = CRC16.xmodem((uint8_t& *) outString, outString.length());
				//outString += (crc, HEX);
				outString += F("\n");
				multiSerial[iface]->write(outChar);
			}
			
			index = 0;
			for (i=0;i<(BUFSIZE);i++) {
				dataBuf[i]=0;
			}
			multiSerial[iface]->flush();
			digitalWrite(Serial1_TxControl, RS485Receive);
			delay(1);
		#endif
		//1st: process reqs
		#ifdef RS485_HOST_MODBUS
			iface = RS485_1;
			if (multiSerial[iface]->available()) {
				if ( (ReadModbusMsg() != 0 ) ||  (Check_CRC != 0) ){
					Serial.println(F("MmsgERR714")); //probably another proto
				} else {
					digitalWrite(Serial1_TxControl, RS485Transmit);
					z = 0;
					if (dataBuf[0] != 0x00 && dataBuf[0] != devID ) {	//will reply to 0x00
						z = 0xFF;
					}
					if (dataBuf[1] != 0x03 && dataBuf[1] != 0x06) { //0x01
						z = 1;
					}
					if (dataBuf[2] != 0x00 && dataBuf[4] != 0x00) { //0x02
						z = 2;
					}
					if (dataBuf[5] > MODBUS_MR)  {	//0x02 
						z = 2;
					}
					i = 0;
					dataBuf[i]  =  devID;
					i++;
					if (z == 0) {
						datetime = rtc.now();
						
						x = dataBuf[3]; 	//addr	
						y = dataBuf[5];		//num
						if (dataBuf[1]  ==  0x03) {
							dataBuf[i]  = 0x03;
							i++;	
							//the most significant byte is sent first
							dataBuf[i]  =  y*2;
							i++;  // data
							for (u = x; u < (x+y); u++) {
								if (u > MODBUS_MR){
									z = 2;
									break;
								}
								switch (u) {								
									case 0x00:
										Add_Double_To_Buf_IntFract(Tci); //uses dataBuf, i
										break;
									case 0x01:
										Add_Double_To_Buf_IntFract(Tco); //uses dataBuf, i
										break;
									case 0x02:
										Add_Double_To_Buf_IntFract(Tbe); //uses dataBuf, i
										break;
									case 0x03:
										Add_Double_To_Buf_IntFract(Tae); //uses dataBuf, i
										break;
									case 0x04:
										//Add_Double_To_Buf_IntFract(Tsg); //uses dataBuf, i
										dataBuf[i]  = 0x00;
										i++;
										dataBuf[i]  = 0x00;
										i++;
										break;
									case 0x05:
										//Add_Double_To_Buf_IntFract(Tsl); //uses dataBuf, i
										dataBuf[i]  = 0x00;
										i++;
										dataBuf[i]  = 0x00;
										i++;
										break;
									case 0x06:
										//Add_Double_To_Buf_IntFract(Tbv); //uses dataBuf, i
										dataBuf[i]  = 0x00;
										i++;
										dataBuf[i]  = 0x00;
										i++;
										break;
									case 0x07:
										//Add_Double_To_Buf_IntFract(Tsuc); //uses dataBuf, i
										dataBuf[i]  = 0x00;
										i++;
										dataBuf[i]  = 0x00;
										i++;
										break;
									case 0x08:
										Add_Double_To_Buf_IntFract(Ts1); //uses dataBuf, i
										break;
									case 0x09:
										Add_Double_To_Buf_IntFract(Ts2); //uses dataBuf, i
										break;
									case 0x0A:
										Add_Double_To_Buf_IntFract(Tcrc); //uses dataBuf, i
										break;
									case 0x0B:
										Add_Double_To_Buf_IntFract(Treg); //uses dataBuf, i
										break;
									case 0x0C:
										Add_Double_To_Buf_IntFract(Tac); //uses dataBuf, i
										break;
									case 0x0D:
										Add_Double_To_Buf_IntFract(Tbc); //uses dataBuf, i
										break;
									case 0x0E:
										Add_Double_To_Buf_IntFract(Tho); //uses dataBuf, i
										break;
									case 0x0F:
										Add_Double_To_Buf_IntFract(Thi); //uses dataBuf, i
										break;
									case 0x10:
										dataBuf[i]  = 0;
										i++;
										dataBuf[i]  = errorcode;
										i++;
										break;
									case 0x11:
										dataBuf[i]  = (int)async_wattage >> 8;
										i++;
										dataBuf[i]  = (int)async_wattage & 0xFF;
										i++;
										break;
									case 0x12:
										dataBuf[i]  = 0;
										i++;
										dataBuf[i]  = 0;
										bitWrite(dataBuf[i], 0, heatpump_state);
										bitWrite(dataBuf[i], 1, hotside_circle_state);
										bitWrite(dataBuf[i], 2, coldside_circle_state);
										bitWrite(dataBuf[i], 3, crc_heater_state);
										//bitWrite(dataBuf[i], 4, reg_heater_state);
										i++;
										break;
									case 0x13:
										Add_Double_To_Buf_IntFract(T_EEV_setpoint); //uses dataBuf, i
										break;
									case 0x14:
										Add_Double_To_Buf_IntFract(T_setpoint); //uses dataBuf, i
										break;
									case 0x15:
										dataBuf[i]  = (int)EEV_cur_pos >> 8;
										i++;
										dataBuf[i]  = (int)EEV_cur_pos & 0xFF;
										i++;
										break;
									case 0x16:
										dataBuf[i]  = lastStopCauseTxt[0];
										i++;
										dataBuf[i]  = lastStopCauseTxt[1];
										i++;
										break;
									case 0x17:
										dataBuf[i]  = lastStopCauseTxt[2];
										i++;
										dataBuf[i]  = lastStopCauseTxt[3];
										i++;
										break;
									case 0x18:
										dataBuf[i]  = lastStopCauseTxt[4];
										i++;
										dataBuf[i]  = lastStopCauseTxt[5];
										i++;
										break;
									case 0x19:
										dataBuf[i]  = lastStopCauseTxt[6];
										i++;
										dataBuf[i]  = lastStopCauseTxt[7];
										i++;
										break;
									case 0x1A:
										dataBuf[i]  = lastStopCauseTxt[8];
										i++;
										dataBuf[i]  = lastStopCauseTxt[9];
										i++;
										break;
									case 0x1B:
										dataBuf[i]  = lastStopCauseTxt[10];
										i++;
										dataBuf[i]  = lastStopCauseTxt[11];
										i++;
										break;
									case 0x1C:
										Add_Double_To_Buf_IntFract(disp_T_EEV_setpoint);//uses dataBuf, i
										break;
									case 0x1D:
										Add_Double_To_Buf_IntFract(disp_T_setpoint_day);//uses dataBuf, i
										break;
									case 0x1E:
										Add_Double_To_Buf_IntFract(disp_T_setpoint_night);//uses dataBuf, i
										break;
									case 0x1F:			//reserved
										dataBuf[i]  = 0;
										i++;
										dataBuf[i]  = 0;
										i++;
										break;
										datetime = rtc.now();
									case 0x20:			//current time
										dataBuf[i]  = 0;
										i++;
										dataBuf[i]  = datetime.hour();
										i++;
										break;
									case 0x21:			//current time 2
										dataBuf[i]  = datetime.minute();
										i++;
										dataBuf[i]  = datetime.second();
										i++;
										break;
									case 0x22:			//daystart time
										dataBuf[i]  = c_day_start_H;
										i++;
										dataBuf[i]  = c_day_start_M;
										i++;
										break;
									case 0x23:			//daystop time
										dataBuf[i]  = c_night_start_H;
										i++;
										dataBuf[i]  = c_night_start_M;
										i++;
										break;
									case 0x24:
										dataBuf[i]  = lastStartMsgTxt[0];
										i++;
										dataBuf[i]  = lastStartMsgTxt[1];
										i++;
										break;
									case 0x25:
										dataBuf[i]  = lastStartMsgTxt[2];
										i++;
										dataBuf[i]  = lastStartMsgTxt[3];
										i++;
										break;
									case 0x26:
										dataBuf[i]  = lastStartMsgTxt[4];
										i++;
										dataBuf[i]  = lastStartMsgTxt[5];
										i++;
										break;
									case 0x27:
										dataBuf[i]  = lastStartMsgTxt[6];
										i++;
										dataBuf[i]  = lastStartMsgTxt[7];
										i++;
										break;
									case 0x28:
										dataBuf[i]  = lastStartMsgTxt[8];
										i++;
										dataBuf[i]  = lastStartMsgTxt[9];
										i++;
										break;
									case 0x29:
										dataBuf[i]  = lastStartMsgTxt[10];
										i++;
										dataBuf[i]  = lastStartMsgTxt[11];
										i++;
										break;
									default:
										dataBuf[i]  = 0x00;
										i++;
										dataBuf[i]  = 0x00;
										i++;
										break;
								}
							}
						} else if (dataBuf[1]  ==  0x06) {	//de-facto echo
							dataBuf[i]  = 0x06;
							i++;
							dataBuf[i]  = 0x00;
							i++;
							dataBuf[i]  = x;
							i++;
							switch (x) {
								case 0x1C:
									IntFract_to_tempdouble(dataBuf[4], dataBuf[5]);
									if (tempdouble > 125.0 || tempdouble < -55.0) {	//just incorrectest values filter
										z = 3;
										break;
									}
									disp_T_EEV_setpoint = tempdouble;
									Add_Double_To_Buf_IntFract(disp_T_EEV_setpoint); //uses dataBuf, i
									break;
								case 0x1D:
									IntFract_to_tempdouble(dataBuf[4], dataBuf[5]);
									if (tempdouble > cT_setpoint_max || tempdouble < cT_setpoint_min) {	//just incorrectest values filter
										z = 3;
										break;
									}
									disp_T_setpoint_day = tempdouble;
									Add_Double_To_Buf_IntFract(disp_T_setpoint_day); //uses dataBuf, i
									break;
								case 0x1E:
									IntFract_to_tempdouble(dataBuf[4], dataBuf[5]);
									if (tempdouble > cT_setpoint_max || tempdouble < cT_setpoint_min) {	//just incorrectest values filter
										z = 3;
										break;
									}
									disp_T_setpoint_day = tempdouble;
									Add_Double_To_Buf_IntFract(disp_T_setpoint_night); //uses dataBuf, i
									break;
								default:
									z = 3;
									break;
							}
						} else {
							z = 1;
						}
						if (z != 0) {
							i = 1;
							bitWrite(dataBuf[i], 7, 1);
							i++;
							dataBuf[i] = z;
							i++;
						}
						
						if (z != 0xFF) {
							CalcAdd_BufCRC();
							multiSerial[iface]->write(dataBuf, i);
							multiSerial[iface]->flush();	
							delay (1);                
						}
						/*for (z = 0; z < i; z++){
							Serial.print(dataBuf[z], HEX);  
							Serial.print(" ");  
							if ( z%50 == 0) Serial.println();
						}
						Serial.println();*/
						
						digitalWrite(Serial1_TxControl, RS485Receive);
					}
				}
			}

		#endif
	}
	
	//------------------------------	main proc	------------------------------
	//2nd - stick with HP
	#ifdef RS485_HP_MODBUS
		//Serial.println(mod_485_2_state);
		if( (mod_485_2_state == MOD_READY ) && ( ((unsigned long)(millis_now - millis_modbus03_prev) > millis_modbus03_cycle )  ||  (millis_modbus03_prev == 0))    ) {
			millis_modbus03_prev = millis_now;
			iface = RS485_2;
			multiSerial[iface]->listen();
			delay(1);
			if (multiSerial[iface]->available()) {
				i = 0; 
				while (multiSerial[iface]->available()) {
					if (i > 200) {
						break;		//infinite dirt
					}
					inChar = multiSerial[iface]->read();
					delayMicroseconds(1800);
					i++;
				}
			}
			digitalWrite(Serial2_TxControl, RS485Transmit);
	
			//all
			i = 0;
			dataBuf[i]  =  hpID;
			i++;
			dataBuf[i]  =  0x03;
			i++;
			dataBuf[i]  =  0x00;
			i++;
			dataBuf[i]  =  0x00;
			i++;
			dataBuf[i]  =  0x00;
			i++;
			dataBuf[i]  =  regsToReq;	//HP regs N, v1.2
			i++;
			CalcAdd_BufCRC();
						
			multiSerial[iface]->write(dataBuf, i);
			multiSerial[iface]->flush();
			time_sent  =  millis_now;
			delay (1);

			/*for (x =0; x<i; x++){
				Serial.print(dataBuf[x], HEX);
				Serial.print(" ");
			}
			Serial.println();*/
	
			digitalWrite(Serial2_TxControl, RS485Receive);
			delayMicroseconds(1800);
			mod_485_2_state = MOD_03_SENT;
			//Serial.println("MOD_03_SENT");
			
			//every cycle - as error, will be auto-cleaned if OK
			sequential_errors += 1;
			//Serial.println(sequential_errors);
			if (sequential_errors > MAX_SEQUENTIAL_ERRORS) {
				HP_conn_state = 0;
			}
			if (sequential_errors > 200) {
				sequential_errors = 200;
			}
		}
		
		if (mod_485_2_state == MOD_03_SENT) {
			//Serial.println("03S");
			disp_T_setpoint_sent		= 0;	//flood prevention
			disp_T_EEV_setpoint_sent	= 0;	//flood prevention
			if (!multiSerial[iface]->available()) {
				if ( (unsigned long)(millis_now - time_sent) > c_resp_timeout ){
					//Serial.println(F("F03respTmOut"));
					mod_485_2_state = MOD_READY;
				} else {
					//waiting...
				}
			} else  {
				if ( (ReadModbusMsg() != 0 ) ||  (Check_CRC() != 0) ){
					Serial.println(F("MmsgERR1219, dump:")); //probably another proto
					for (x = 0; x<index; x++){
						Serial.print(dataBuf[x], HEX);
						Serial.print(" ");
					}
					Serial.println();
				} else {
					/*
					Serial.println(F("Modbus Got OK, dump:"));
					
					for (x = 0; x<index; x++){
						Serial.print(dataBuf[x], HEX);
						Serial.print(" ");
					}
					Serial.println();
					*/
					
					z = 0;
					if (dataBuf[0] != hpID ) {	//will reply to 0x00
						z = 0xFF;
					}
					if (dataBuf[1] != 0x03 ) {
						z = 0xFE;
					}
					if (dataBuf[2] != regsToReq*2) { 
						z = 0XFD;
					}
					if ( z != 0 ){
						Serial.print(F("ModParseErr:"));
						Serial.println(z);
					} else {
						sequential_errors = 0;
						HP_conn_state = 1;
						//Serial.println(F("Parsing.."));
						for (i = 0; i< regsToReq; i++){
							if (deb_mod == 1) Serial.print(F("Reg"));
							if (deb_mod == 1) Serial.print(i);
							x = 3 + i*2;
							y = 0;
							IntFract_to_tempdouble(dataBuf[x], dataBuf[x+1]);	//most values applicable here
							if (deb_mod == 1) {
								Serial.print(" >");
								Serial.print(dataBuf[x], DEC);
								Serial.print(" ");
								Serial.print(dataBuf[x+1], DEC);
								Serial.print("> ");
							}
							if ( tempdouble > 125.0 || tempdouble < -55.0 ) {		//simple incorrectest values filter
								y = 1;
								if ( tempdouble == -127.0 ){
									y = 0; //"no sensor" values passed
								}
							}
							switch (i) {
								case 0x00:
									if (y == 0) {	Tci = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tci "));	Serial.println(Tci);}
									break;
								case 0x01:
									if (y == 0) {	Tco = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tco "));	Serial.println(Tco);}
									break;
								case 0x02:
									if (y == 0) {	Tbe = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tbe "));	Serial.println(Tbe);}
									break;
								case 0x03:
									if (y == 0) {	Tae = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tae "));	Serial.println(Tae);}
									break;
								case 0x04:
									if (y == 0) {	Tsg = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tsg "));	Serial.println(Tsg);}
									break;
								case 0x05:
									if (y == 0) {	Tsl = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tsl "));	Serial.println(Tsl);}
									break;
								case 0x06:
									if (y == 0) {	Tbv = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tbv "));	Serial.println(Tbv);}
									break;
								case 0x07:
									if (y == 0) {	Tsuc = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tsuc "));	Serial.println(Tsuc);}
									break;
								case 0x08:
									if (y == 0) {	Ts1 = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Ts1 "));	Serial.println(Ts1);}
									break;
								case 0x09:
									if (y == 0) {	Ts2 = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Ts2 "));	Serial.println(Ts2);}
									break;
								case 0x0A:
									if (y == 0) {	Tcrc = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tcrc "));	Serial.println(Tcrc);}
									break;
								case 0x0B:
									if (y == 0) {	Treg = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Treg "));	Serial.println(Treg);}
									break;
								case 0x0C:
									if (y == 0) {	Tac = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tac "));	Serial.println(Tac);}
									break;
								case 0x0D:
									if (y == 0) {	Tbc = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tbc "));	Serial.println(Tbc);}
									break;
								case 0x0E:
									if (y == 0) {	Tho = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Tho "));	Serial.println(Tho);}
									break;
								case 0x0F:
									if (y == 0) {	Thi = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("Thi "));	Serial.println(Thi);}
									break;
								case 0x10:
									errorcode = dataBuf[x+1];
									if (deb_mod == 1) {Serial.print(F("errorcode "));		Serial.println(errorcode);}
									break;
								case 0x11:
									async_wattage = (double)( ((int) dataBuf[x]<<8) + ((int)dataBuf[x+1]) );	//does double casting works? use () around casted chars
									if (deb_mod == 1) {Serial.print(F("async_wattage "));		Serial.println(async_wattage);}
									break;
								case 0x12:
									heatpump_state = 	bitRead(dataBuf[x+1], 0);
									hotside_circle_state = 	bitRead(dataBuf[x+1], 1);
									coldside_circle_state = bitRead(dataBuf[x+1], 2);
									crc_heater_state = 	bitRead(dataBuf[x+1], 3);
									//reg_heater_state = 	bitRead(dataBuf[x+1], 4);
									if (deb_mod == 1) {Serial.print(F("heatpump_state "));		Serial.println(heatpump_state);}
									if (deb_mod == 1) {Serial.print(F("hotside_circle_state "));	Serial.println(hotside_circle_state);}
									if (deb_mod == 1) {Serial.print(F("coldside_circle_state "));	Serial.println(coldside_circle_state);}
									if (deb_mod == 1) {Serial.print(F("crc_heater_state "));	Serial.println(crc_heater_state);}
									//if (deb_mod == 1) {Serial.print(F("reg_heater_state "));	Serial.println(reg_heater_state);}
									break;
								case 0x13:
									if (y == 0) {	T_EEV_setpoint = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("T_EEV_setpoint "));	Serial.println(T_EEV_setpoint);}
									break;
								case 0x14:
									if (y == 0) {	T_setpoint = tempdouble;	}
									if (deb_mod == 1) {Serial.print(F("T_setpoint "));	Serial.println(T_setpoint);}
									break;
								case 0x15:
									//Serial.println((int) dataBuf[x]<<8);
									//Serial.println((int) dataBuf[x+1]);
									EEV_cur_pos = ((int) dataBuf[x]<<8) + ((int)dataBuf[x+1]);	//use () around casted chars
									if (deb_mod == 1) {Serial.print(F("EEV_cur_pos "));	Serial.println(EEV_cur_pos);}
									break;
								case 0x16:
									lastStopCauseTxt[0] = dataBuf[x];
									lastStopCauseTxt[1] = dataBuf[x+1];
									break;
								case 0x17:
									lastStopCauseTxt[2] = dataBuf[x];
									lastStopCauseTxt[3] = dataBuf[x+1];
									break;
								case 0x18:
									lastStopCauseTxt[4] = dataBuf[x];
									lastStopCauseTxt[5] = dataBuf[x+1];
									break;
								case 0x19:
									lastStopCauseTxt[6] = dataBuf[x];
									lastStopCauseTxt[7] = dataBuf[x+1];
									break;
								case 0x1A:
									lastStopCauseTxt[8] = dataBuf[x];
									lastStopCauseTxt[9] = dataBuf[x+1];
									break;
								case 0x1B:
									lastStopCauseTxt[10] = dataBuf[x];
									lastStopCauseTxt[11] = dataBuf[x+1];
									if (deb_mod == 1) {Serial.print("LSC:");Serial.write(lastStopCauseTxt,12);Serial.println();}
									break;
								case 0x1C:
									lastStartMsgTxt[0] = dataBuf[x];
									lastStartMsgTxt[1] = dataBuf[x+1];
									break;
								case 0x1D:
									lastStartMsgTxt[2] = dataBuf[x];
									lastStartMsgTxt[3] = dataBuf[x+1];
									break;
								case 0x1E:
									lastStartMsgTxt[4] = dataBuf[x];
									lastStartMsgTxt[5] = dataBuf[x+1];
									break;
								case 0x1F:
									lastStartMsgTxt[6] = dataBuf[x];
									lastStartMsgTxt[7] = dataBuf[x+1];
									break;
								case 0x20:
									lastStartMsgTxt[8] = dataBuf[x];
									lastStartMsgTxt[9] = dataBuf[x+1];
									break;
								case 0x21:
									lastStartMsgTxt[10] = dataBuf[x];
									lastStartMsgTxt[11] = dataBuf[x+1];
									if (deb_mod == 1) {Serial.print(F("LSM:"));Serial.write(lastStartMsgTxt,12);Serial.println();}
									break;
								default:
									break;
							}
						}
					}
				
				}
				mod_485_2_state = MOD_READY;
			}
		}
		//set HP reg if differ
		//ex:11 06 0001 0003 9A9B
		if (HP_conn_state != 0 && mod_485_2_state != MOD_03_SENT) {
			i = 0;
			dataBuf[i]  =  0x00;
			i++;
			dataBuf[i]  =  0x06;
			i++;
			dataBuf[i]  =  0x00;
			i++;
			#ifdef A_MINI
				hour = Clock.getHour(h12, PM);
				minute = Clock.getMinute();
			#endif
			#ifdef A_MEGA
				hour = 0;
				minute = 0;
			#endif
			if (	(disp_T_setpoint_night > T_setpoint+0.005 || disp_T_setpoint_night < T_setpoint-0.005) 	&& 	(hour*60+minute <= c_day_start_H*60 + c_day_start_M || hour*60+minute >= c_night_start_H*60 + c_night_start_M)	&&  ( disp_T_setpoint_sent == 0)	){
				//0x14
				//night
				//Serial.println(F("06NightSET"));
				//Serial.println(T_setpoint,9);
				//Serial.println(disp_T_setpoint_night,9);
				dataBuf[i]  =  0x14;
				i++;
				Add_Double_To_Buf_IntFract(disp_T_setpoint_night);
				disp_T_setpoint_sent	= 1;
			} else if (	(disp_T_setpoint_day > T_setpoint+0.005 || disp_T_setpoint_day < T_setpoint-0.005) 	&&	(hour*60+minute > c_day_start_H*60 + c_day_start_M && hour*60+minute < c_night_start_H*60 + c_night_start_M)	&&  ( disp_T_setpoint_sent == 0)	){
				//0x14
				//day
				//Serial.println(F("06DaySET"));
				dataBuf[i]  =  0x14;
				i++;
				Add_Double_To_Buf_IntFract(disp_T_setpoint_day);
				disp_T_setpoint_sent	= 1;
			} else if ((disp_T_EEV_setpoint > T_EEV_setpoint+0.005 || disp_T_EEV_setpoint < T_EEV_setpoint-0.005 ) && (disp_T_EEV_setpoint_sent == 0) ) {
				//Serial.println(disp_T_EEV_setpoint_sent);
				//Serial.println(T_EEV_setpoint,9);
				//Serial.println(disp_T_EEV_setpoint,9);
				//Serial.println(F("06EEVSET"));
				dataBuf[i]  =  0x13;
				i++;
				Add_Double_To_Buf_IntFract(disp_T_EEV_setpoint);
				disp_T_EEV_setpoint_sent = 1; //flood prevention
				
					
			} else {
				//Serial.println(F("06HPvaluesOK"));
			}
			if (i == 6) {	//request ready
				digitalWrite(Serial2_TxControl, RS485Transmit);
				CalcAdd_BufCRC();
			
				multiSerial[iface]->write(dataBuf, i);
				multiSerial[iface]->flush();
				time_sent  =  millis();
				delay (1);
				
				digitalWrite(Serial2_TxControl, RS485Receive);	//do not read answer
				delayMicroseconds(1800);
			}
		}
	#endif
	
	digitalWrite(Serial1_TxControl, RS485Receive);
	digitalWrite(Serial2_TxControl, RS485Receive);
}



