// #include <Arduino.h>
// #include <Preferences.h>
// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <esp_now.h>
// #include <WebServer.h>
// #include <terseCRSF.h>
// #include <esp_wifi.h>

// #include <cstring>      // For memset
// #include <sys/socket.h> // For socket functions
// #include <netinet/in.h> // For sockaddr_in
// #include <unistd.h>     // For close()

// #include <common/mavlink.h>

// extern void sendMavlinkBundle();

// // Global MAC address variables
// uint8_t UID[6] = {0, 0, 0, 0, 0, 0};          // This will be the MAC address used for STA (and SoftAP if derived)
// char configuredUidString[30] = "0,0,0,0,0,0"; // Stores the user's input string for display

// const char *ssid = "MAVLink Serial Forwarder";
// const char *password = "mavlink123";

// const char *targetBackPackWiFiSSID = "ExpressLRS TX Backpack B29E29";
// const char *targetBackPackWiFiPassword = "expresslrs";

// // Variables to store the MAC of the first ESP-NOW sender
// uint8_t senderMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Store the MAC of the first sender
// bool senderMacKnown = false;                                 // Flag to indicate if a sender's MAC has been learned

// WiFiUDP Udp;
// Preferences preferences;
// WebServer server(80);

// // Extern declarations for variables in other files
// extern CRSF crsf;
// extern int16_t espnow_len;
// extern int16_t crsf_len;
// extern bool espnow_received;

// // HardwareSerial &mavSerial = Serial;

// // Function to save UID (binary MAC) to NVS
// void saveUID()
// {
//     preferences.begin("uid", false);
//     preferences.putBytes("mac", UID, 6);
//     preferences.end();
//     Serial.print("UID (binary MAC) saved: ");
//     for (int i = 0; i < 6; i++)
//     {
//         Serial.printf("%02X%s", UID[i], (i < 5 ? ":" : ""));
//     }
//     Serial.println();
// }

// // Function to load UID (binary MAC) from NVS
// void loadUID()
// {
//     preferences.begin("uid", true);
//     if (preferences.isKey("mac"))
//     {
//         preferences.getBytes("mac", UID, 6);
//         Serial.print("UID (binary MAC) loaded from NVS: ");
//         for (int i = 0; i < 6; i++)
//         {
//             Serial.printf("%02X%s", UID[i], (i < 5 ? ":" : ""));
//         }
//         Serial.println();
//     }
//     else
//     {
//         Serial.println("No UID (binary MAC) found in NVS, using default (all zeros).");
//     }
//     preferences.end();
// }

// // Function to save the user's input string to NVS
// void saveConfiguredUidString()
// {
//     preferences.begin("uid_str", false);
//     preferences.putString("input_mac_str", configuredUidString);
//     preferences.end();
//     Serial.printf("Configured UID String saved: %s\n", configuredUidString);
// }

// // Function to load the user's input string from NVS
// void loadConfiguredUidString()
// {
//     preferences.begin("uid_str", true);
//     String tempStr = preferences.getString("input_mac_str", "0,0,0,0,0,0");
//     tempStr.toCharArray(configuredUidString, sizeof(configuredUidString));
//     preferences.end();
//     Serial.printf("Configured UID String loaded from NVS: %s\n", configuredUidString);
// }

// // Helper to format a MAC address as a string (always two hex digits)
// String macBytesToString(const uint8_t *macArray)
// {
//     String macStr = "";
//     for (int i = 0; i < 6; i++)
//     {
//         char hexBuf[3];
//         sprintf(hexBuf, "%02X", macArray[i]);
//         macStr += hexBuf;
//         if (i < 5)
//             macStr += ":";
//     }
//     return macStr;
// }

// // Web server root handler
// void handleRoot()
// {
//     uint8_t actualStaMac[6];
//     esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
//     if (netif_sta)
//     {
//         esp_netif_get_mac(netif_sta, actualStaMac);
//     }
//     else
//     {
//         memset(actualStaMac, 0, 6);
//         memcpy(actualStaMac, UID, 6);
//     }

//     String currentUIDStr = macBytesToString(actualStaMac);

