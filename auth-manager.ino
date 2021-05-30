#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>

#define SS_PIN D2 
#define RST_PIN D1

#define LED D0 
#define BUZZER D8 

MFRC522 mfrc522(SS_PIN, RST_PIN);

const char* ssid     = "janithWLAN";
const char* password = "janithWLAN..";
  
WebSocketsServer websockets(81);


String StrUID="";

boolean isAuthenticate;
boolean isAnEmployee;
boolean isAssignTag;
DynamicJsonDocument  doc(1024);
String reqUid;
uint8_t responceNum;


void setup() {
  Serial.begin(115200);

  pinMode(LED,OUTPUT);
  pinMode(BUZZER,OUTPUT);

  Serial.print("Connecting to wifi..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());

  websockets.begin();
  Serial.print("Websocket Started");
  websockets.onEvent(webSocketEvent);
  
  SPI.begin();      //--> Init SPI bus
  mfrc522.PCD_Init(); //--> Init MFRC522 card
}

// the loop function runs over and over again forever
void loop() {
  websockets.loop();
  if(isAuthenticate){
    authenticateEmployee(reqUid,responceNum);
  }
  if(isAnEmployee){
    checkIsAnEmployee();
  }
  if(isAssignTag){
    assignTag();
  }
}

//read rfid card
int getid() {
  digitalWrite(LED,HIGH);
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }
  
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  mfrc522.PICC_HaltA();
  return 1;
}


//generate and assign uid to StarUID variable
void dump_byte_array(byte *buffer, byte bufferSize) {
    StrUID="";
    for (byte i = 0; i < bufferSize; i++) {
        StrUID.concat(String(buffer[i] < 0x10 ? " 0" : " "));
        StrUID.concat(String(buffer[i], HEX));
    }
    StrUID.toUpperCase();
    StrUID.trim();
    Serial.print("THE UID OF THE SCANNED CARD IS : ");
    Serial.println(StrUID);
}

//buzzer beep for one sec when user authentication fail
void handleFail(){
  digitalWrite(BUZZER,HIGH);
  delay(1000);
  digitalWrite(BUZZER,LOW);
}

//success buzzer beep when user authentication success
void handleSuccess(){
  digitalWrite(BUZZER,HIGH);
  delay(100);
  digitalWrite(BUZZER,LOW);
  delay(30);
  digitalWrite(BUZZER,HIGH);
  delay(150);
  digitalWrite(BUZZER,LOW);
  digitalWrite(LED,LOW);
}

//JSON generator
DynamicJsonDocument converToJson(String message){
 
  deserializeJson(doc, message);

  return doc;
}

//
void checkIsAnEmployee(){
 getid();
 int len=doc["uidList"].size();
 if(StrUID!=""){
  boolean exist=false;
  for(int i=0;i<len;i++){
    String uid=doc["uidList"][i];
    if(StrUID==uid){
      exist=true;
      handleSuccess();
      isAnEmployee=false;
      websockets.sendTXT(responceNum, uid);
      StrUID="";
      break;
    }
  }
  if(!exist){
    handleFail();
    StrUID="";
  }
 }
}


//check if a tag exist on given tag lis -> used to check if theres any employee for given tag
void assignTag(){
 getid();
 int len=doc["uidList"].size();
 if(StrUID!=""){
  boolean exist=false;
  for(int i=0;i<len;i++){
    String uid=doc["uidList"][i];
    if(StrUID==uid){
      exist=true;
    }
  }
  if(!exist){
    handleSuccess();
    isAssignTag=false;
    websockets.sendTXT(responceNum, StrUID);
    StrUID="";
  }
  if(exist){
    handleFail();
    StrUID="";
  }
 }
}

//authenticate partiular employee by uid
void authenticateEmployee(String uid,uint8_t num){
  uid.toUpperCase();
  getid();
  if(StrUID!=""&&String(StrUID)==(String(uid))){
      handleSuccess();
      isAuthenticate=false;
      websockets.sendTXT(num, uid);
      StrUID="";
   }
  else if(StrUID!=""){
    handleFail();
    StrUID="";
  }
}

//handle client request payload
void handleRequest(DynamicJsonDocument root,uint8_t num){
  
  String operation=root["operation"];
  if(operation=="AUTHENTICATE"){
    String uid=root["uid"];
    reqUid=uid;
    responceNum=num;
    isAuthenticate=true;
  }
  if(operation=="IS_AN_EMPLOYEE"){
    responceNum=num;
    isAnEmployee=true;
  }
  if(operation=="ASSIGN_TAG"){
    responceNum=num;
    isAssignTag=true;
  }
  if(operation=="TERMINATE"){
    isAuthenticate=false;
    isAnEmployee=false;
    isAssignTag=false;
    digitalWrite(LED,LOW);
  }
}

//handle websocket events
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) 
  {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = websockets.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:{
      Serial.printf("[%u] get Text: %s\n", num, payload);
      String message = String((char*)( payload));
      DynamicJsonDocument root = converToJson(message);
      handleRequest(root,num);
      break;      
    }
  }
}
