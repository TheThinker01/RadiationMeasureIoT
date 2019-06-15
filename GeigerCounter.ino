/*
* --------------------------------------------------------------------------------------
* WHAT IS CPM?
* CPM (or counts per minute) is events quantity from Geiger Tube you get during one minute. Usually it used to 
* calculate a radiation level. Different GM Tubes has different quantity of CPM for background. Some tubes can produce
* about 10-50 CPM for normal background, other GM Tube models produce 50-100 CPM or 0-5 CPM for same radiation level.
* Please refer your GM Tube datasheet for more information. Just for reference here, J305 and SBM-20 can generate 
* about 10-50 CPM for normal background. 
* --------------------------------------------------------------------------------------
* HOW TO CONNECT GEIGER KIT?
* The kit 3 wires that should be connected to Arduino UNO board: 5V, GND and INT. PullUp resistor is included on
* kit PCB. Connect INT wire to Digital Pin#2 (INT0), 5V to 5V, GND to GND. Then connect the Arduino with
* USB cable to the computer and upload this sketch. 
* 
 * Author:JiangJie Zhang * If you have any questions, please connect cajoetech@qq.com
 * 
 * License: MIT License
 * 
 * Please use freely with attribution. Thank you!
*/

/* 
 * **************************** Connections: ***********************
 * 
 *  Arduino         
 *  Pin 10    TX of Wifi Module
 *  Pin 11    RX of Wifi Module
 *  Pin 2     Gieger Counter Vin
 *  Pin 9     Buzzer(+) connected Pin 9 via a 330Ohm-1kOhm resistor 
 *  3.3 V     Vcc,EN of Wifi Module
 *  SDA,SCL   SDA and SCL Of LCD(0x27),SDA and SCL of MPU 6050 (0x68 with AD0 as low)
 *  5V        Vcc Of Geiger Counter,Vcc of LCD,Vcc of MPU6050
 *  GND       GND of Wifi Module,Geiger Counter,GND of Buzzer,MPU6050, AD0 of MPU6050
 *  
 * 
 * ******************************************************************
 */
 
/* *********************** LIBRARIES ************************ */
#include <SPI.h>
#include <SoftwareSerial.h>//for making virtual serial port for esp8266
#include <dht.h>
#include <Wire.h>           // Include Wire Library for I2C
#include <LiquidCrystal_I2C.h>// Include NewLiquidCrystal Library for I2C

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"

/* ********************** MACROS *************************** */

#define LOG_PERIOD 15000  //Logging period in milliseconds, recommended value 15000-60000.(For GM Counter)
#define MAX_PERIOD 60000  //Maximum logging period without modifying this sketch(For GM Counter)

#define RX 10 //Connect TX of wifi module to here
#define TX 11 //Connect RX of wifi module to here

#define OUTPUT_READABLE_ACCELGYRO
/* ************************ LCD variables **************************** */
const int  en = 2, rw = 1, rs = 0, d4 = 4, d5 = 5, d6 = 6, d7 = 7, bl = 3;  // Define LCD pinout
const int i2c_addr = 0x27;  // Define I2C Address for LCD - change if reqiuired 
LiquidCrystal_I2C lcd(i2c_addr, en, rw, rs, d4, d5, d6, d7, bl, POSITIVE);

/* ********************* Geiger Counter Variables ************************** */
int countTrueCommand;
int countTimeCommand; 
boolean found = false; 
double valSensor = 1.0;

unsigned long counts;     //variable for GM Tube events
unsigned long cpm;        //variable for CPM
unsigned int multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement
double result; //for storing the cpm in uSv/h

/* ***************** Wifi Module Variables *********************** */
String API = "DYCWDCCKA5P6F9DO";   // CHANGE ME
String HOST = "api.thingspeak.com";
String PORT = "80";
String field = "field1";

String AP = "Archisman";       // CHANGE ME
String PASS = "archis0900"; // CHANGE ME
SoftwareSerial esp8266(RX,TX); 

/* ************** MPU6050 variables ******************* */

// class default I2C address for MPU6050 is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for GY521 evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;
//MPU6050 accelgyro(0x69); // <-- use for AD0 high

int16_t ax, ay, az; //current values of acceleration
int16_t oax, oay, oaz;  //old values of acceleration

const int buzzer = 9;



/* ************FUNCTIONS**************  */

//Function for Security Unlock - Unlocks The device and Stops The Buzzer
int8_t ch ;//Just a dummy variable which I am Using for Now untill proper unlock works
int securityUnlock()
{
  ch++;
  if(ch>7){ lcd.clear();lcd.print("DEVICE UNLOCKED"); return 1;}
  return 0;
}

