// demo: CAN-BUS Shield, receive data with interrupt mode, and set mask and filter

#include <Arduino.h>
#include <ESP8266WiFi.h>   
#include <ESP8266WebServer.h>
#include <DNSserver.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <SPI.h>
#include "mcp_can.h"
#define DEBUG
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#define OTA_DEBUG
#include "BK3254.h"
// #define BENCH


// #define LED_BUILTIN 2
// static const uint8_t D0   = 16;
// static const uint8_t D1   = 5;
// static const uint8_t D2   = 4;
// static const uint8_t D3   = 0;
// static const uint8_t D4   = 2;
// static const uint8_t D5   = 14;
// static const uint8_t D6   = 12;
// static const uint8_t D7   = 13;
// static const uint8_t D8   = 15;
// static const uint8_t RX   = 3;
// static const uint8_t TX   = 1;


//default values. Switchable via compiler options
#ifndef CAN_baud
  #define CAN_baud CAN_100KBPS
#endif
#define SPI_CS_PIN D8
#ifndef MCP_INT_PIN
  #define MCP_INT_PIN D2
#endif
#ifdef USE_SW_SERIAL
  SoftwareSerial swSerial(D2,D1); //rx,tx
  #define SerialForBT swSerial
#else
  #define SerialForBT Serial
#endif
#define SPI_CS_PIN D8
#define BT_reset D3 //dummy pin - not used currently
  
MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin
BK3254 BT(&SerialForBT, BT_reset); //Serial for controlling the BK3254
char str[20];
uint16_t myTime=0;
uint16_t timeout=500;

// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfigNeeded = false;
const char* Hostname = "BlueGenie";

#define MAX_SRV_CLIENTS 2
WiFiServer server(23);  // for telnet
WiFiClient serverClients[MAX_SRV_CLIENTS];
#define logger (&Serial)
#define STACK_PROTECTOR  512 // bytes


void init_WiFiManager(const char *Hostname){
  pinMode(LED_BUILTIN, OUTPUT);
  //WiFi.disconnect(); // forget SSID for debugging
  WiFi.printDiag(Serial); //Remove this line if you do not want to see WiFi password printed
  if (WiFi.SSID()==""){
    Serial.println("We haven't got any access point credentials, so get them now");   
    initialConfigNeeded = true;
  }
  else{
    digitalWrite(LED_BUILTIN, HIGH); // Turn led off as we are not in configuration mode.
    WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    unsigned long startedAt = millis();
    Serial.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis()- startedAt);
    Serial.print(waited/1000);
    Serial.print(" secs in setup() connection result is ");
    Serial.println(connRes);
  }
  
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("failed to connect, finishing setup anyway");
  } else{
    Serial.print("local ip: ");
    Serial.println(WiFi.localIP());
  }
}

