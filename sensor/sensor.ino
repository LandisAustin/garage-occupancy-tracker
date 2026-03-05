/*********
  Firebase Occupancy Increment
  + HC-SR04 Entrance Sensor
  Author: Austin Landis
*********/

# include "credentials_example.h"

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

// --------------------
// Button Setup
// --------------------
#define BUTTON_PIN 5
bool lastButtonState = HIGH;

// --------------------
// Ultrasonic Setup
// --------------------
#define TRIG_PIN 32
#define ECHO_PIN 33

#define DISTANCE_THRESHOLD 10  // cm (adjust as needed)

bool objectDetected = false;

// --------------------
// Firebase Setup
// --------------------
void processData(AsyncResult &aResult);

UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// --------------------
// Function to increment occupancy
// --------------------
void incrementOccupancy()
{
  float delta = 1;

  object_t incr_json, sv_json;
  JsonWriter writer;

  writer.create(incr_json, "increment", delta);
  writer.create(sv_json, ".sv", incr_json);

  bool status = Database.set<object_t>(
    aClient,
    "/occupancy/current",
    sv_json
  );

  if (status)
    Serial.println("Increment success!");
  else
    Firebase.printf("Error: %s\n", aClient.lastError().message().c_str());
}

// --------------------
// Read Ultrasonic Distance
// --------------------
long readDistanceCM()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms

  long distance = duration / 58; // convert to cm

  return distance;
}

void setup() {
  Serial.begin(115200);

  // Button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Ultrasonic setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected!");

  // SSL config
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  // Firebase init
  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

void loop()
{
  app.loop();

  if (app.ready())
  {
    // --------------------
    // BUTTON INCREMENT
    // --------------------
    bool currentButtonState = digitalRead(BUTTON_PIN);

    if (lastButtonState == HIGH && currentButtonState == LOW)
    {
      Serial.println("Button pressed!");
      incrementOccupancy();
      delay(200); // debounce
    }

    lastButtonState = currentButtonState;

    // --------------------
    // ULTRASONIC DETECTION
    // --------------------
    long distance = readDistanceCM();

    if (distance > 0)  // valid reading
    {
      Serial.print("Distance: ");
      Serial.println(distance);

      // Object enters detection zone
      if (distance < DISTANCE_THRESHOLD && !objectDetected)
      {
        Serial.println("Car detected...");
        objectDetected = true;
      }

      // Object leaves detection zone (full pass complete)
      if (distance >= DISTANCE_THRESHOLD && objectDetected)
      {
        Serial.println("Car passed! Incrementing...");
        incrementOccupancy();
        objectDetected = false;
      }
    }

    delay(100); // small loop delay
  }
}

// --------------------
// Firebase Callback
// --------------------
void processData(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  if (aResult.isEvent())
    Firebase.printf("Event: %s\n", aResult.eventLog().message().c_str());

  if (aResult.isDebug())
    Firebase.printf("Debug: %s\n", aResult.debug().c_str());

  if (aResult.isError())
    Firebase.printf("Error: %s\n", aResult.error().message().c_str());

  if (aResult.available())
    Firebase.printf("Payload: %s\n", aResult.c_str());
}