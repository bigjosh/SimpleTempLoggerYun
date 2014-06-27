// This is program will allow an Arduino Yun to log tempurature data from DS1820 One-Wire sensors
// to a Google Spreadsheet. For more info, see...
// http://wp.josh.com/...

// Data wire from Temp Sensors is plugged into this pin on the Arduino
#define ONE_WIRE_BUS 11

// The addrsss of the Google Apps Script that will append the data into a spreadsheet
// For info on how to get this URL for your own google spreadsheet, see...
// http://wp.josh.com/2014/06/16/using-google-spreadsheets-for-logging-sensor-data/
//
// In the meantime, for playing you can use the URL below that logs to a sample spreadsheet here...
// https://docs.google.com/spreadsheets/d/1P0JM7CJU3snBUgjXYzidC7_h2MzJNZuPMAMUblVY28s/edit?usp=sharing
// Use this only for testing becuase I periodically clear it out (getting your own private sheet is FREE!)

#define GOOGLE_URL "https://script.google.com/macros/s/AKfycbylmWrBhAs__0V_sqUmk_N_0b23n8QHWG0menWSBuyEobR1-C8/exec"

// UnComment out this line to turn on debug messages to the USB port that are visible 
// using Serial Monitor on the Arduino IDE. Do not leave DEBUG defined for a board in production
// becuase the print statements will hang if there is no computer attached. Also, printing to the serial
// port on the Yun only seems to work after you download from the IDE. If you try to do it on power-up
// the Yun seems to not create the USB Serial port on the host. Argh.

//#define DEBUG 

// Hook up a speaker to this port to hear a reassuring beep on each sample
// Comment line out for no tone (or just dont connect a speaker the the pin :))

#define TONE_PIN 10
a
// We blink the LED connected here to indicate that we are running
//  1Hrz = waiting for dead air on Linino before starting up
// 10Hrz = waiting for next sample time
// Off   = Sampling sensors
// On    = Making HTTP request

#define LED_PIN 13

#define SAMPLE_INTERVAL 120   // Time between samples in seconds

#define MAX_DEVICE_COUNT 20   // Maximum supported number of connected temp sensors
                              // You can increase this until you run out of memory
                              // or exceed the 1-wire specs

// Debug statements compile away if DEBUG not defined

#ifdef DEBUG

  #define db(x) (Serial.print(x))
  #define dbl(x) (Serial.println(x))
  
#else

  #define db(x) 
  #define dbl(x) 

#endif


// Any extra params you want to add at the begining of the URL. Here we have the linux side add a timestamp & MAC address

#define PARAM_HEAD "?TIMESTAMP=$(date +%F~%T)&SOURCEMAC=$(cat /sys/class/net/eth0/address)"


// The actual command to pass to Linux side

#define COMMAND_HEAD "curl -k \""      // Must enclose url in quotes or else & will be read by the shell. Must use -k or else SSL to google doesn't work on Linino.
#define COMMAND_TAIL "\""              // This goes at the very end of the command line


// Concatinate all these parts into the begining of the command line

#define COMMAND_PREFIX COMMAND_HEAD GOOGLE_URL PARAM_HEAD


/*

 Dallas temp sensor reading library from...
 
 http://www.milesburton.com/?title=Dallas_Temperature_Control_Library
 
 */
 

#include <OneWire.h>
#include <DallasTemperature.h>
  
 // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);


// This buffer is used to build up the CURL command we will send to the Linino command line
// We compute the maximum size the command line can be based on the max number of sensors and the max size of the param for each sensor
char commandLineBuffer[ sizeof( COMMAND_PREFIX )-1 + sizeof( "SEQ=1234567890" )-1 + ( MAX_DEVICE_COUNT * (sizeof( "&AABBCCDDEEFFGGHH=-999.99" ) -1 ) ) + (sizeof(COMMAND_TAIL) -1 ) + 1 + 1];


// Needed for functions to access strings in PROGMEM

#include <avr/pgmspace.h>

// Copy from a FlashStringHelper into RAM 
// Just isolates the ugly typcast....

void strcpy_F( char *d , const __FlashStringHelper *s)
 {
	strcpy_P( d , (const prog_char *) s );
 }

const char hexdigits[] = "0123456789ABCDEF";

// buffer must be 17 bytes long

int address2string( char *buffer ,  DeviceAddress d ) {
    
  int p=0;
      
  for(int i=0; i<8;i++ ) {
  
    uint8_t digit = d[i];
         
    buffer[p++] = hexdigits[ digit >> 4 ];
    
    buffer[p++] = hexdigits[ digit & 0x0f ];
             
  }
   
  buffer[p] = 0x00;  // Make it into a proper string by zero terminating it
  
  return( 8*2 );
  
}

// The library only scans for devices at startup anyway, so lets only make the list once
// so we don't keep scanning the bus


DeviceAddress deviceAddressList[MAX_DEVICE_COUNT]; 

int deviceCount; 


