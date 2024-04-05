//stable 0.1.3

#include <iostream>
#include <string>

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>  //Udp client npt time server
#include <NTPClient.h>
#include <ArduinoJson.h>  // Include ArduinoJson library

const char *ssid = "DREAMMAKER";
const char *password = "liha4488";
char server[] = "api.attendora.com";
uint16_t port = 80;
char path[] = "/createSession";
char path1[] = "/getstudents";
char path2[] = "/markAbsent";


#define RST_PIN 3
#define SS_PIN 2

String arduinoNum = "111";
String arduinoPass = "AWALarduino123";
int internalCount = 0;
JsonDocument studentsDoc;

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient client;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 1);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println(F("Read and fetch teacher's data using RFID:"));
  lcd.init();
  lcd.backlight();
  WiFi.begin(ssid, password);
  lcd.setCursor(2,0);
  lcd.print("Connecting");
  lcd.setCursor(3,1);
  lcd.print("To wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("Connected");
  delay(1500);
  lcd.clear();
  timeClient.begin();
}

void loop() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte block;
  byte len;
  MFRC522::StatusCode status;

  if (!mfrc522.PICC_IsNewCardPresent()) {
    lcd.setCursor(3, 0);
    lcd.print("Reading...");
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println(F("Card Detected:"));

  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

  byte buffer1[18];
  block = 4;  // Adjust if needed
  len = 18;

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(block, buffer1, &len);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  Serial.print(len);
  String teacherName = "";

  for (uint8_t i = 1; i < 16; i++) {
    teacherName += (char)buffer1[i];
  }

  byte buffer2[18];
  block = 1;  // Adjust if needed
  len = 18;

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(block, buffer2, &len);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Construct the JSON payload with the RFID tag ID
  uint8_t i = 0;
  String tagId = "";
  String response = "";

  for (uint8_t i = 0; i < 5; i++) {
    tagId += (char)buffer2[i];
  }

  if (!tagId.startsWith("0")) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(teacherName);
    lcd.setCursor(0, 1);
    lcd.print("Not a teacher!");
    delay(2000);
    lcd.clear();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hello Professor");
  lcd.setCursor(0, 1);
  lcd.print(teacherName);

  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Prof: " + teacherName);
  lcd.setCursor(0, 1);
  lcd.print("Creating Session");


  Serial.println();
  Serial.print("tag content" + tagId);

  String jsonPayload = "{\"arduinoId\":\"" + arduinoNum + "\",\"teacherId\":\"" + tagId + "\"}";

  // Print the JSON payload to Serial Monitor
  Serial.println("JSON Payload: " + jsonPayload);

  // Make a POST request to the API using WiFiClient
  if (client.connect(server, port)) {
    client.println("POST " + String(path) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(jsonPayload.length());
    client.println();
    client.println(jsonPayload);

    // Wait for the server to respond
    while (!client.available()) {
      delay(10);
    }
    String line;

    bool jsonStart = false;
    // Read and print the response from the server
    while (client.available()) {
      line = client.readStringUntil('\n');
      if (line == "\r") {
        jsonStart = true;  // Now we start reading the JSON data
      } else if (jsonStart) {
        response += line;
      }
    }


    // Close the connection
    client.stop();


    char json[response.length() + 1];
    response.toCharArray(json, sizeof(json));
    JsonDocument doc;  // Adjust the size based on the expected size of the JSON response
    deserializeJson(doc, json);

    String sessionId = doc["sessionId"];
    String Element = doc["Element"];
    String teacherName = doc["teacherName"];
    String salle = doc["salle"];
    String filiere = doc["filiere"];

    if (sessionId == "null"){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Server Error!");
      lcd.setCursor(0, 1);
      lcd.print("Retry...");
      delay(1000);
      return;
    }

    Serial.print("-----");
    Serial.print("response: " + response);
    Serial.print("-----");
    // Extract and display the "prenom" (first name)
    Serial.println();
    Serial.print("nom: " + teacherName);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Salle:" + salle);
    lcd.setCursor(11, 0);
    timeClient.update();
    int hour = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    long seconds = timeClient.getEpochTime();
    long deadline = seconds + 40;
    String hourString = hour < 10 ? "0" + String(hour) : String(hour);
    String minuteString = minutes < 10 ? "0" + String(minutes) : String(minutes);
    String time = hourString + ":" + minuteString;


    Serial.print(time);

    lcd.setCursor(0, 1);
    lcd.print(Element);
    lcd.setCursor(6, 1);
    lcd.print(" w/Pr " + teacherName);
    // lcd.setCursor(0, 1);
    // String classLine = Element + " with Pr " + teacherName;
    // scrollText(classLine, salle, 150);
    delay(1000);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    getStudentDoc(filiere);


    String countString = studentsDoc["count"];
    const int studentCount = countString.toInt();
    
    Serial.println(studentCount);
    lcd.clear();
    while (markStudents(filiere, Element, deadline) == 0) {
      markStudents(filiere, Element, deadline);
    }

    if (internalCount >= studentCount){
      lcd.setCursor(0,1);
      lcd.print("Good Class!!!");
    } else if (internalCount > 0) {
      bool status = sendList(sessionId);
      if (status == 0){
        Serial.println("list not sent!");
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        return;
      } else {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Absence Marked");
        lcd.setCursor(0,1);
        lcd.print("Good Bye");
      }
    } else {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Absence Canceled");
      lcd.setCursor(0,1);
      lcd.print("No Presence!");
    }


  } else {
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      return;
  }
  internalCount = 0;
  delay(3000);  // Wait for 3 seconds before the next read
  lcd.clear();
  lcd.noBacklight();
  ESP.deepSleep(0);
}

void getStudentDoc(String studentFiliere) {
  String jsonPayload = "{\"filiere\":\"" + studentFiliere + "\"}";
  // Print the JSON payload to Serial Monitor
  Serial.println("JSON Payload: " + jsonPayload);
  // Make a POST request to the API using WiFiClient
  if (client.connect(server, port)) {
    client.println("POST " + String(path1) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(jsonPayload.length());
    client.println();
    client.println(jsonPayload);

    // Wait for the server to respond
    while (!client.available()) {
      delay(10);
    }
    String line;
    String response;
    bool jsonStart = false;
    // Read and print the response from the server
    while (client.available()) {
      line = client.readStringUntil('\n');
      if (line == "\r") {
        jsonStart = true;  // Now we start reading the JSON data
      } else if (jsonStart) {
        response += line;
      }
    }

    Serial.println("response doc: "+response);

    // Close the connection
    client.stop();

    char jsonStudents[response.length() + 1];
    response.toCharArray(jsonStudents, sizeof(jsonStudents));  // Adjust the size based on the expected size of the JSON response
    
    deserializeJson(studentsDoc, jsonStudents);
  }
  return ;
}

bool markStudents(String filiere, String Element, long deadline) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  byte block;
  byte len;
  MFRC522::StatusCode status;
  filiere = filiere + "-";
  timeClient.update();
  long seconds = timeClient.getEpochTime();
  String countString = studentsDoc["count"];
  const int studentCount = countString.toInt();
  if (internalCount >= studentCount){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("All is here");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return true;
  }

  if (seconds > deadline) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time is UP!!!");
    delay(800);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return true;
  }

  while (seconds <= deadline) {
    if (!mfrc522.PICC_IsNewCardPresent()) {
      timeClient.update();
      seconds = timeClient.getEpochTime();
      String minutesToDeadline = ((deadline - seconds) / 60) < 10 ? "0" + String((deadline - seconds) / 60) : String((deadline - seconds) / 60);
      String secondstoDeadline = ((deadline - seconds) % 60) < 10 ? "0" + String((deadline - seconds) % 60) : String((deadline - seconds) % 60);
      String deadlineString = minutesToDeadline + ":" + secondstoDeadline;
      lcd.setCursor(0, 0);
      lcd.print(filiere);
      lcd.setCursor(4, 0);
      lcd.print(Element);
      lcd.setCursor(11, 0);
      lcd.print(deadlineString);
      lcd.setCursor(3, 1);
      lcd.print("waiting...");
      return false;
    }

    if (!mfrc522.PICC_ReadCardSerial()) {
      return false;
    }

    Serial.println(F("Student Card Detected:"));

    mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

    byte buffer1[18];
    block = 4;  // Adjust if needed
    len = 18;

    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Authentication failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
      ;
    }

    status = mfrc522.MIFARE_Read(block, buffer1, &len);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Reading failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }

    byte buffer2[18];
    block = 1;  // Adjust if needed
    len = 18;

    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Authentication failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
      ;
    }

    status = mfrc522.MIFARE_Read(block, buffer2, &len);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Reading failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
      ;
    }

    String studentName = "";
    String tagId = "";

    for (uint8_t i = 1; i < 16; i++) {
      studentName += (char)buffer1[i];
    }
    String space;
    for (uint8_t i = 0; i < 5; i++) {
      space = (char)buffer2[i];
      if (space!=" " && space!="\n") tagId += (char)buffer2[i];
    }

    if (tagId.startsWith("0")) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Prof " + studentName);
      lcd.setCursor(0, 1);
      lcd.print("Not a Student!");
      delay(2000);
      lcd.clear();
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      return false;
    }
    Serial.println();
    Serial.print("tag content" + tagId);
    String existance = studentsDoc[tagId];
    Serial.println("existance"+ existance + existance.length());

    if (existance != "null" && existance != 0 && existance != "1" ){
      studentsDoc[tagId] = "1";
      internalCount++;
      //small beep for successs
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Hello ");
      lcd.setCursor(7, 0);
      lcd.print(studentName);
      lcd.setCursor(0, 1);
      lcd.print("You are Checked!");
      if (internalCount>=studentCount) {
        delay(2000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("All is here");
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        return true;
      }
      delay(1000);
      lcd.clear();
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    } else if (existance == "1"){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Already Marked");
      delay(1000);
      lcd.clear();
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      //long beep for error
      return false;
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Hello ");
      lcd.setCursor(7, 0);
      lcd.print(studentName);
      lcd.setCursor(0, 1);
      lcd.print("Not your class!");
      delay(1000);
      lcd.clear();
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      studentsDoc.remove(tagId);
      //long beep for error
      return false;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time is UP");
  delay(800);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return true;
  
}

bool sendList (String sessionId) {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Marking");
  lcd.setCursor(2, 1);
  lcd.print("Absense...");
  String payload;
  studentsDoc["sessionId"] = sessionId;
  serializeJson(studentsDoc, payload);

  Serial.println("JSON Payload: " + payload);

  if (client.connect(server, port)) {
    client.println("POST " + String(path2) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(payload.length());
    client.println();
    client.println(payload);

    // Wait for the server to respond
    while (!client.available()) {
      delay(10);
    }
    String line;
    String response;
    bool jsonStart = false;
    // Read and print the response from the server
    while (client.available()) {
      line = client.readStringUntil('\n');
      if (line == "\r") {
        jsonStart = true;  // Now we start reading the JSON data
      } else if (jsonStart) {
        response += line;
      }
    }


    // Close the connection
    client.stop();


    char jsonStudents[response.length() + 1];
    response.toCharArray(jsonStudents, sizeof(jsonStudents));
    JsonDocument docStudents;  // Adjust the size based on the expected size of the JSON response
    deserializeJson(docStudents, jsonStudents);

    if(docStudents["status"]="1"){
      return true;
    } else if (docStudents["status"]="2") {
      lcd.clear();
      lcd.setCursor(5, 0);
      lcd.print("Empty");
      lcd.setCursor(5, 1);
      lcd.print("List!");
      lcd.clear();
      return true;
    } else {
      lcd.setCursor(0,1);
      lcd.print("Server Error!");
      return false;
    }
  }
  else{
    Serial.println("Unable to connect to the server");
    return false;
  }
}
