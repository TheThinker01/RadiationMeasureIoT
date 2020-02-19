
/* 
 *  Connections:
 *  Arduino   Module        
 *  Pin 10    TX of Wifi Module
 *  Pin 11    RX of Wifi Module
 *  Pin 2     Gieger Counter Vin
 *  3.3 V     Vcc,EN of Wifi Module
 *  SDA,SCL   SDA and SCL Of LCD
 *  5V        Vcc Of Geiger Counter,Vcc of LCD
 *  GND       GND of Wifi Module,Geiger Counter
 * 
 * Author: Archisman Pathak
*/


#include <SPI.h>
#include <SoftwareSerial.h>//for making virtual serial port for esp8266
#include <dht.h>
#include <Wire.h>           // Include Wire Library for I2C
#include <LiquidCrystal_I2C.h>// Include NewLiquidCrystal Library for I2C

#define LOG_PERIOD 15000  //Logging period in milliseconds, recommended value 15000-60000.(For GM Counter)
#define MAX_PERIOD 60000  //Maximum logging period without modifying this sketch(For GM Counter)

#define RX 10 //Connect TX of wifi module to here
#define TX 11 //Connect RX of wifi module to here

/*LCD variables */
const int  en = 2, rw = 1, rs = 0, d4 = 4, d5 = 5, d6 = 6, d7 = 7, bl = 3;  // Define LCD pinout
const int i2c_addr = 0x27;  // Define I2C Address - change if reqiuired
LiquidCrystal_I2C lcd(i2c_addr, en, rw, rs, d4, d5, d6, d7, bl, POSITIVE);

/*Geiger Counter Variables */
int countTrueCommand;
int countTimeCommand; 
boolean found = false; 
double valSensor = 1.0;

unsigned long counts;     //variable for GM Tube events
unsigned long cpm;        //variable for CPM
unsigned int multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement
double result; //for storing the cpm in uSv/h

/* Wifi Module Variables  */
String API = "DYCWDCCKA5P6F9DO";   // CHANGE ME
String HOST = "api.thingspeak.com";
String PORT = "80";
String field = "field1";

String AP = "Archisman";       // CHANGE ME
String PASS = "archis0900"; // CHANGE ME
SoftwareSerial esp8266(RX,TX); 

void tube_impulse(){       //subprocedure for capturing events from Geiger Kit
  counts++;
}

void setup(){             //setup subprocedure
  counts = 0;
  cpm = 0;
  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  
  lcd.begin(16,2);  // Set display type as 16 char, 2 rows
  
  Serial.begin(9600);
  esp8266.begin(115200);
  
  attachInterrupt(0, tube_impulse, FALLING);  //define external interrupts on PIN 0
  
  sendCommand("AT",5,(char*)"OK");                                         //Set Up The Wifi Module
  sendCommand("AT+CWMODE=1",5,(char*)"OK");                              //Set Up The Wifi Module
  sendCommand("AT+CWJAP=\""+ AP +"\",\""+ PASS +"\"",20,(char*)"OK");   //Set Up The Wifi Module
  
}

void loop(){                                 //main cycle
  valSensor = getSensorData();
  if(valSensor!=-1)
  {
  String getData = "GET /update?api_key="+ API +"&"+ field +"="+String(valSensor);
   sendCommand("AT+CIPMUX=1",5,(char*)"OK");
   sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,(char*)"OK");
   sendCommand("AT+CIPSEND=0," +String(getData.length()+4),4,(char*)">");
   esp8266.println(getData);delay(1500);countTrueCommand++;
   sendCommand("AT+CIPCLOSE=0",5,(char*)"OK");
   printOnLcd(valSensor);
  }
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
 {
    lcd.setCursor(0,0);
    lcd.println("Reading:");
    lcd.setCursor(0,1);
    lcd.print(valSensor);
    lcd.print("  uSv/h");    
 }