//Generates the Alarm
void generateAlarm()
{   lcd.clear();
  while(securityUnlock()==0)
  {
    lcd.setCursor(0,0);
    lcd.print("DEVICE LOCKED");
    Serial.println("Theft Detected Alert!!!!");  tone(buzzer, 1000); // Send 1KHz sound signal...
    delay(1000);        // ...for 1 sec
    noTone(buzzer);     // Stop sound...
    delay(1000);
    }
 }

void tube_impulse(){       //subprocedure for capturing events from Geiger Kit
  counts++;
}

void setup(){             //setup subprocedure
  
   #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif
    pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output
    // initialize serial communication
    // (38400 chosen because it works as well at 8MHz as it does at 16MHz, but
    // it's really up to you depending on your project)
    Serial.begin(38400);

    // initialize device
    Serial.println("Initializing I2C devices...");
    accelgyro.initialize();

    // verify connection
    Serial.println("Testing device connections...");
    Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
    
  counts = 0;
  cpm = 0;
  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  
  lcd.begin(16,2);  // Set display type as 16 char, 2 rows

  esp8266.begin(115200);
  
  attachInterrupt(0, tube_impulse, FALLING);  //define external interrupts on PIN 0
  
  sendCommand("AT",5,(char*)"OK");                                         //Set Up The Wifi Module
  sendCommand("AT+CWMODE=1",5,(char*)"OK");                              //Set Up The Wifi Module
  sendCommand("AT+CWJAP=\""+ AP +"\",\""+ PASS +"\"",20,(char*)"OK");   //Set Up The Wifi Module
  
}

void loop(){     //main cycle
  detectMotion();
  valSensor = getSensorData();
  detectMotion();
  if(valSensor!=-1)
  {
    printOnLcd(valSensor);
    String getData = "GET /update?api_key="+ API +"&"+ field +"="+String(valSensor);
   sendCommand("AT+CIPMUX=1",5,(char*)"OK");
   detectMotion();
   sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,(char*)"OK");
   detectMotion();
   sendCommand("AT+CIPSEND=0," +String(getData.length()+4),4,(char*)">");
   detectMotion();
   esp8266.println(getData);delay(1500);countTrueCommand++;
   detectMotion();
   sendCommand("AT+CIPCLOSE=0",5,(char*)"OK");
   detectMotion();
   
  }
  detectMotion();
}

double getSensorData(){
    unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > LOG_PERIOD){
    previousMillis = currentMillis;
    cpm = counts * multiplier;
    result = cpm / 151.0 ;
    Serial.println(result);
    counts = 0;
    return result;
    
  }
  return -1.0;
}


void sendCommand(String command, int maxTime, char readReplay[]) {
  Serial.print(countTrueCommand);
  Serial.print(". at command => ");
  Serial.print(command);
  Serial.print(" ");
  while(countTimeCommand < (maxTime*3))//while 1
  {
    esp8266.println(command);//at+cipsend
  
    if(esp8266.find(readReplay))//ok
    {
      found = true;
      
      break;
    }
  
    countTimeCommand++;
  }
  
  if(found == true)
  {
    Serial.println("Done");
    countTrueCommand++;
    countTimeCommand = 0;
  }
  
  if(found == false)
  {
    Serial.println("Fail");
    countTrueCommand = 0;
    countTimeCommand = 0;
  }
  
  found = false;
 }

 void printOnLcd(double valSensor)
 {  lcd.clear();
    lcd.setCursor(0,0);
    lcd.println("Reading:");
    lcd.setCursor(0,1);
    lcd.print(valSensor);
    lcd.print("  uSv/h");    
 }

 void detectMotion()
 {
  accelgyro.getAcceleration(&ax, &ay, &az);
  if(((ax-oax>10000)||(ay-oay>10000))||((az-oaz>10000)||(oax-ax>10000))||((oay-ay>10000)||(oaz-az>10000)))//Initial Check
    { int flag=0;
      delay(1000);
      for(int i=0;i<2;i++)//Keep checking for the next 1 seconds 
      {
         flag=1;
        oax=ax;oay=ay;oaz=az;
        accelgyro.getAcceleration(&ax, &ay, &az);
        
        if(((ax-oax>2000)||(ay-oay>2000))||((az-oaz>2000)||(oax-ax>2000))||((oay-ay>2000)||(oaz-az>2000))){delay(500);}
        else
        {
          flag=0;
          break;
        }
      } 
        if(flag==1)
        {generateAlarm();}
      
    }
    delay(2000);
    oax=ax;oay=ay;oaz=az;
 }