//     String html = "<!DOCTYPE html><html><body>"
//                   "<h2>MAVLink Serial Forwarder</h2>"
//                   "<p>Configured UID:<br> <b>" +
//                   String(configuredUidString) + "</b></p>"
//                                                 "<p><b>Actual MAC:<br> " +
//                   currentUIDStr + "</b></p>"
//                                   "<form action='/setuid' method='POST'>"
//                                   "Set new UID:<br> <input name='uid' value='' placeholder='e.g. 11,22,33,44,55,66'> "
//                                   "<br><br>"
//                                   "<input type='submit' value='Save & Reboot'>"
//                                   "</form><br>"
//                                   "</body></html>";

//     server.send(200, "text/html", html);
// }

// // Web server handler to set new UID
// void handleSetUID()
// {
//     if (!server.hasArg("uid"))
//     {
//         server.send(400, "text/plain", "Missing uid argument");
//         return;
//     }
//     String uidInput = server.arg("uid");
//     uidInput.trim();

//     uidInput.toCharArray(configuredUidString, sizeof(configuredUidString));

//     uint8_t newUID_temp[6];
//     int byteIndex = 0;
//     int start = 0;
//     for (byteIndex = 0; byteIndex < 6; byteIndex++)
//     {
//         int commaPos = uidInput.indexOf(',', start);
//         String byteStr;
//         if (commaPos == -1)
//         {
//             if (byteIndex < 5)
//             {
//                 server.send(400, "text/plain", "Invalid UID format: Too few bytes or missing commas.");
//                 return;
//             }
//             byteStr = uidInput.substring(start);
//         }
//         else
//         {
//             byteStr = uidInput.substring(start, commaPos);
//         }
//         byteStr.trim();
//         if (byteStr.length() == 0 || byteStr.length() > 3)
//         {
//             server.send(400, "text/plain", "Invalid UID format: Byte string length invalid for '" + byteStr + "'. Please enter DECIMAL values (0-255).");
//             return;
//         }
//         char *endptr;
//         long val = strtol(byteStr.c_str(), &endptr, 10);
//         if (*endptr != 0 || val < 0 || val > 255)
//         {
//             server.send(400, "text/plain", "Invalid byte value or format: '" + byteStr + "'. Please enter valid DECIMAL values (0-255).");
//             return;
//         }
//         newUID_temp[byteIndex] = (uint8_t)val;
//         start = commaPos + 1;
//     }
//     if (byteIndex != 6)
//     {
//         server.send(400, "text/plain", "UID must have exactly 6 bytes after parsing.");
//         return;
//     }

//     memcpy(UID, newUID_temp, 6);
//     saveUID();
//     saveConfiguredUidString();

//     Serial.println("Applying new MAC and restarting WiFi/ESP-NOW...");

//     String html = "<!DOCTYPE html><html><body>"
//                   "<h2>MAC Address Saved!</h2>"
//                   "<p>The new MAC address has been saved. The device is now rebooting to apply the changes.</p>"
//                   "<p>Please wait a moment and then reconnect to the WiFi network.</p>"
//                   "<p>You may need to refresh this page (or navigate to 10.0.0.1) after the reboot.</p>"
//                   "</body></html>";
//     server.send(200, "text/html", html);
//     delay(2000);
//     ESP.restart();
// }

// // Handler for resetting MAC to default/random
// void handleResetMAC()
// {
//     memset(UID, 0, 6);
//     strcpy(configuredUidString, "0,0,0,0,0,0");
//     saveUID();
//     saveConfiguredUidString();

//     Serial.println("Resetting MAC to default/random and restarting...");

//     String html = "<!DOCTYPE html><html><body>"
//                   "<h2>MAC Address Reset!</h2>"
//                   "<p>The MAC address has been reset to default/random. The device is now rebooting.</p>"
//                   "<p>Please wait a moment and then reconnect to the WiFi network.</p>"
//                   "<p>You may need to refresh this page (or navigate to 10.0.0.1) after the reboot.</p>"
//                   "</body></html>";
//     server.send(200, "text/html", html);
//     delay(2000);
//     ESP.restart();
// }

