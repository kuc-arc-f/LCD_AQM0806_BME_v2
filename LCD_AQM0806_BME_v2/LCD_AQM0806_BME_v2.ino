#include <ESP8266WiFi.h>
#include <BME280_MOD-1022.h>
#include <Wire.h>
const char* ssid = "";
const char* password = "";

const char* mHost     = "api.thingspeak.com";
const char* mHostTime = "dn1234.com";
String mAPI_KEY="";

//LCD
#define ADDR 0x3e
uint8_t CONTROLL[] = {
  0x00, // Command
  0x40, // Data
};

const int C_COMM = 0;
const int C_DATA = 1;

const int LINE = 2;
String buff[LINE];
//
int mMode=0;
int mMode_TMP =0;
int mMode_HUM =1;
//int mMode_PRE =2;

float mTemp=0;
float mHumidity=0;
float mPressure=0;

uint32_t mNextHttp= 30000;
uint32_t mTimerTmpInit=30000;
uint32_t mTimerDisp;
uint32_t mTimerTime;
uint32_t mTimerTemp;

//String mReceive="";
String mTimeStr="0000";

// Arduino needs this to pring pretty numbers
void printFormattedFloat(float x, uint8_t precision) {
char buffer[10];

  dtostrf(x, 7, precision, buffer);
  Serial.print(buffer);

}

// print out the measurements
void printCompensatedMeasurements(void) {

//float temp, humidity,  pressure, pressureMoreAccurate;
float  pressureMoreAccurate;
double tempMostAccurate, humidityMostAccurate, pressureMostAccurate;
char buffer[80];

//  temp      = BME280.getTemperature();
//  humidity  = BME280.getHumidity();
//  pressure  = BME280.getPressure();
  mTemp      = BME280.getTemperature();
  mHumidity  = BME280.getHumidity();
  mPressure  = BME280.getPressure();
  
  pressureMoreAccurate = BME280.getPressureMoreAccurate();  // t_fine already calculated from getTemperaure() above
  
  tempMostAccurate     = BME280.getTemperatureMostAccurate();
  humidityMostAccurate = BME280.getHumidityMostAccurate();
  pressureMostAccurate = BME280.getPressureMostAccurate();
  Serial.println("                Good  Better    Best");
  Serial.print("Temperature  ");
  printFormattedFloat(mTemp, 2);
  Serial.print("         ");
  printFormattedFloat(tempMostAccurate, 2);
  Serial.println();
  
  Serial.print("Humidity     ");
  printFormattedFloat(mHumidity, 2);
  Serial.print("         ");
  printFormattedFloat(humidityMostAccurate, 2);
  Serial.println();

  Serial.print("Pressure     ");
  printFormattedFloat(mPressure, 2);
  Serial.print(" ");
  printFormattedFloat(pressureMoreAccurate, 2);
  Serial.print(" ");
  printFormattedFloat(pressureMostAccurate, 2);
  Serial.println();
}