void init_ArduinoOTA(const char *Hostname){
  ArduinoOTA.setHostname(Hostname);
    
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("End OTA");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

void ICACHE_RAM_ATTR MCP2515_ISR() {
;
}


void init_CAN()
{ 
  byte a=CAN.begin(MCP_STD, CAN_baud, MCP_8MHZ);
  if(a==CAN_OK ){     // init can bus : baudrate = 500k
    Serial.println("CAN BUS Shield init ok!" + a);
  }
  else
  {
    Serial.println("CAN BUS Shield init fail: " + a);
  }
  CAN.setMode(MCP_LISTENONLY);

  
  //attachInterrupt(digitalPinToInterrupt(MCP_INT_PIN), &MCP2515_ISR, FALLING); // interrupt not needed, since buffer reading logic is outside anyway
  pinMode(MCP_INT_PIN, INPUT);

  //https://github.com/dzid26/opendbc-BMW-E8x-E9x/blob/master/bmw_e9x_e8x.dbc
  CAN.init_Mask(0,0,0x07ff0000);                // Apply filter to all messages (don't let then be passed without looking at the filters below)
  CAN.init_Filt(0,0,0x01d60000);                // Init first filter...
  CAN.init_Filt(1,0,0x01d60000);                // Init second filter...
  
  CAN.init_Mask(1,0,0x07ff0000);                // Init second mask... 
  CAN.init_Filt(2,0,0x01d60000);                // Init third filter...
  CAN.init_Filt(3,0,0x01d60000);                // Init fouth filter...
  CAN.init_Filt(4,0,0x01d60000);                // Init fifth filter...
  CAN.init_Filt(5,0,0x01d60000);                // Init sixth filter...
}


void BletoothInitStates() {
  myTime=millis();
  while (BT.getName() != 1 && myTime - millis() < timeout);
  myTime=millis();
  while (BT.getConnectionStatus() != 1 && myTime - millis() < timeout);
  myTime=millis();
  while (BT.getPinCode() != 1 && myTime - millis() < timeout);
  myTime=millis();
  while (BT.getAddress() != 1 && myTime - millis() < timeout);
  myTime=millis();
  while (BT.getMusicStatus() != 1 && myTime - millis() < timeout);
  myTime=millis();
  while (BT.getHFPStatus() != 1 && myTime - millis() < timeout);
}

void allServersPrint(String s){ 
  for (int i = 0; i < MAX_SRV_CLIENTS; i++) 
    if (serverClients[i].availableForWrite()>0)
      serverClients[i].print(s);
}
void allServersPrintLn(String s=""){ allServersPrint(s + "\r\n");}










void setup()
{
  Serial.begin(9600); //for debugging and info

  BT.begin(); //it also begins Serial (TX0) to 9600
  init_CAN();

  init_WiFiManager(Hostname); //it writes on serial for debugging
  WiFi.hostname(Hostname);
  
  //start server telnet
  server.begin();
  server.setNoDelay(true);
  
  init_ArduinoOTA(Hostname);
  
  //Serial.flush();
  // Serial.set_tx(2); //swappes to TX1 to write to bluetooth module
  
  BletoothInitStates();
  delay(100);
  myTime=millis();
  BT.voicesOff();
  BT.changeName("BlueGenie BMW");
  delay(100);
  BT.connectLastDevice();
  //BT.volumeSet("0x00"); 
  BT.volumeGet();
}


void handlingTelnetComm()
{
  //check if there are any new clients
  if (server.hasClient()) {
    //find free/disconnected spot
    int i;
    for (i = 0; i < MAX_SRV_CLIENTS; i++)
      if (!serverClients[i]) { // equivalent to !serverClients[i].connected()
        serverClients[i] = server.available();
        logger->print("New client: index ");
        logger->print(i);
        break;
      }

    //no free/disconnected spot so reject
    if (i == MAX_SRV_CLIENTS) {
      server.available().println("busy");
      // hints: server.available() is a WiFiClient with short-term scope
      // when out of scope, a WiFiClient will
      // - flush() - all data will be sent
      // - stop() - automatically too
      logger->printf("server is busy with %d active connections\n", MAX_SRV_CLIENTS);
    }
  }

    //check TCP clients for data
  #if 1
    // Incredibly, this code is faster than the bufferred one below - #4620 is needed
    // loopback/3000000baud average 348KB/s
    for (int i = 0; i < MAX_SRV_CLIENTS; i++)
      while (serverClients[i].available()) {//&& swSerial.availableForWrite() > 0) {
        // working char by char is not very efficient
        SerialForBT.write(serverClients[i].read());
      }
  #else
    // loopback/3000000baud average: 312KB/s
    for (int i = 0; i < MAX_SRV_CLIENTS; i++)
      while (serverClients[i].available() && Serial.availableForWrite() > 0) {
        size_t maxToSerial = std::min(serverClients[i].available(), Serial.availableForWrite());
        maxToSerial = std::min(maxToSerial, (size_t)STACK_PROTECTOR);
        uint8_t buf[maxToSerial];
        size_t tcp_got = serverClients[i].read(buf, maxToSerial);
        size_t serial_sent = Serial.write(buf, tcp_got);
        if (serial_sent != maxToSerial) {
          logger->printf("len mismatch: available:%zd tcp-read:%zd serial-write:%zd\n", maxToSerial, tcp_got, serial_sent);
        }
      }
  #endif

  // determine maximum output size "fair TCP use"
  // client.availableForWrite() returns 0 when !client.connected()
  size_t maxToTcp = 0;
  for (int i = 0; i < MAX_SRV_CLIENTS; i++)
    if (serverClients[i]) {
      size_t afw = serverClients[i].availableForWrite();
      if (afw) {
        if (!maxToTcp) {
          maxToTcp = afw;
        } else {
          maxToTcp = std::min(maxToTcp, afw);
        }
      } else {
        // warn but ignore congested clients
        logger->println("one client is congested");
      }
    }

  //check UART for data
  size_t len = std::min((size_t)Serial.available(), maxToTcp);
  len = std::min(len, (size_t)STACK_PROTECTOR);
  if (len) {
    uint8_t sbuf[len];
    size_t serial_got = Serial.readBytes(sbuf, len);
    // push UART data to all connected telnet clients
    for (int i = 0; i < MAX_SRV_CLIENTS; i++)
      // if client.availableForWrite() was 0 (congested)
      // and increased since then,
      // ensure write space is sufficient:
      if (serverClients[i].availableForWrite() >= serial_got) {
        size_t tcp_sent = serverClients[i].write(sbuf, serial_got);
        if (tcp_sent != len) {
          logger->printf("len mismatch: available:%zd serial-read:%zd tcp-write:%zd\n", len, serial_got, tcp_sent);
        }
      }
  }
}


void printBTstate() {
  switch (BT.BTState) {
    case BT.Connected:
      allServersPrintLn("Bluetooth connected");break;
    case BT.Disconnected:
      allServersPrintLn("Bluetooth disconnected");break;
    case BT.Pairing:
      allServersPrintLn("Bluetooth in pairing mode");break;
  }
}

void wifiConfigureOnEvent(){
  
    //Local intialization. Once its business is done, there is no need to keep it around
    Serial.println("Configuration portal requested.");
    WiFiManager wifiManager;

    //sets timeout in seconds until configuration portal gets turned off.
    //If not specified device will remain in configuration mode until
    //switched off via webserver or device is restarted.
    //wifiManager.setConfigPortalTimeout(600);

    //it starts an access point 
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.startConfigPortal(Hostname, NULL)) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      initialConfigNeeded = false;
      //if you get here you have connected to the WiFi
      initialConfigNeeded = false;
      Serial.println("connected...yeey :)");
    }
    digitalWrite(LED_BUILTIN, HIGH); // Turn led off as we are not in configuration mode.
    ESP.reset(); // This is a bit crude. For some unknown reason webserver can only be started once per boot up 
    // so resetting the device allows to go back into config mode again when it reboots.
    delay(5000);
}










