#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

const char* ssid = "UW MPSK";
const char* password = "!P\\E475dfc"; // Replace with your network password

#define DATABASE_URL "https://esp32-firebase-demo-wanling-default-rtdb.firebaseio.com/" // Replace with your database URL
#define API_KEY "AIzaSyCic3XUnSDaRle8D_2DTdoiw-caJrkJGJg" // Replace with your API key

#define STAGE_INTERVAL 8000 // 8 seconds for the initial measurement phase
#define MAX_WIFI_RETRIES 5 // Maximum number of WiFi connection retries

int uploadInterval = 2000; // Adjusted for 0.5 Hz measurement frequency

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

// HC-SR04 Pins
const int trigPin = D2;
const int echoPin = D3;

// Define sound speed in cm/usec
const float soundSpeed = 0.034;

// Function prototypes
float measureDistance();
void connectToWiFi();
void initFirebase();
void sendDataToFirebase(float distance);

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Run ultrasonic sensor only for 8 seconds
  Serial.println("Measuring distance for 8 seconds...");
  unsigned long startTime = millis();
  int invalidDistanceCnt = 0;

  while (millis() - startTime < STAGE_INTERVAL) // 8 seconds
  {
    float currentDistance = measureDistance();

    if (currentDistance > 50)
    {
      invalidDistanceCnt++;
    }

    delay(2000); // Adjusted delay to achieve 0.5 Hz measurement frequency
  }
  
  // Most measurements > 50, go to deep sleep for 50 seconds
  if (invalidDistanceCnt >= 4) // Adjusted based on the new measurement interval
  {
    Serial.println("All measured distance > 50, going to deep sleep for 50 seconds...");
    esp_sleep_enable_timer_wakeup(50000 * 1000); // 50 seconds in microseconds
    esp_deep_sleep_start();
  }
  else 
  {
    // Turn on WiFi and turn on Firebase and send data with distance measurements
    Serial.println("Turning on WiFi and measuring...");
    connectToWiFi();
    initFirebase();
    startTime = millis();
    while (millis() - startTime < 5000)    {
      float currentDistance = measureDistance();
      sendDataToFirebase(currentDistance);
      delay(2000); // Adjusted delay to match the 0.5 Hz frequency
    }

    // Go to deep sleep for 50 seconds
    Serial.println("Going to deep sleep for 50 seconds...");
    WiFi.disconnect();
    esp_sleep_enable_timer_wakeup(50000 * 1000); // 50 seconds in microseconds
    esp_deep_sleep_start();
  }
}

void loop(){
  // This is not used
}

float measureDistance()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * soundSpeed / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void connectToWiFi()
{
  Serial.println(WiFi.macAddress());
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  int wifiCnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wifiCnt++;
    if (wifiCnt > MAX_WIFI_RETRIES){
      Serial.println("WiFi connection failed");
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void sendDataToFirebase(float distance){
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > uploadInterval || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    if (Firebase.RTDB.pushFloat(&fbdo, "test/distance", distance)){
      Serial.println("PASSED");
      Serial.print("PATH: ");
      Serial.println(fbdo.dataPath());
      Serial.print("TYPE: ");
      Serial.println(fbdo.dataType());
    } else {
      Serial.println("FAILED");
      Serial.print("REASON: ");
      Serial.println(fbdo.errorReason());
    }
    count++;
  }
}