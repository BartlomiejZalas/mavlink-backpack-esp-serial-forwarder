#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>
#include <WebServer.h>

const int SERIAL_BAUD = 57600;
const int MAVLINK_PORT = 14550;
const int LED_PIN = 2;

// Wifi hotspot credentials
const String ownSsid = "MAVLink Serial Forwarder";
const String ownPassword = "mavlink123";

// Target Backpack WiFi credentials
String targetBackPackWiFiSSID = "";
String targetBackPackWiFiPassword = "";

WiFiUDP udp;
Preferences preferences;
WebServer server(80);

void saveBackpackWifi()
{
  preferences.begin("backpack_wifi", false);
  preferences.putString("ssid", targetBackPackWiFiSSID);
  preferences.putString("password", targetBackPackWiFiPassword);
  preferences.end();
  Serial.print("Backpack WiFi credentials saved: ");
}

boolean loadBackpackWifi()
{
  preferences.begin("backpack_wifi", true);
  if (preferences.isKey("ssid") && preferences.isKey("password"))
  {
    String ssid = preferences.getString("ssid", "default_ssid");
    String password = preferences.getString("password", "default_password");
    targetBackPackWiFiSSID = ssid;
    targetBackPackWiFiPassword = password;
    Serial.println("Backpack WiFi credentials loaded.");
    preferences.end();
    return true;
  }

  Serial.println("No Backpack WiFi credentials saved.");
  preferences.end();
  return false;
}

void handleRoot()
{

  String html = "<!DOCTYPE html><html><body>"
                "<h2>MAVLink Serial Forwarder</h2>"
                "<form action='/setWifi' method='POST'>"
                "SSID:<br> <input name='ssid' value='" +
                targetBackPackWiFiSSID + "'><br/>"
                                         "Password:<br> <input type='password' name='password' value='" +
                targetBackPackWiFiPassword + "' > "
                                             "<br><br>"
                                             "<input type='submit' value='Save & Reboot'>"
                                             "</form>"
                                             "</body></html>";

  server.send(200, "text/html", html);
}

void handleSaveWiFiCredentials()
{
  String ssidInput = server.arg("ssid");
  String passwordInput = server.arg("password");
  ssidInput.trim();
  passwordInput.trim();
  if (ssidInput.length() == 0)
  {
    server.send(400, "text/plain", "SSID cannot be empty");
    return;
  }
  if (passwordInput.length() == 0)
  {
    server.send(400, "text/plain", "Password must be at least 8 characters");
    return;
  }

  targetBackPackWiFiSSID = ssidInput;
  targetBackPackWiFiPassword = passwordInput;
  saveBackpackWifi();

  Serial.println("Applying new WiFi credentials and restarting...");

  String html = "<!DOCTYPE html><html><body>"
                "<h2>WiFi Credentials Saved!</h2>"
                "<p>The new WiFi credentials have been saved. The device is now rebooting to apply the changes.</p>"
                "<p>Please wait a moment and then reconnect to the WiFi network.</p>"
                "<p>You may need to refresh this page (or navigate to 10.0.0.1) after the reboot.</p>"
                "</body></html>";
  server.send(200, "text/html", html);
  delay(2000);
  ESP.restart();
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("Starting...");

  bool configured = loadBackpackWifi();

  // Creating WiFi AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.softAPConfig(IPAddress(10, 10, 0, 1), IPAddress(10, 10, 0, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ownSsid, ownPassword);
  Serial.println("WiFi AP Started.");

  // Creating web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setWifi", HTTP_POST, handleSaveWiFiCredentials);
  server.begin();
  Serial.println("Web server started.");

  // Connect to WiFi
  WiFi.begin(targetBackPackWiFiSSID, targetBackPackWiFiPassword);
  Serial.print("Connecting to WiFi");

  if (!configured)
  {
    Serial.print("... not configured. Skipping. Configure Backpack WiFi on 10.0.0.1.");
    return;
  }

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 10)
  {
    delay(1000);
    Serial.print(".");
    ++retry;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Start UDP
    udp.begin(MAVLINK_PORT);
    Serial.printf("Listening on UDP port %d\n", MAVLINK_PORT);
  } else {
    Serial.println("Failed to connect to WiFi. Configure Backpack WiFi on 10.0.0.1.");
  }

  // Setup LED pin

  pinMode(LED_PIN, OUTPUT);

  Serial.println("Entering main loop...");
}

unsigned long lastLedChanged = 0;
bool ledOn = false;

void loop()
{
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(1);
    return;
  }

  if (millis() - lastLedChanged >= 100)
  {
    lastLedChanged = millis();
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  }

  // Check for incoming UDP packets (from GCS)
  int packetSize = udp.parsePacket();
  if (packetSize)
  {
    char packetBuffer[512];
    int len = udp.read(packetBuffer, sizeof(packetBuffer));
    if (len > 0)
    {
      packetBuffer[len] = 0;
      Serial.write((uint8_t *)packetBuffer, len); // Send to serial
    }
  }

  // Check for incoming serial data (from telemetry or flight controller)
  char serialBuffer[512];
  int serialIndex = 0;

  while (Serial.available())
  {
    char c = Serial.read();
    serialBuffer[serialIndex++] = c;

    // Optional: flush buffer if too large
    if (serialIndex >= sizeof(serialBuffer))
    {
      break;
    }

    // Optional: Add a timeout or termination character logic here
    delay(1);
  }
  
  // If buffer has data
  if (serialIndex > 0)
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write((uint8_t *)serialBuffer, serialIndex);
    udp.endPacket();
    serialIndex = 0;
  }

  delay(1); // avoid busy looping
}