boolean nextUpButton = false;
boolean nextUpButton_last = false;
boolean prevDnButton = false;
boolean prevDnButton_last = false;
boolean volUpButton = false;
boolean volUpButton_last = false;
boolean volDnButton = false;
boolean volDnButton_last = false;


uint16_t BTState;
void loop()
{
  BT.getNextEventFromBT();
  ArduinoOTA.handle();
  if (BTState != BT.BTState) {
    printBTstate();
    BTState = BT.BTState;
  }
  handlingTelnetComm();






  long unsigned int canId;
  unsigned char message_length = 0;
  unsigned char message_buffer[8];
  String messageText;
  if(!digitalRead(MCP_INT_PIN))                   // check if get data
  {
      CAN.readMsgBuf(&canId, &message_length, message_buffer);    // read data,  len: data length, buf: data buf
      //Print raw messages
      messageText = "\r\n---------------------------\r\n" + String(canId) + ":  0x";
      for(int j = 0; j<message_length; j++)    // print the data
        messageText += String(message_buffer[j], HEX) + "\t";
        #ifdef DEBUG
          Serial.println(messageText);
        #endif
      allServersPrintLn(messageText);

      //Extract messages
      if (canId == 470)
        if (message_buffer[0] != 0xFF){ //indicates steering buttons error
          nextUpButton = bitRead(message_buffer[0], 5);
          prevDnButton = bitRead(message_buffer[0], 4);
          volUpButton = bitRead(message_buffer[0], 3);
          volDnButton = bitRead(message_buffer[0], 2);
        }
  }

  if ((prevDnButton && volDnButton) || initialConfigNeeded){ //add VehSpd==0 condition
    allServersPrintLn("WiFi config requested");
    wifiConfigureOnEvent(); //two down buttons held - configure WiFi connection. Stops anything else
  }
  else if (nextUpButton < nextUpButton_last){ //falling edge
    BT.musicNextTrack();
    allServersPrintLn("BT next requested");
  }
  else if (prevDnButton < prevDnButton_last)  //falling edge
  {
    BT.musicPreviousTrack();
    allServersPrintLn("BT previous requested");
  }else {
    allServersPrint(".");
  }
  


  nextUpButton_last = nextUpButton;
  prevDnButton_last = prevDnButton;
  volUpButton_last = volUpButton;
  volDnButton_last = volDnButton;
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/