// void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
// {
//     // if (!senderMacKnown) {
//     //     memcpy(senderMac, mac, 6);
//     //     senderMacKnown = true;
//     //     Serial.print("Learned sender MAC: ");
//     //     for (int i = 0; i < 6; i++) {
//     //         Serial.printf("%02X%s", senderMac[i], (i < 5 ? ":" : ""));
//     //     }
//     //     Serial.println();
//     // }
//     // espnow_len = len;
//     // crsf_len = len - 8;
//     // espnow_received = true;
//     // memcpy(&crsf.crsf_buf, incomingData + 8, sizeof(crsf.crsf_buf));
//     Serial.print("Data received. Passing.");
//     Serial.write(incomingData + 8, len - 8);
// }

// #define LED 2
// WiFiUDP udp;
// WiFiUDP sendingUDP;
// const unsigned int localUdpPort = 14550;
// const unsigned int sendingUdpPort = 14555;
// char incomingPacket[2048]; // Buffer for incoming data

// void setup()
// {
//     Serial.begin(57600);
//     Serial.println("\nStarting Backpack Wifi MAVLink Bridge...");

//     loadUID();
//     loadConfiguredUidString();

//     UID[0] = UID[0] & ~0x01;

//     WiFi.mode(WIFI_AP_STA);
//     WiFi.disconnect();

//     esp_wifi_set_mac(WIFI_IF_STA, UID);

//     WiFi.softAPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
//     WiFi.softAP(ssid, password);

//     // if (esp_now_init() != ESP_OK) {
//     //     Serial.println("Error initializing ESP-NOW");
//     //     return;
//     // }

//     // esp_now_register_recv_cb(OnDataRecv);

//     server.on("/", HTTP_GET, handleRoot);
//     server.on("/setuid", HTTP_POST, handleSetUID);
//     server.on("/resetmac", HTTP_POST, handleResetMAC);
//     server.begin();
//     Serial.println("Web server started.");

//     WiFi.softAP(ssid, password);

//     WiFi.begin(targetBackPackWiFiSSID, targetBackPackWiFiPassword);
//     Serial.print("Connecting to WiFi ..");
//     while (WiFi.status() != WL_CONNECTED)
//     {
//         Serial.print('.');
//         delay(1000); // Wait for 1 second before checking again
//     }

//     Serial.println("");
//     Serial.println("WiFi connected.");
//     Serial.print("IP address: ");
//     Serial.println(WiFi.localIP());

//     udp.begin(localUdpPort);
//     // Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
//     pinMode(LED, OUTPUT);
// }

// unsigned long lastMavSend = 0;
// unsigned long lastLedChanged = 0;
// static std::vector<std::vector<uint8_t>> toGcsPackets;
// bool ledOn = false;
// uint8_t serialBuffer[255];
// uint8_t udpBuffer[255];

// void loop()
// {
//     if (millis() - lastLedChanged >= 100)
//     {
//         lastLedChanged = millis();
//         ledOn = !ledOn;
//         digitalWrite(LED, ledOn ? HIGH : LOW);
//     }

//     server.handleClient();

//     int packetSize = udp.parsePacket();
//     if (packetSize)
//     {
//         Serial.write(udp.read());
//         // int len = udp.read(incomingPacket, 2048);
//         // if (len > 0)
//         // {
//         //     toGcsPackets.push_back(std::vector<uint8_t>((uint8_t *)incomingPacket, (uint8_t *)incomingPacket + len));
//         // }       
//     }

//     if (Serial.available()) {
//         sendingUDP.beginPacket("10.0.0.1", sendingUdpPort);
//         sendingUDP.write(Serial.read());
//         int result = sendingUDP.endPacket();
//         Serial.println("Package sent result:" + String(result));
//     }

//     // if (millis() - lastMavSend >= 100)
//     // {
//     //     lastMavSend = millis();
       
//     //     for (const auto &pkt : toGcsPackets)
//     //     {
//     //         Serial.write(pkt.data(), pkt.size());
//     //     }
//     //     toGcsPackets.clear();
//     // }
// }
