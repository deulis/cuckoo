/*******************************************************************************
* Modern Cuckcoo Clock
* Eng. Deulis Antonio Pelegrin Jaime February 2020
* 
* This is a very limited version
*
* Hardware Connections:
*	D1 - Tx
*	D3 - Rx
*	
*	D5 - Rx (SoftSerial to comunicate with the DFPlayer mini)
*	D4 - Tx (SoftSerial to comunicate with the DFPlayer mini)
*	
*	D0 - LED Blue
*	D2 - LED Green
*	D15- LED Red
*
*	D12- CS of Led Matrix Display
*	D13- DIN of Led Matrix Display
*	D14- CLK of Led Matrix Display
*
*	D16- Servo
*******************************************************************************/


#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#include <SoftwareSerial.h>
#include <DFPlayerMini_Fast.h>

#include <Servo.h>

#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include "Timezone.h"




#define STA_SSID "your-ssid"
#define STA_PASSWORD  "your-password"

#define SOFTAP_SSID "Cuckoo"
#define SOFTAP_PASSWORD "12345678" 
#define SERVER_PORT 2000

#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "north-america.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)

#define LED_RED 15
#define LED_GREEN 2
#define LED_BLUE 0

#define SERVO_PIN 16
#define SERVO_MIN 5
#define SERVO_DANCE 160
#define SERVO_MAX 180

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES  4
#define CLK_PIN   14  // or SCK
#define DATA_PIN  13  // or MOSI
#define CS_PIN    12  // or SS

#define SOUND_SERIAL_RX 5
#define SOUND_SERIAL_TX 4


WiFiServer server(SERVER_PORT);

// SPI hardware interface
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

SoftwareSerial sound_serial(SOUND_SERIAL_RX, SOUND_SERIAL_TX); // RX, TX
DFPlayerMini_Fast sound;

Servo servor;  // create servo object to control a servo

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

bool connected = false;
unsigned long last_second;

enum colors {
	color_black, color_blue, color_green, color_cyan, color_red, color_magenta, color_yellow, color_white
};

String st;
const char* days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char* ampm[] = { "AM", "PM" };

unsigned long cuckoo_animation_start = millis();
int cuckoo_animation_index = 0;
int cuckoo_animation_hours = 0;
int color_index = 1;

void light(int color)
{
	if (color > 7 || color < 0)
		color = 0;

	bool b = (color & 0x01) == 0x01;
	bool g = (color & 0x02) == 0x02;
	bool r = (color & 0x04) == 0x04;

	digitalWrite(LED_RED, r);
	digitalWrite(LED_GREEN, g);
	digitalWrite(LED_BLUE, b);
}

void bird_in()
{
	servor.write(SERVO_MIN);
}

void bird_out()
{
	servor.write(SERVO_MAX);
}


byte rgb = 0;

void setup() {

	pinMode(LED_RED, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);

	light(color_black);

	servor.attach(SERVO_PIN);
	//bird_in();
	servor.write(SERVO_MIN);

	last_second = millis();
	Serial.begin(9600);
	P.begin();
	P.displayText("Hello", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

	delay(1000); //To allow DFPlayer be ready
	sound_serial.begin(9600);
	sound.begin(sound_serial);
	sound.volume(25);
	delay(20);
	sound.play(1);

	bool r;
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPConfig(IPAddress(192, 168, 73, 1), IPAddress(192, 168, 73, 1), IPAddress(255, 255, 255, 0));
	r = WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD, 6, 0);
	server.begin();

	timeClient.begin();   // Start the NTP UDP client

	if (r)
		Serial.println("SoftAP started!");
	else
		Serial.println("ERROR: Starting SoftAP");


	Serial.print("Trying WiFi connection to ");
	Serial.println(STA_SSID);

	WiFi.setAutoReconnect(true);
	WiFi.begin(STA_SSID, STA_PASSWORD);

	ArduinoOTA.begin();
}


int c;
bool go_to_sleep = false;
bool allow_animation = true;
void loop() {
	//Handle OTA
	ArduinoOTA.handle();

	//Handle Connection to Access Point
	if (WiFi.status() == WL_CONNECTED)
	{
		if (!connected)
		{
			connected = true;
			Serial.println("");
			Serial.print("Connected to ");
			Serial.println(STA_SSID);
			Serial.println("WiFi connected");
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());
		}
	}
	else
	{
		if (connected)
		{
			connected = false;
			Serial.print("Disonnected from ");
			Serial.println(STA_SSID);
		}
	}

	if (millis() - last_second > 1000)
	{
		last_second = millis();

		

		timeClient.update();
		unsigned long epochTime = timeClient.getEpochTime();

		// convert received time stamp to time_t object
		time_t local, utc;
		utc = epochTime;
		
		// Then convert the UTC UNIX timestamp to local time
		TimeChangeRule usEDT = { "EDT", Second, Sun, Mar, 2, -300 };  //UTC - 5 hours - change this as needed
		TimeChangeRule usEST = { "EST", First, Sun, Nov, 2, -360 };   //UTC - 6 hours - change this as needed
		Timezone usEastern(usEDT, usEST);
		local = usEastern.toLocal(utc);

		int h = hourFormat12(local);
		int m = minute(local);

		st = "";
		st += h;
		st += ":";		
		if (m < 10)  // add a zero if minute is under 10
			st += "0";
		st += m;

		P.print(st);

		if (m == 0)
		{
			if (allow_animation)
			{
				allow_animation = false;
				cuckoo_animation_hours = h;
				cuckoo_animation_index = 0;
				cuckoo_animation_start = millis();
			}
		}
		else
		{
			allow_animation = true;
		}

	}

	P.displayAnimate();

	unsigned long elapsed = millis() - cuckoo_animation_start;
	
	if (cuckoo_animation_hours > 0)
	{
		if (cuckoo_animation_index == 0)
		{
			color_index++;
			if (color_index > 7)
				color_index = 1;
			light(color_index);

			servor.write(SERVO_DANCE);
			sound.play(1);
			cuckoo_animation_index++;
		}
		else if (cuckoo_animation_index == 1 && elapsed > 300)
		{
			servor.write(SERVO_MAX);
			cuckoo_animation_index++;
		}
		else if (cuckoo_animation_index == 2 && elapsed > 500)
		{
			servor.write(SERVO_DANCE);
			cuckoo_animation_hours--;
			cuckoo_animation_index++;

			if (cuckoo_animation_hours == 0)
			{
				go_to_sleep = true;
				cuckoo_animation_start = millis();
			}			
		}
		else if (cuckoo_animation_index == 3 && elapsed > 1500)
		{
			cuckoo_animation_index=0;
			cuckoo_animation_start = millis();
		}
	}
	else if (go_to_sleep && elapsed > 500)
	{
		go_to_sleep = false;
		servor.write(SERVO_MIN);
		light(0);
	}

}
