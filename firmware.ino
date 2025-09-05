#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>  // for HTTPS

// ====== USER SETTINGS ======
#define FW_VERSION "1.0.0"  // bump this when you publish a new release
const char* SSID = "Arpinet-79732";
const char* PASS = "CIPR$2025";

// RAW link to latest.json in your repo:
const char* MANIFEST_URL = "https://raw.githubusercontent.com/annaalmasyan2-glitch/esp32-ota-demo/main/latest.json";

// (Optional) Your LEDs for visible behavior
const int LED1 = 5;
const int LED2 = 18;

// ====== SIMPLE SEMVER COMPARISON (major.minor.patch) ======
bool isNewerVersion(const String& currentV, const String& remoteV) {
  int c1, c2, c3, r1, r2, r3;
  if (sscanf(currentV.c_str(), "%d.%d.%d", &c1, &c2, &c3) != 3) return true;   // if bad format, assume update
  if (sscanf(remoteV.c_str(), "%d.%d.%d", &r1, &r2, &r3) != 3) return false;  // remote bad -> skip
  if (r1 != c1) return r1 > c1;
  if (r2 != c2) return r2 > c2;
  return r3 > c3;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.print("Wi-Fi: ");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnected. IP: %s\n", WiFi.localIP().toString().c_str());
}

bool fetchManifest(String& versionOut, String& urlOut) {
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(15000);

  Serial.println(String("GET ") + MANIFEST_URL);
  if (!http.begin(MANIFEST_URL)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Manifest GET failed: %d\n", code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  versionOut = doc["version"].as<String>();
  urlOut     = doc["url"].as<String>();
  Serial.printf("Manifest: version=%s\nURL=%s\n", versionOut.c_str(), urlOut.c_str());
  return true;
}

void doHttpOta(const String& fwUrl) {
  // HTTPS client (DEV ONLY: insecure). For production, use root CA with setCACert().
  WiFiClientSecure client;
  client.setInsecure();

  HTTPUpdate httpUpdate;
  httpUpdate.rebootOnUpdate(true); // device auto-reboots if update OK
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  Serial.println("Starting OTA...");
  t_httpUpdate_return ret = httpUpdate.update(client, fwUrl);

  if (ret == HTTP_UPDATE_OK) {
    Serial.println("OTA success (device will reboot)!");
  } else if (ret == HTTP_UPDATE_NO_UPDATES) {
    Serial.println("No update available on server.");
  } else { // HTTP_UPDATE_FAILED
    Serial.printf("OTA failed. err=%d: %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);

  connectWiFi();

  // 1) Read manifest
  String latestVer, fwUrl;
  if (fetchManifest(latestVer, fwUrl)) {
    // 2) Compare versions
    if (isNewerVersion(FW_VERSION, latestVer) && fwUrl.length() > 0) {
      Serial.printf("Newer version found (%s -> %s). Updating...\n", FW_VERSION, latestVer.c_str());
      // 3) Do OTA (device will reboot on success)
      doHttpOta(fwUrl);
    } else {
      Serial.printf("Up-to-date (current %s, remote %s)\n", FW_VERSION, latestVer.c_str());
    }
  } else {
    Serial.println("Manifest fetch failed; continue with current firmware.");
  }
}

void loop() {
  // Your app logic. For example:
  // v1.0.0: only LED1 blinks
  digitalWrite(LED1, HIGH); delay(300);
  digitalWrite(LED1, LOW);  delay(300);

  // keep LED2 off in v1.0.0
  digitalWrite(LED2, LOW);
}