void setupLinino() {

  long linuxBaud = 250000;
  Serial1.begin(linuxBaud); 
  
  db(F("Chekcing Linino console for dead air...."));

  // Wait for at least 1 second of dead air to make sure that Linino boot process is complete,
  // otherwise we might accedentally answer the "press any key to start uboot" prompt.
  
  do {
    while (Serial1.available()) Serial1.read();
    db(F("Waiting 1 second of dead air from Linino link before continuing..."));
    digitalWrite(LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(500);                    // wait for 1/2 a second
    digitalWrite(LED_PIN, LOW);    // turn the LED off by making the voltage LOW
    delay(500);                    // wait for 1/2 a second    
    
  } while (Serial1.available());
  

  Serial1.print( F("\xff\0\0\x05XXXXX\x0d\xaf") );   // Bridge shutdown command incase bridge is running
  Serial1.print( '\x03' );                           // control-c incase there is a running process
  Serial1.print( '\r' );                             // Send enter in case we are at "Please press enter to activate" on fresh boot


}


void blindSendToLinino( const char *s ) {
    
  Serial1.println( s );  
  
}


void setupDebug() {
  Serial.begin(250000);
  while (!Serial);      // Wait for connections (does not seem to work... argh.)
}
  
 
void setup(void)
{
  
  #ifdef DEBUG
     setupDebug();
  #endif
      
  // Blink some lights to show we are alive... 
  pinMode( 13 , OUTPUT );
    
  setupLinino();
 
   
  db( F("Multi-DS1820 Temp Sensor Logging to Google Spreadsheet" ));
  
  dbl(F("Starting sensor library..."));
  sensors.begin(); 
  
  deviceCount = sensors.getDeviceCount();
  
  db(F("Device count:"));
  dbl(deviceCount);
  
  if (deviceCount > MAX_DEVICE_COUNT ) {
    
    deviceCount = MAX_DEVICE_COUNT;

    db(F("Device count exceeded MAX_DEVICE_COUNT. Truncating to "));
    dbl(deviceCount);

    
  }
  
  for(int i=0; i<deviceCount; i++) {
    
          db(F("Storing Sensor #"));
          dbl(i);
          
          sensors.getAddress( (uint8_t* ) deviceAddressList[i]  , i );
  } 
    
}


unsigned long next_sample_time = 0;

void loop(void)
{ 
 
  db( F("Waiting for next schedualed update..."));
  
  // Wait for next sample time...
  while (millis()<next_sample_time) {
    // Blink some lights to show we are alive... 
    digitalWrite(LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);               // wait for a second
    digitalWrite(LED_PIN, LOW);    // turn the LED off by making the voltage LOW
    delay(100);               // wait for a second    
  }    
      
  // Schedual next sample now so we don't drift forward due to the time it actually take to take the sample...
     
  next_sample_time = millis() + (SAMPLE_INTERVAL * 1000UL );
  
  dbl(F("SAMPLE TIME!"));
          
  #ifdef TONE_PIN     
    tone(TONE_PIN,1000);
  #endif
  
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus   
 
  db(F("Requesting temperatures..."));
  sensors.requestTemperatures(); // Send the command to get temperatures
  dbl(F("DONE "));
  
  #ifdef TONE_PIN 
    noTone(TONE_PIN);
  #endif
    
  strcpy_F( commandLineBuffer , F( COMMAND_PREFIX ) );    // Start with the begingin of the command line in the buffer (the F() keeps the string in PROGMEM rather than using up RAM)    
    
  // Build up one URL paramaeter and value for each temp sensor reading
    
  for(int i=0; i<deviceCount ; i++) {
    
          db(F("Reading Sensor #"));
          dbl(i);
                                                    
          int16_t temp = sensors.getTemp( deviceAddressList[i] );
          
          db(F("TempRaw="));
          dbl(temp);
                              
          if (temp != DEVICE_DISCONNECTED_RAW ) {
            
            strcat( commandLineBuffer , "&");
            address2string( commandLineBuffer+strlen(commandLineBuffer) , deviceAddressList[i] );
            strcat( commandLineBuffer , "=");
                        
            float tempF = sensors.rawToFahrenheit( temp );
            
            db(F("TempF="));
            dbl(tempF);            
            
            // Convert float to string with 1 decimal place
            dtostrf( tempF , 1 , 1 , commandLineBuffer+strlen(commandLineBuffer) );
            
          } 
                                    
  }
  
  strcat( commandLineBuffer , COMMAND_TAIL );
      
  db(F("Sending command to Linino:"));
  dbl(commandLineBuffer);
  
  digitalWrite(LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
      
  blindSendToLinino( commandLineBuffer );
  
  dbl(F("Sent."));
  
  dbl(F("RESPONSE------>" ));  
  
  // Note that for debug this will only show part of the respose becuase the Arduino serial will overflow,
  // but it iwll be enough for use to see if the request worked or not.
  
  do {
  
  
    while (Serial1.available()) {
    
      char c = Serial1.read();
      
      db( c );
      
    }
    
    delay(1);    // Let command execute and 

    
  } while (Serial1.available());

    
 
  dbl(  F("<------RESPONSE" ));  
  
  
  digitalWrite(LED_PIN, LOW);   // turn the LED off 
  
      
//  */
  
  dbl(  F("Done with cycle." ));
           
}

