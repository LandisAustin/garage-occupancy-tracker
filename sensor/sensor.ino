/*********
  Firebase Occupancy Tracker (Robust Demo Version)
  + HC-SR04 Sensors
  + Debug LEDs
  + Offline Support
  + WiFi Reconnect Button
  Author: Austin Landis
*********/

#include "credentials.h"

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

// --------------------
// LED Setup
// --------------------
#define WIFI_LED 2
#define ENTRY_LED 15
#define EXIT_LED 16

// --------------------
// WiFi Reconnect Button
// --------------------
#define WIFI_BUTTON 5
bool lastButtonState = HIGH;

// --------------------
// Ultrasonic Setup
// --------------------
#define ENTRY_TRIG_PIN 4
#define ENTRY_ECHO_PIN 32

#define EXIT_TRIG_PIN 18
#define EXIT_ECHO_PIN 34

#define DISTANCE_THRESHOLD 15  // cm

bool entryObjectDetected = false;
bool exitObjectDetected = false;

// --------------------
// Local Occupancy (OFFLINE SUPPORT)
// --------------------
int localOccupancy = 0;

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
// WiFi Reconnect Function
// --------------------
void reconnectWiFi()
{
  Serial.println("Attempting WiFi reconnect...");

  WiFi.disconnect();
  delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 8000;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < wifiTimeout)
  {
    digitalWrite(WIFI_LED, HIGH);
    delay(200);
    digitalWrite(WIFI_LED, LOW);
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(WIFI_LED, HIGH);
    Serial.println("\nWiFi reconnected!");

    // Reinitialize Firebase
    ssl_client.setInsecure();
    ssl_client.setConnectionTimeout(1000);
    ssl_client.setHandshakeTimeout(5);

    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
  }
  else
  {
    digitalWrite(WIFI_LED, LOW);
    Serial.println("\nReconnect failed");
  }
}

// --------------------
// Increment Occupancy
// --------------------
void incrementOccupancy()
{
  localOccupancy++;

  Serial.print("Local Occupancy: ");
  Serial.println(localOccupancy);

  if (WiFi.status() == WL_CONNECTED && app.ready())
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
      Serial.println("Firebase Increment success!");
    else
      Firebase.printf("Error: %s\n", aClient.lastError().message().c_str());
  }
  else
  {
    Serial.println("Firebase unavailable (offline mode)");
  }
}

// --------------------
// Decrement Occupancy
// --------------------
void decrementOccupancy()
{
  localOccupancy--;

  if (localOccupancy < 0)
    localOccupancy = 0;

  Serial.print("Local Occupancy: ");
  Serial.println(localOccupancy);

  if (WiFi.status() == WL_CONNECTED && app.ready())
  {
    float delta = -1;

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
      Serial.println("Firebase Decrement success!");
    else
      Firebase.printf("Error: %s\n", aClient.lastError().message().c_str());
  }
  else
  {
    Serial.println("Firebase unavailable (offline mode)");
  }
}

// --------------------
// Read Ultrasonic Distance
// --------------------
long readDistanceCM(int trigPin, int echoPin)
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  long distance = duration / 58;

  return distance;
}

// --------------------
// Setup
// --------------------
void setup()
{
  Serial.begin(115200);
  Serial.println("System Start");

  // LED setup
  pinMode(WIFI_LED, OUTPUT);
  pinMode(ENTRY_LED, OUTPUT);
  pinMode(EXIT_LED, OUTPUT);

  // Button setup
  pinMode(WIFI_BUTTON, INPUT_PULLUP);

  // Ultrasonic setup
  pinMode(ENTRY_TRIG_PIN, OUTPUT);
  pinMode(ENTRY_ECHO_PIN, INPUT);

  pinMode(EXIT_TRIG_PIN, OUTPUT);
  pinMode(EXIT_ECHO_PIN, INPUT);

  // --------------------
  // WiFi Connection (with timeout)
  // --------------------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 8000;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < wifiTimeout)
  {
    digitalWrite(WIFI_LED, HIGH);
    delay(200);
    digitalWrite(WIFI_LED, LOW);
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(WIFI_LED, HIGH);
    Serial.println("\nWiFi connected!");

    // --------------------
    // SSL Config
    // --------------------
    ssl_client.setInsecure();
    ssl_client.setConnectionTimeout(1000);
    ssl_client.setHandshakeTimeout(5);

    // --------------------
    // Firebase Init
    // --------------------
    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
  }
  else
  {
    digitalWrite(WIFI_LED, LOW);
    Serial.println("\nStartup WiFi failed - entering offline mode");
  }
}

// --------------------
// Main Loop
// --------------------
void loop()
{
  app.loop();

  // --------------------
  // Reconnect Button
  // --------------------
  bool currentButtonState = digitalRead(WIFI_BUTTON);

  if (lastButtonState == HIGH && currentButtonState == LOW)
  {
    reconnectWiFi();
  }

  lastButtonState = currentButtonState;

  // --------------------
  // Read Sensors
  // --------------------
  long entryDistance = readDistanceCM(ENTRY_TRIG_PIN, ENTRY_ECHO_PIN);
  delay(60);
  long exitDistance = readDistanceCM(EXIT_TRIG_PIN, EXIT_ECHO_PIN);

  // Debug if sensors fail
  if (entryDistance <= 0)
    Serial.println("Entry sensor not detecting");

  if (exitDistance <= 0)
    Serial.println("Exit sensor not detecting");

  // --------------------
  // ENTRY SENSOR
  // --------------------
  if (entryDistance > 0)
  {
    if (entryDistance < DISTANCE_THRESHOLD && !entryObjectDetected)
    {
      Serial.println("Car entering...");
      entryObjectDetected = true;
      digitalWrite(ENTRY_LED, HIGH);
    }

    if (entryDistance >= DISTANCE_THRESHOLD && entryObjectDetected)
    {
      Serial.println("Car entered!");
      incrementOccupancy();
      entryObjectDetected = false;
      digitalWrite(ENTRY_LED, LOW);
    }
  }

  // --------------------
  // EXIT SENSOR
  // --------------------
  if (exitDistance > 0)
  {
    if (exitDistance < DISTANCE_THRESHOLD && !exitObjectDetected)
    {
      Serial.println("Car exiting...");
      exitObjectDetected = true;
      digitalWrite(EXIT_LED, HIGH);
    }

    if (exitDistance >= DISTANCE_THRESHOLD && exitObjectDetected)
    {
      Serial.println("Car exited!");
      decrementOccupancy();
      exitObjectDetected = false;
      digitalWrite(EXIT_LED, LOW);
    }
  }

  delay(100);
}

// --------------------
// Firebase Callback
// --------------------
void processData(AsyncResult &aResult)
{
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