//
void init_BME280(){
  uint8_t chipID;
  
  Serial.println("Welcome to the BME280 MOD-1022 weather multi-sensor test sketch!");
  Serial.println("Embedded Adventures (www.embeddedadventures.com)");
  chipID = BME280.readChipId();
  
  // find the chip ID out just for fun
  Serial.print("ChipID = 0x");
  Serial.print(chipID, HEX);
  
 
  // need to read the NVM compensation parameters
  BME280.readCompensationParams();
  
  // Need to turn on 1x oversampling, default is os_skipped, which means it doesn't measure anything
  BME280.writeOversamplingPressure(os1x);  // 1x over sampling (ie, just one sample)
  BME280.writeOversamplingTemperature(os1x);
  BME280.writeOversamplingHumidity(os1x);
  
  // example of a forced sample.  After taking the measurement the chip goes back to sleep
  BME280.writeMode(smForced);
  while (BME280.isMeasuring()) {
    Serial.println("Measuring...");
    delay(50);
  }
  Serial.println("Done!");
  
  // read out the data - must do this before calling the getxxxxx routines
  BME280.readMeasurements();
  Serial.print("Temp=");
  Serial.println(BME280.getTemperature());  // must get temp first
  Serial.print("Humidity=");
  Serial.println(BME280.getHumidity());
  Serial.print("Pressure=");
  Serial.println(BME280.getPressure());
  Serial.print("PressureMoreAccurate=");
  Serial.println(BME280.getPressureMoreAccurate());  // use int64 calculcations
  Serial.print("TempMostAccurate=");
  Serial.println(BME280.getTemperatureMostAccurate());  // use double calculations
  Serial.print("HumidityMostAccurate=");
  Serial.println(BME280.getHumidityMostAccurate()); // use double calculations
  Serial.print("PressureMostAccurate=");
  Serial.println(BME280.getPressureMostAccurate()); // use double calculations
  
  // Example for "indoor navigation"
  // We'll switch into normal mode for regular automatic samples
  
  BME280.writeStandbyTime(tsb_0p5ms);        // tsb = 0.5ms
  BME280.writeFilterCoefficient(fc_16);      // IIR Filter coefficient 16
  BME280.writeOversamplingPressure(os16x);    // pressure x16
  BME280.writeOversamplingTemperature(os2x);  // temperature x2
  BME280.writeOversamplingHumidity(os1x);     // humidity x1
  
  BME280.writeMode(smNormal);
}
//
uint8_t settings[] = {
  0b00111001, // 0x39 // [Function set] DL(4):1(8-bit) N(3):1(2-line) DH(2):0(5x8 dot) IS(0):1(extension)
  // Internal OSC frequency
  (0b0001 << 4) + 0b0100, // BS(3):0(1/5blas) F(210):(internal Freq:100)
  // Contrast set
  (0b0111 << 4) + 0b0100, // Contrast(3210):4
  // Power/ICON/Contrast control
  (0b0101 << 4) + 0b0110, // Ion(3):0(ICON:off) Bon(2):1(booster:on) C5C4(10):10(contrast set)
  // Follower control
  (0b0110 << 4) + 0b1100, // Fon(3):1(on) Rab(210):100
  // Display ON/OFF control
  (0b00001 << 3) + 0b111, // D(2):1(Display:ON) C(1):1(Cursor:ON) B(0):1(Cursor Blink:ON)
};

//
void proc_httpTime(){
//Serial.print("connecting to ");
//Serial.println(host);  
      WiFiClient client;
      const int httpPort = 80;
      if (!client.connect(mHostTime, httpPort)) {
        Serial.println("connection failed");
        return;
      }
      String url = "/test_7seg4_0109a.php?mc_id=1&tmp=0"; 
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
        "Host: " + mHostTime + "\r\n" + 
        "Connection: close\r\n\r\n");
      delay(10);      
      int iSt=0;
      while(client.available()){
          String line = client.readStringUntil('\r');
//Serial.print(line);
          int iSt=0;
          iSt = line.indexOf("res=");
          if(iSt >= 0){
              iSt = iSt+ 4;
              String sDat = line.substring(iSt );
Serial.print("sDat[TM]=");
Serial.println(sDat);
              if(sDat.length() >= 4){
                  mTimeStr = sDat.substring(0, 4);
              }
          }
      }  // end_while  
}
//
//void proc_http(String sTemp, String sHum){
void proc_http(String sTemp, String sHum, String sPre){
//Serial.print("connecting to ");
//Serial.println(mHost);  
      WiFiClient client;
      const int httpPort = 80;
      if (!client.connect(mHost, httpPort)) {
        Serial.println("connection failed");
        return;
      }
      String url = "/update?key="+ mAPI_KEY + "&field1="+ sTemp +"&field2=" + sHum+ "&field3="+sPre;
//      String url = "/update?key="+ mAPI_KEY + "&field1="+ sTemp; 
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
        "Host: " + mHost + "\r\n" + 
        "Connection: close\r\n\r\n");
      delay(10);      
      int iSt=0;
      while(client.available()){
          String line = client.readStringUntil('\r');
Serial.print(line);
      }    
}
//
void setup() {  
  mTimerTemp= mTimerTmpInit;
  Serial.begin(9600);
  Serial.println("# Start-setup");

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);  
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  
  Wire.begin(); // (SDA,SCL)
  // LCD初期化
  write(C_COMM, settings, sizeof(settings));
  init_BME280();
}

