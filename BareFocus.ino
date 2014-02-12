/*
Serial Command Reference:

t;    - get temp    returns t:65:F;
s;    - get steps   returns s:100;
m#;   - move to # step
aX;   - set LCD temp display.  X is F or C
h;    - halt in progress move.
b#;   - set backlight brightness from 0-100
g;    - get "is moving" returns g:false; or g:true;
d;    - set debug state 0 for off, 1 for on
u#;   - how often in milliseconds to update the LCD
p#;   - set stepper RPM
*/

#include <stdio.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"

#define STEPSIZE 	6
#define STEPS     200 
#define RPM       100
#define ledPin     13

unsigned long UPDATE_MILLIS = 100;
unsigned long UPDATE_TEMP_MILLIS = 10000; // 10 seconds should be good.



//EEPROM Memory Locations:
#define EEPROM_SCHEMA_VERSION 0x11
int EEPROM_schema    = 0; // 1 byte for schema
int EEPROM_bl_1b     = 1; // 1 byte for BackLight Value
int EEPROM_temp_type = 2; // 1 byte for temp type
int EEPROM_pos       = 3; // 4 bytes for pos
int EEPROM_rpm       = 7; // 2 bytes for rpm

boolean needEEPromStepUpdate = false;

unsigned long lastUpdate = 0;
unsigned long lastTempUpdate = 0;

int DEBUG = 0;

unsigned long stepVal = 0;
unsigned long moveVal = 0;
boolean moving = false;
int rpm = RPM;

#if USETEMP
OneWire oneWire(TEMPSENSOR);
DallasTemperature sensors(&oneWire);
#endif


void setup() {
  Serial.begin(9600);

  byte blVal;
  initializeEeprom();  

  EEPROM_readAnything(EEPROM_bl_1b, blVal );
  EEPROM_readAnything(EEPROM_pos, stepVal );
  EEPROM_readAnything(EEPROM_rpm, rpm);
  moveVal = stepVal;

  analogReference(EXTERNAL);

  lastUpdate = millis();
  pinMode(ledPin, OUTPUT);
}

void initializeEeprom()
{
	byte version;
	EEPROM_readAnything(EEPROM_schema, version);

	if( EEPROM_SCHEMA_VERSION != version )
	{
		if( DEBUG )
		{
			Serial.print("Current EEPROM Schema: ");
			Serial.println(version, BIN);
			Serial.print("Version Expected: ");
			Serial.println((byte)EEPROM_SCHEMA_VERSION, BIN);
			Serial.println("Initializing EEPROM");
		}
		EEPROM_writeAnything(EEPROM_schema, (byte)EEPROM_SCHEMA_VERSION);
		EEPROM_writeAnything(EEPROM_bl_1b, (byte)0xFF);
		EEPROM_writeAnything(EEPROM_temp_type, (char)'C');
		EEPROM_writeAnything(EEPROM_pos, (unsigned long)0);
		EEPROM_writeAnything(EEPROM_rpm, (int)75);
	}
}


void loop() {
 handleSerial();
 moveStepper();
}



boolean doUpdate()
{
  unsigned long now = millis();
  
  if( now > UPDATE_MILLIS + lastUpdate )
  {
    if( DEBUG )
    {
      Serial.print( "Updating " );
      Serial.print( UPDATE_MILLIS + lastUpdate );
      Serial.print( "  " );
      Serial.print( now );
      Serial.print( "  " );
      Serial.println( UPDATE_MILLIS );
    }
    lastUpdate = now;
    return true;
  }
  return false;
}

void handleSerial()
{
  if( Serial.available() > 0 )
  {
    char cmd;
	char str[16];
	memset(str, 0, 16);

    if( DEBUG )
    {
      Serial.print( "bytes = " );
      Serial.println( Serial.available() );
    }

    cmd = Serial.read();

	Serial.readBytesUntil('$', str, 16);

    if( 0 )
    {
	  Serial.print( "cmd = ");
	  Serial.print( cmd );
	  Serial.println( str );
    }
	
	switch( cmd )
	{
	//======================================================================
	//====================Focuser Commands==================================
	//======================================================================
	case 't': // get temp - t$
		{			
			Serial.print("t:");
			Serial.print(27.6, 2);
			Serial.println(":C;");
		}
		break;
	case 's': // get current position - s$
		{
			Serial.print("m:");
			Serial.print(moving ? "true;" : "false;");

			Serial.print("s:");
			Serial.print(stepVal);

			Serial.print(";t:");
			Serial.print(27.6, 2);

			Serial.println('$');
		}
		break;
	case 'm': // move - m100$
		{
			if( DEBUG )
			{
				Serial.print("Moving to string: ");
				Serial.println(str);
			}
			Serial.println("mm$");
			moveVal = atol(str);
		}
		break;
	case 'a': // set temp display type - aC$ or aF$
		{
			char tempType = str[0];
			EEPROM_writeAnything( EEPROM_temp_type, tempType );
			Serial.println("aa$");
		}
		break;
	case 'h': // halt - h$
		{
			if( moving )
			{
				moveVal = stepVal;
				moving = false;
			}
			Serial.println("hh$");
		}
		break;
	case 'c': // set current position - c100$
		{
			moveVal = stepVal = atol(str);
			needEEPromStepUpdate = true;
			Serial.println("cc$");
		}
		break;
	case 'g': // isMoving? - g$ returns g:true$ or g:false$
		{
			char temp[16];
			sprintf( temp, "g:%s$", moving ? "true" : "false");
			Serial.println(temp);
		}
		break;
	case 'p': // sets stepper rpm
		{
			rpm = atoi(str);
			Serial.println("pp$");
			EEPROM_writeAnything(EEPROM_rpm, rpm);
		}
		break;
	}

	if( DEBUG )
	{
		if( Serial.available() > 0 )
		{
			Serial.print("Have data left: ");
			Serial.println(Serial.available());
		}
	}

    while( Serial.available() > 0 )
		Serial.read();
  }
}


void moveStepper()
{
  if( moveVal == stepVal )
  {
	  if( needEEPromStepUpdate )
	  {
		  unsigned long tEEVal = 0UL;
		  EEPROM_readAnything(EEPROM_pos, tEEVal);

		  if( tEEVal != stepVal )
			  EEPROM_writeAnything(EEPROM_pos, stepVal);

		  needEEPromStepUpdate = false;
	  }
	  digitalWrite(ledPin, LOW);
	  moving = false;
      return;
  }

  digitalWrite(ledPin, HIGH);
  moving = true;
  needEEPromStepUpdate = true;

  if( moveVal > stepVal )
  {
    int stepSize = moveVal - stepVal > STEPSIZE ? STEPSIZE : moveVal - stepVal;
	delay(50);
    stepVal += stepSize;
    return;
  }

  if( moveVal < stepVal )
  {
    int stepSize = stepVal - moveVal > STEPSIZE ? STEPSIZE : stepVal - moveVal;
    delay(50);
    stepVal -= stepSize;
    return;
  }

}






