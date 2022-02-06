/************************************************************************************/
/*														                                        							*/
/*              Centralka alarmowa, czujniki PIR, powiadomienie SMS,MQTT            */
/*              Hardware: Arduino Nano ATmega328P									                  */
/*				Na podstawie: http://tiny.cc/tiny-gsm-readme						                  */			
/*																					                                        */
/*              Author: Dariusz Wulkiewicz											                    */
/*                      d.wulkiewicz@gmail.com  									                  */
/*																					                                        */
/*              Date: 01.2018														                            */
/************************************************************************************/


//-----------------------------------------------------------------
//konfiguracja:
//-----------------------------------------------------------------

// Set phone numbers, if you want to test SMS and Calls
#define SMS_TEL_NUMBER "+4860xxxxxxx"

//-----------------------------------------------------------------
//-----------------------------------------------------------------

// Select your modem:
#define TINY_GSM_MODEM_SIM800

#include <SoftwareSerial.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#define SERIAL_AT_RX_GPIO_PIN  7
#define SERIAL_AT_TX_GPIO_PIN  6

#define PIR_SENSOR1_GPIO_PIN 9
#define PIR_SENSOR2_GPIO_PIN 10

#define PIR_INACTIVE 0x0
#define PIR_ACTIVE 0x1 

#define BUILTIN_LED_GPIO_PIN 13
#define LED_ON 0x1
#define LED_OFF 0x0

#define BUZZER_GPIO_PIN 2
#define BUZZER_ON 0x0
#define BUZZER_OFF 0x1

#define LCD_I2C_ADDRESS 0x3F
#define LCD_I2C_HEIGHT     2
#define LCD_I2C_WIDTH     16

// set the LCD address to 0x3F for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_I2C_WIDTH, LCD_I2C_HEIGHT);  

// Set serial for debug console (to the Serial Monitor, speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
SoftwareSerial SerialAT(SERIAL_AT_RX_GPIO_PIN, SERIAL_AT_TX_GPIO_PIN); // RX, TX

//#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG SerialMon

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

bool pirSensor_01_state = false;
bool pirSensor_02_state = false;

bool alarmState = false;
bool alarmStatePir01 = false;
bool alarmStatePir02 = false;
long smsCount = 0;

//----------------------------------------------------------------- 
void pirSensorsLoop(bool forceStatusPrint = false)
{
  //wykrywanie zboczy
  if (digitalRead(PIR_SENSOR1_GPIO_PIN) != pirSensor_01_state) 
	{
    pirSensor_01_state = !pirSensor_01_state;   
    printPirStatus();
    //wykrywanie aktywacji
    if (pirSensor_01_state == PIR_ACTIVE) {           
      alarmState = true;          
    }    
	}
  if (digitalRead(PIR_SENSOR2_GPIO_PIN) != pirSensor_02_state) 
  {
    pirSensor_02_state = !pirSensor_02_state;    
    printPirStatus();    
    //wykrywanie aktywacji
    if (pirSensor_02_state == PIR_ACTIVE) {           
      alarmState = true;        
    }
  }
  digitalWrite(BUILTIN_LED_GPIO_PIN, pirSensor_01_state || pirSensor_02_state ? LED_ON : LED_OFF);
  digitalWrite(BUZZER_GPIO_PIN, pirSensor_01_state || pirSensor_02_state ? BUZZER_ON : BUZZER_OFF);    

  if(forceStatusPrint)
  {
    printPirStatus();    
  }  
}

//----------------------------------------------------------------- 
bool alarmLoop() {  
    //wykrywanie zmiany stanu od ostatniego SMS'a 
    if(!alarmState){
      alarmStatePir01 = pirSensor_01_state;
      alarmStatePir02 = pirSensor_02_state;      
      return true;
    }        
    if (alarmState && (alarmStatePir01 != pirSensor_01_state || alarmStatePir02 != pirSensor_02_state)) {
                         
      if(sendSMS(String("OGRODEK Narzedziownia: ") + String(pirSensor_01_state ? "ALARM" : "OK") + String(" Altana: ") + String(pirSensor_02_state ? "ALARM" : "OK"))){
        alarmStatePir01 = pirSensor_01_state;
        alarmStatePir02 = pirSensor_02_state;
        if (!alarmStatePir01 && !alarmStatePir02){
          alarmState = false;
        }
        return true;   
      }
      else
        return false;          
    }  
    return true;
}
//-----------------------------------------------------------------  
bool modemRestart() {  
  lcd.setCursor(0,1);
  lcd.print("Modem restart..."); 
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DBG("Initializing modem...");
  bool res = modem.restart();
  lcd.setCursor(0,1);
  lcd.print(res ? "Restart OK      " : "Restart ERROR  ");  
  delay(200);   
  return res;
}
//-----------------------------------------------------------------
bool waitForNetwork() { 
  lcd.setCursor(0,1);
  lcd.print("Finding network.");           
  DBG("Waiting for network...");
  bool res = modem.waitForNetwork();
  lcd.setCursor(0,1);
  lcd.print(res ? "Network OK      " : "Network ERROR  ");  
  delay(200);   
  return res;
}
//------------------------------------------------------------------
void modemInit(){
  if (!modemRestart())
    modemInit();
  if(!waitForNetwork())
    modemInit();      
}
//-----------------------------------------------------------------
bool sendSMS(String msg) { 
  lcd.setCursor(0,1);
  lcd.print("SMS sending...  "); 

  SerialMon.println(msg);  
    
#ifdef SMS_TEL_NUMBER  
  bool res = modem.sendSMS(SMS_TEL_NUMBER, msg);  
  lcd.setCursor(0,1);
  lcd.print(res ? "SMS send: OK    " : "SMS send: FAIL  ");     
  DBG("SMS: ", res ? "OK" : "FAIL"  + " (" + msg + ")");
  smsCount++;
  delay(500);  
  lcd.setCursor(0,1);
  lcd.print("SMS count:      "); 
  lcd.setCursor(11,1);
  lcd.print(String(smsCount));
  return res;     
#else 
  lcd.setCursor(0,1);
  lcd.print("SMS send: INACT ");    
  DBG("SMS:", "INACTIVE");
  delay(500);  
  return true;   
#endif  
}
//-----------------------------------------------------------------
void printPirStatus(){
  lcd.setCursor(0,0);
  lcd.print(String("Narz: ") + String(pirSensor_01_state ? "AL" : "OK") + String(" Alt: ") + String(pirSensor_02_state ? "AL" : "OK"));  
}
//-----------------------------------------------------------------
//-----------------------------------------------------------------
void setup() {  
  pinMode(BUZZER_GPIO_PIN, OUTPUT);
  pinMode(BUILTIN_LED_GPIO_PIN, OUTPUT);
  pinMode(PIR_SENSOR1_GPIO_PIN, INPUT);
  pinMode(PIR_SENSOR2_GPIO_PIN, INPUT);
    
  lcd.init();  
  
  pirSensorsLoop(true);  
  alarmState = false;
  
  lcd.setCursor(0,1);
  lcd.print("Initialising... ");  

  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  // Set GSM module baud rate
  SerialAT.begin(19200);
  delay(500);

  modemInit();
}
//-----------------------------------------------------------------
void loop() {
  pirSensorsLoop();
  if(!alarmLoop())
    modemInit();      
}