//
void loop() {
    delay(100);
    while (BME280.isMeasuring()) {
    }
    String sTmp="";
    String sHum="";
    String sPre="";  
    if (millis() > mTimerDisp) {
        mTimerDisp = millis()+ 3000;    
        // read out the data - must do this before calling the getxxxxx routines
        BME280.readMeasurements();
        printCompensatedMeasurements();
        
        String line_1 ="00:00,";
        String line_2 ="Temp:";
        int itemp  =(int)mTemp;   
        int iHum   = (int)mHumidity;   
        int iPress = (int)mPressure;          
        sTmp=String(itemp);
        sHum=String(iHum);
        
        sPre=String(iPress);
        //init
        for (int i = 0; i < LINE ; i++) {
          buff[i]="";
        }
        //disp
        if(mMode== mMode_TMP){
          line_1 =mTimeStr.substring(0,2)+":";
          line_1+=mTimeStr.substring(2,4);         
          line_2 = "Temp:"+ sTmp+"C";
          mMode=mMode+1;
        }else if(mMode== mMode_HUM){
          line_1 = "Hum:"+ sHum+ "%";
          line_2 =sPre+ "hPa";
          mMode=mMode_TMP;
        }
        /*
        else{
          line_2 =sPre+ "hPa";
          mMode=mMode_TMP;
        }
        */
          buff[0]=line_1;
          buff[1]=line_2;
          for (int i = 0; i < LINE ; i++) {
              print2line();
Serial.print("buf[");
Serial.print(i);
Serial.print("]=");
Serial.println(buff[i]);
          char sResT[2+1];
          char sResH[3+1];
          sprintf(sResT, "%02d", itemp);
          sprintf(sResH, "%03d", iHum);
//Serial.print("res=");
//Serial.print(sResT);
//Serial.println(sResH);
//Serial.print("TIME=");
//Serial.println(line_1);
          }
    } // if_timeOver
    if (millis() > mTimerTime ) {
      mTimerTime = millis()+ mNextHttp;
      proc_httpTime();
      delay(1000);    
    }
    if (millis() > mTimerTemp) {
      mTimerTemp = millis()+ mNextHttp+ mTimerTmpInit;
      int itemp  =(int)mTemp;   
      int iHum   = (int)mHumidity;   
      int iPress = (int)mPressure;          
      sTmp=String(itemp);
      sHum=String(iHum);
      sPre=String(iPress);      
      proc_http(sTmp, sHum, sPre);
      delay(1000);
    }    
}

// print
void print2line() {
  uint8_t cmd[] = {0x01};
  write(C_COMM, cmd, sizeof(cmd));
  delay(1);

  for (int i = 0; i < LINE; i++) {
    uint8_t pos = 0x80 | i * 0x40;
    uint8_t cmd[] = {pos};
    write(C_COMM, cmd, sizeof(cmd));
    write(C_DATA, (uint8_t *)buff[i].c_str(), buff[i].length());
  }
}

//
void write(int type, uint8_t *data, size_t len) {
  for (int i=0; i < len; i++) {
    Wire.beginTransmission(ADDR);
    Wire.write(CONTROLL[type]);
    Wire.write(data[i]);
    Wire.endTransmission();
    delayMicroseconds(27);
  }
}



