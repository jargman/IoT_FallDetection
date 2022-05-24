
// import Wire and WiFi package for easier manipulation of registry values and wifi config, respectively
#include <Wire.h>
#include <ESP8266WiFi.h>

/* initialise global variables */
/* --------------------------- */

// initialising the infrared readings, assigning as int as measurements vary from ~20 - 1000
int ir_reading = 0; 

// I2C address of the MPU-6050
const int MPU=0x68;    

// X + Y + Z axes for accelerometer and gyroscope
int16_t acc_x,acc_y,acc_z,temp,gyro_x,gyro_y,gyro_z;  

// 'cleaned' variables of the above
float a_x=0, a_y=0, a_z=0, g_x=0, g_y=0, g_z=0;       

// localised chunks to identify a potential fall
bool fall_detected = false; 
bool trig_low_thresh=false; 
bool trig_high_thresh=false; 
bool trig_orientation=false;

// number of ticks after respective trigger
byte trig_low_thresh_count=0;  
byte trig_high_thresh_count=0; 
byte trig_orientation_count=0;
int gyro_rotation=0;

// wifi details (pls don't dox me)
const char *ssid =  "Yodafone4G";     
const char *pass =  "waterycoconut778"; 

// if-this-then-that (IFTTT) is the website hosting the alert capability
// the connection is created over wifi via API which requires a private key, assigned below
void send_event(const char *event);
const char *host = "maker.ifttt.com";
const char *privateKey = "v7ukvAXTERqbeJ26GfGgKlMaOx1KvJUsHFi5MDKo2c";


void setup(){

  // assigning input from A0 pin to measure infrared proximity recordings
  /* ---------------------------------------- */
  pinMode(A0,INPUT);   
  
  // set baud rate, wake up MPU 
  /* ---------------------------------------- */
  Serial.begin(9600);
  Wire.begin();                      // Initialize comunication
  Wire.beginTransmission(MPU);       // Start communication with MPU6050 // MPU=0x68
  Wire.write(0x6B);                  // Talk to the register 6B
  Wire.write(0x00);                  // Make reset - place a 0 into the 6B register
  Wire.endTransmission(true);        //end the transmission
 
  // initial connection to wifi
  /* ---------------------------------------- */
  WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
      {delay(500);
      Serial.print(".");              
      }
    Serial.println("");
    Serial.println("WiFi connected");
 }


 
void loop(){

/* extract core value from MPU registry*/
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);

//taking IR reading from A0 input pin
  ir_reading =  analogRead(A0); 

/*bit wise movements to resolve and assign correct values from 'whole' value above */
  Wire.requestFrom(MPU,14,true);    // request a total of 14 registers
    acc_x  = Wire.read()<<8|Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
    acc_y  = Wire.read()<<8|Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    acc_z  = Wire.read()<<8|Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    temp   = Wire.read()<<8|Wire.read(); // 0x41 (TEMP_OUT_H)   & 0x42 (TEMP_OUT_L)
    gyro_x = Wire.read()<<8|Wire.read(); // 0x43 (GYRO_XOUT_H)  & 0x44 (GYRO_XOUT_L)
    gyro_y = Wire.read()<<8|Wire.read(); // 0x45 (GYRO_YOUT_H)  & 0x46 (GYRO_YOUT_L)
    gyro_z = Wire.read()<<8|Wire.read(); // 0x47 (GYRO_ZOUT_H)  & 0x48 (GYRO_ZOUT_L)

// clean accelerometer/gyroscope readings 
   a_x = (acc_x-2050)/16384.00;
   a_y = (acc_y-77)/16384.00;
   a_z = (acc_z-1947)/16384.00;
   g_x = (gyro_x+270)/131.07;
   g_y = (gyro_y-351)/131.07;
   g_z = (gyro_z+136)/131.07;
 
 /* accelerometer: distance in point-to-point (p2p) movement  */
 float p2p = pow(pow(a_x,2)+pow(a_y,2)+pow(a_z,2),0.5);

/* gyroscope: measuring rotational spin across all 3 axes */
 float gyro_rotation = pow(pow(g_x,2)+pow(g_y,2)+pow(g_z,2),0.5); 
 
 /* now scale values so they are more distinguishable */
 int p2p_clean = p2p * 10; 

/* for debugging
 Serial.print(a_x); Serial.print(" -- ");
 Serial.print(a_y); Serial.print(" -- ");
 Serial.print(a_z); Serial.print(" -- ");
 Serial.print(g_x); Serial.print(" -- ");
 Serial.print(g_y); Serial.print(" -- ");
 Serial.print(g_z); Serial.print(" -- ");
 Serial.println(p2p_clean);
*/
 
/* identify when accelerometer suddenly stops */
 Serial.println(p2p_clean);
 if (p2p_clean<=3 && trig_high_thresh==false){
   trig_low_thresh=true;
   Serial.println("SUDDEN STOP");  }

   
/* identify when accelerometer quickly moves */
 if (trig_low_thresh==true){
   trig_low_thresh_count++;
   if (p2p_clean>=6){ 
     trig_high_thresh=true;
     Serial.println("SUDDEN MOVE");
     trig_low_thresh=false; 
   trig_low_thresh_count=0;
    }}
 
 
 /* identify if gyroscope records relative change in position (total sum across three dimensions) of 90 degrees */
 if (trig_high_thresh==true){
   trig_high_thresh_count++;
   Serial.println(gyro_rotation);
   if (gyro_rotation>=15 && gyro_rotation<=270){ // 3 angles * 90 degrees possible rotation = max 270
     trig_orientation=true; 
   trig_high_thresh_count=0;
     Serial.println(gyro_rotation);
     Serial.println("GYRO SPIN");
}}


/* if (sudden stop or sudden move) AND rotation detected, send alert */
 if (((trig_high_thresh == true) || (trig_low_thresh == true)) 
  && (trig_orientation == true)){
   Serial.println("FALL DETECTED");

/* now connect to IFTTT with alert package */
/* sourced from: https://stackoverflow.com/questions/39707504/how-to-use-esp8266wifi-library-get-request-to-obtain-info-from-a-website */
          Serial.print("Connecting to "); 
          Serial.println(host);
          WiFiClient client;
          const int httpPort = 80;
          if (!client.connect(host, httpPort)) {
            Serial.println("Connection failed");
            return;
          }
          String url = "/trigger/";
          url += "FALL_DETECTED";
          url += "/with/key/";
          url += privateKey;
          Serial.print("Requesting URL: ");
          Serial.println(url);
          client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                       "Host: " + host + "\r\n" + 
                       "Connection: close\r\n\r\n");
          while(client.connected())
          { if(client.available())
            { String line = client.readStringUntil('\r');
              Serial.print(line);
            } else {
              delay(50);
            };
          }
          Serial.println();
          Serial.println("closing connection");
          client.stop();

/* reset counters so the alert doesn't get stuck in loop */
   fall_detected=false;
   trig_high_thresh=false;
   trig_low_thresh=false; 
   trig_orientation=false;
   }

/* allow 3 seconds (6 loops of 0.5 seconds) for triggers to be deactivated if false-positive */   
 if (trig_high_thresh_count>=6){
   trig_high_thresh=false; 
   trig_high_thresh_count=0;
   Serial.println("HIGH THRESH - DEACTIVATED");
   }
 if (trig_low_thresh_count>=6){ 
   trig_low_thresh=false; 
   trig_low_thresh_count=0;
   Serial.println("LOW THRESH - DEACTIVATED");
   }
  delay(500);
}
