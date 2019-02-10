// demo: CAN-BUS Shield, receive data with interrupt mode, and set mask and filter

#include <Arduino.h>
#include <ESP8266WiFi.h>   
#include <ESP8266WebServer.h>
#include <DNSserver.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <SPI.h>
#include "mcp_can.h"
#define OTA_DEBUG
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
//#define DEBUG
#define USE_SW_SERIAL
#include <BK3254.h>

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = D8;
const int MCP_INT_PIN = D0; //D4, D3, D8 can't be hight at boot, D0 can't be used as interrupt
uint8_t BT_reset = D3; //dummy pin - not used currently
MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin

SoftwareSerial swSerial(D2,D1); //rx,tx
BK3254 BT(&swSerial, BT_reset); //Serial for controlling the BK3254

bool flagRecv = false;
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
  //WiFi.disconnect(); // forget SSID for debigging
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
// void MCP2515_ISR()
// {
//     flagRecv = true;
// }

void init_CAN()
{
  for (int i=0; ( CAN.begin(CAN_100KBPS,MCP_8MHz)!=CAN_OK && i<11); i++)       // init can bus : baudrate = 500k
  {
      Serial.println("CAN BUS Shield init fail");
      Serial.println("Init CAN BUS Shield again");
      delay(100);
  }
  Serial.println("CAN BUS Shield init ok!");

  //attachInterrupt(digitalPinToInterrupt(MCP_INT_PIN), &MCP2515_ISR, FALLING); // interrupt not needed, since buffer reading logic is outside anyway
  pinMode(MCP_INT_PIN, INPUT);

  /*
    * set mask, set both the mask to 0x3ff
    */
  CAN.init_Mask(0, 0, 0x3ff);                         // there are 2 mask in mcp2515, you need to set both of them
  CAN.init_Mask(1, 0, 0x3ff);


  /*
    * set filter, we can receive id from 0x04 ~ 0x09
    */
  CAN.init_Filt(0, 0, 470);                          // there are 6 filter in mcp2515
  CAN.init_Filt(1, 0, 470);                          // there are 6 filter in mcp2515

  CAN.init_Filt(2, 0, 470);                          // there are 6 filter in mcp2515
  CAN.init_Filt(3, 0, 470);                          // there are 6 filter in mcp2515
  CAN.init_Filt(4, 0, 470);                          // there are 6 filter in mcp2515
  CAN.init_Filt(5, 0, 470);                          // there are 6 filter in mcp2515
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
  Serial.begin(115200); //for debugging and info

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
      swSerial.write(serverClients[i].read());
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







  unsigned char message_length = 0;
  unsigned char message_buffer[8];
  String messageText;
  if(!digitalRead(MCP_INT_PIN))                   // check if get data
  {
      CAN.readMsgBuf(&message_length, message_buffer);    // read data,  len: data length, buf: data buf
      //Print raw messages
      messageText = "\r\n---------------------------\r\n" + String(CAN.getCanId()) + ":  0x";
      for(int j = 0; j<message_length; j++)    // print the data
        messageText += String(message_buffer[j], HEX) + "\t";
      allServersPrintLn(messageText);
      Serial.println(messageText);

      //Extract messages
      if (CAN.getCanId() == 470)
        if (message_buffer[0] != 0xFF) //indicates steering buttons error
          nextUpButton = bitRead(message_buffer[0], 5);
          prevDnButton = bitRead(message_buffer[0], 4);
          volUpButton = bitRead(message_buffer[0], 3);
          volDnButton = bitRead(message_buffer[0], 2);
  }

  if ((nextUpButton && volUpButton) || initialConfigNeeded) //add VehSpd==0 condition
    wifiConfigureOnEvent(); //two up buttons hel - configure WiFi connection. Stops anything else
  else if (nextUpButton < nextUpButton_last) //falling edge
    BT.musicNextTrack();
  else if (prevDnButton < prevDnButton_last)  //falling edge
  {
    BT.musicPreviousTrack();
  }else 
    allServersPrint(".");
  


  nextUpButton_last = nextUpButton;
  prevDnButton_last = prevDnButton;
  volUpButton_last = volUpButton;
  volDnButton_last = volDnButton;
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/