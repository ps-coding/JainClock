#include <WiFi.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>

char ssid[] = "NB_Guest";
const unsigned long WIFI_TIMEOUT_MS = 15000;
const unsigned long HTTP_TIMEOUT_MS = 10000;

const char IPIFY_HOST[] = "api.ipify.org";
const int IPIFY_PORT = 443;
const char IPIFY_PATH[] = "/?format=json";

const char TIME_API_HOST[] = "timeapi.io";
const int TIME_API_PORT = 443;
const char TIME_API_CURRENT_IP_PATH[] = "/api/v1/time/current/ip";

const char IP_API_HOST[] = "ip-api.com";
const int IP_API_PORT = 80;
const char IP_API_PATH[] = "/json/?fields=status,message,zip,lat,lon,offset";

const char WEATHER_API_HOST[] = "api.weatherapi.com";
const int WEATHER_API_PORT = 443;
const int WEATHER_API_HTTP_PORT = 80;
const char WEATHER_API_KEY[] = "442cbb78d4644d56ae133317262302";

const char FREE_ASTROLOGY_HOST[] = "json.freeastrologyapi.com";
const int FREE_ASTROLOGY_PORT = 443;
const char FREE_ASTROLOGY_API_KEY[] = "r9DOBHaobf9x1HHi01cooaZkENtqnI7O9tmx1LIy";
const char FREE_ASTROLOGY_PATH[] = "/tithi-durations";

int status = WL_IDLE_STATUS;

String extractHttpBody(const String& response) {
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart != -1) {
    return response.substring(bodyStart + 4);
  }
  int bodyStart2 = response.indexOf("\n\n");
  if (bodyStart2 != -1) {
    return response.substring(bodyStart2 + 2);
  }
  return response;
}

int extractHttpStatus(const String& response) {
  int spacePos = response.indexOf(' ');
  if (spacePos == -1) return -1;
  int secondSpace = response.indexOf(' ', spacePos + 1);
  if (secondSpace == -1) return -1;
  String statusStr = response.substring(spacePos + 1, secondSpace);
  return atoi(statusStr.c_str());
}

struct GeoData {
  String zip;
  float timezoneOffset;
  float lat;
  float lon;
};

struct TimeData {
  String iso;
  String localTime;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

struct SunData {
  String sunrise;
  String sunset;
};

struct JainTithiData {
  int number;
  String name;
  String paksha;
  String completesAt;
  float leftPercentage;
};

// -------------------------
// State tracking for smart refresh scheduling
// -------------------------
const unsigned long SIX_HOURS_MS = 6 * 60 * 60 * 1000UL;
const unsigned long ONE_MINUTE_MS = 60 * 1000UL;

// Cached data (persists across loop iterations)
String cachedPublicIp = "";
GeoData cachedGeo;
TimeData cachedTimeData;
SunData cachedSun;
JainTithiData cachedTithi;

// Timestamps (millis() when each data was fetched)
unsigned long ipFetchedAtMs = 0;
unsigned long timeFetchedAtMs = 0;
unsigned long tithiFetchedAtMs = 0;
unsigned long tithiExpiresAtMs = 0;  // When the tithi duration ends (from completesAt)

// Helper: Calculate seconds elapsed since a timestamp
unsigned long secondsElapsedSince(unsigned long timestampMs) {
  if (timestampMs == 0) return ULONG_MAX;  // Never fetched
  unsigned long elapsed = millis() - timestampMs;
  return elapsed / 1000;
}

// Helper: Parse HH:MM:SS string to total seconds
unsigned long parseTimeToSeconds(const String& timeStr) {
  // Expected format: "HH:MM:SS"
  int colon1 = timeStr.indexOf(':');
  int colon2 = timeStr.indexOf(':', colon1 + 1);
  if (colon1 == -1 || colon2 == -1) return 0;
  
  int hours = atoi(timeStr.substring(0, colon1).c_str());
  int minutes = atoi(timeStr.substring(colon1 + 1, colon2).c_str());
  int seconds = atoi(timeStr.substring(colon2 + 1).c_str());
  
  return (unsigned long)hours * 3600 + (unsigned long)minutes * 60 + (unsigned long)seconds;
}

// Helper: Update local time based on elapsed ms since fetch
// TODO: FIX THIS TO HANDLE ALL EDGE CASES!
void updateLocalTimeFromCache() {
  unsigned long secondsElapsed = secondsElapsedSince(timeFetchedAtMs);
  
  // Calculate new time
  unsigned long totalSeconds = parseTimeToSeconds(
    String(cachedTimeData.hour) + ":" + String(cachedTimeData.minute) + ":" + String(cachedTimeData.second)
  );
  totalSeconds += secondsElapsed;
  
  // Handle day overflow
  if (totalSeconds >= 86400) {
    totalSeconds -= 86400;
    cachedTimeData.day++;
    if (cachedTimeData.day > 31) {  // Simplified; doesn't account for month length
      cachedTimeData.day = 1;
      cachedTimeData.month++;
      if (cachedTimeData.month > 12) {
        cachedTimeData.month = 1;
        cachedTimeData.year++;
      }
    }
  }
  
  cachedTimeData.hour = totalSeconds / 3600;
  cachedTimeData.minute = (totalSeconds % 3600) / 60;
  cachedTimeData.second = totalSeconds % 60;
}

// Helper: Parse datetime string "YYYY-MM-DD HH:MM:SS" to milliseconds from now
unsigned long parseCompleteTimeToFutureMs(const String& dateTimeStr) {
  // Expected format: "2026-03-25 14:30:45"
  // Returns: milliseconds until that time (assuming same day or next day if already passed)
  
  // For simplicity, assume completesAt is today or tomorrow
  // We'll calculate seconds until that time and convert to ms
  int spacePos = dateTimeStr.indexOf(' ');
  if (spacePos == -1) return 0;
  
  String timeStr = dateTimeStr.substring(spacePos + 1);
  unsigned long targetSeconds = parseTimeToSeconds(timeStr);
  unsigned long currentSeconds = parseTimeToSeconds(
    String(cachedTimeData.hour) + ":" + String(cachedTimeData.minute) + ":" + String(cachedTimeData.second)
  );
  
  if (targetSeconds > currentSeconds) {
    return (targetSeconds - currentSeconds) * 1000UL;
  } else {
    // Next day
    return (86400 - currentSeconds + targetSeconds) * 1000UL;
  }
}

String urlEncode(const String& value) {
  String encoded;
  char hex[4];

  for (size_t i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
      encoded += hex;
    }
  }

  return encoded;
}

bool httpGet(const char* host, int port, const String& path, bool useHttps, String& response, int& statusCode) {
  WiFiSSLClient httpsClient;
  WiFiClient httpClient;
  WiFiClient* client = useHttps ? (WiFiClient*)&httpsClient : &httpClient;
  
  if (!client->connect(host, port)) {
    Serial.print("Failed to connect to ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
    return false;
  }

  String request = "GET ";
  request += path;
  // Use HTTP/1.0 to reduce chunked transfer responses on constrained clients.
  request += " HTTP/1.0\r\nHost: ";
  request += host;
  request += "\r\nAccept: application/json\r\nConnection: close\r\n\r\n";
  
  client->print(request);
  
  response = "";
  unsigned long timeout = millis() + HTTP_TIMEOUT_MS;
  while (client->connected() && millis() < timeout) {
    if (client->available()) {
      response += (char)client->read();
    }
  }
  
  client->stop();
  
  statusCode = extractHttpStatus(response);
  return statusCode >= 200 && statusCode < 300;
}

bool httpPostJson(
  const char* host,
  int port,
  const String& path,
  bool useHttps,
  const String& jsonBody,
  const char* apiKeyHeaderName,
  const char* apiKeyValue,
  String& response,
  int& statusCode
) {
  WiFiSSLClient httpsClient;
  WiFiClient httpClient;
  WiFiClient* client = useHttps ? (WiFiClient*)&httpsClient : &httpClient;
  
  if (!client->connect(host, port)) {
    Serial.print("Failed to connect to ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
    return false;
  }

  String request = "POST ";
  request += path;
  request += " HTTP/1.1\r\nHost: ";
  request += host;
  request += "\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: ";
  request += jsonBody.length();
  request += "\r\n";
  
  if (apiKeyHeaderName != nullptr && apiKeyValue != nullptr) {
    request += apiKeyHeaderName;
    request += ": ";
    request += apiKeyValue;
    request += "\r\n";
  }
  
  request += "Connection: close\r\n\r\n";
  request += jsonBody;
  
  client->print(request);
  
  response = "";
  unsigned long timeout = millis() + HTTP_TIMEOUT_MS;
  while (client->connected() && millis() < timeout) {
    if (client->available()) {
      response += (char)client->read();
    }
  }
  
  client->stop();
  
  statusCode = extractHttpStatus(response);
  return statusCode >= 200 && statusCode < 300;
}

bool fetchPublicIp(String& ip) {
  String response;
  int statusCode = 0;
  bool ok = httpGet(IPIFY_HOST, IPIFY_PORT, IPIFY_PATH, true, response, statusCode);
  if (!ok) {
    Serial.print("ipify request failed. HTTP ");
    Serial.println(statusCode);
    return false;
  }

  String jsonBody = extractHttpBody(response);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.print("ipify JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  ip = String(doc["ip"] | "");
  if (ip.length() == 0) {
    Serial.println("ipify response missing ip field.");
    return false;
  }

  return true;
}

bool fetchTimeFromTimeApi(const String& ipAddress, TimeData& timeData) {
  String response;
  int statusCode = 0;
  String path = String(TIME_API_CURRENT_IP_PATH) + "?ipAddress=" + urlEncode(ipAddress);

  bool ok = httpGet(TIME_API_HOST, TIME_API_PORT, path, true, response, statusCode);
  if (!ok) {
    Serial.print("timeapi.io request failed. HTTP ");
    Serial.println(statusCode);
    String errorBody = extractHttpBody(response);
    Serial.print("timeapi.io request path: ");
    Serial.println(path);
    Serial.print("timeapi.io error body: ");
    if (errorBody.length() == 0) {
      Serial.println("<empty>");
    } else {
      Serial.println(errorBody);
    }
    return false;
  }

  String jsonBody = extractHttpBody(response);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.print("timeapi.io JSON parse failed: ");
    Serial.println(err.c_str());
    Serial.print("timeapi.io raw body: ");
    if (jsonBody.length() == 0) {
      Serial.println("<empty>");
    } else {
      Serial.println(jsonBody);
    }
    return false;
  }

  String dateStr = String(doc["date"] | "");
  String timeStr = String(doc["time"] | "");

  timeData.localTime = timeStr;
  if (dateStr.length() > 0 && timeStr.length() > 0) {
    timeData.iso = dateStr + " " + timeStr;
  } else {
    timeData.iso = "";
  }

  // Parse date: YYYY-MM-DD
  if (dateStr.length() >= 10 && dateStr.charAt(4) == '-' && dateStr.charAt(7) == '-') {
    timeData.year = atoi(dateStr.substring(0, 4).c_str());
    timeData.month = atoi(dateStr.substring(5, 7).c_str());
    timeData.day = atoi(dateStr.substring(8, 10).c_str());
  } else {
    timeData.year = 0;
    timeData.month = 0;
    timeData.day = 0;
  }

  // Parse time: HH:MM:SS (ignore anything after seconds)
  if (timeStr.length() >= 8 && timeStr.charAt(2) == ':' && timeStr.charAt(5) == ':') {
    timeData.hour = atoi(timeStr.substring(0, 2).c_str());
    timeData.minute = atoi(timeStr.substring(3, 5).c_str());
    timeData.second = atoi(timeStr.substring(6, 8).c_str());
  } else {
    timeData.hour = 0;
    timeData.minute = 0;
    timeData.second = 0;
  }

  if (timeData.localTime.length() == 0 && timeData.hour >= 0) {
    char buff[9];
    snprintf(buff, sizeof(buff), "%02d:%02d:%02d", timeData.hour, timeData.minute, timeData.second);
    timeData.localTime = String(buff);
  }

  if (timeData.localTime.length() == 0 && timeData.iso.length() == 0) {
    Serial.println("timeapi.io response did not include expected time fields.");
    return false;
  }

  if (timeData.year == 0 || timeData.month == 0 || timeData.day == 0) {
    Serial.println("timeapi.io response missing date components for astrology payload.");
    return false;
  }

  return true;
}

bool fetchGeoFromIpApi(GeoData& geo) {
  String response;
  int statusCode = 0;

  bool ok = httpGet(IP_API_HOST, IP_API_PORT, IP_API_PATH, false, response, statusCode);
  if (!ok) {
    Serial.print("ip-api request failed. HTTP ");
    Serial.println(statusCode);
    return false;
  }

  String jsonBody = extractHttpBody(response);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.print("ip-api JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  String status = String(doc["status"] | "fail");
  if (status != "success") {
    Serial.print("ip-api status error: ");
    Serial.println(String(doc["message"] | "unknown"));
    return false;
  }

  geo.zip = String(doc["zip"] | "");
  geo.lat = doc["lat"] | 0.0;
  geo.lon = doc["lon"] | 0.0;

  int offsetSeconds = doc["offset"] | 0;
  geo.timezoneOffset = (float)offsetSeconds / 3600.0f;

  if (geo.lat < -90.0 || geo.lat > 90.0 || geo.lon < -180.0 || geo.lon > 180.0) {
    Serial.println("ip-api returned invalid coordinates.");
    return false;
  }

  return true;
}

bool fetchSunTimesFromWeatherApi(const String& ipAddress, const String& dateIso, SunData& data) {
  String response;
  int statusCode = 0;

  String qValue = ipAddress.length() > 0 ? ipAddress : "auto:ip";

  String path = "/v1/astronomy.json?key=";
  path += WEATHER_API_KEY;
  path += "&q=";
  path += urlEncode(qValue);
  path += "&dt=";
  path += urlEncode(dateIso);

  Serial.print("weatherapi q value: ");
  Serial.println(qValue);

  bool ok = httpGet(WEATHER_API_HOST, WEATHER_API_PORT, path, true, response, statusCode);

  // Some networks/boards intermittently fail TLS handshakes to weatherapi.
  // Retry over plain HTTP as a connectivity fallback when status is 0.
  if (!ok && statusCode == 0) {
    Serial.println("weatherapi HTTPS failed with status 0, retrying over HTTP...");
    ok = httpGet(WEATHER_API_HOST, WEATHER_API_HTTP_PORT, path, false, response, statusCode);
  }

  if (!ok) {
    Serial.print("weatherapi request failed. HTTP ");
    Serial.println(statusCode);
    Serial.print("weatherapi request path: ");
    Serial.println(path);
    String errorBody = extractHttpBody(response);
    Serial.print("weatherapi error body: ");
    if (errorBody.length() == 0) {
      Serial.println("<empty>");
    } else {
      Serial.println(errorBody);
    }
    return false;
  }

  String jsonBody = extractHttpBody(response);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.print("weatherapi JSON parse failed: ");
    Serial.println(err.c_str());
    Serial.print("weatherapi raw body: ");
    if (jsonBody.length() == 0) {
      Serial.println("<empty>");
    } else {
      Serial.println(jsonBody);
    }
    return false;
  }

  data.sunrise = String(doc["astronomy"]["astro"]["sunrise"] | "");
  data.sunset = String(doc["astronomy"]["astro"]["sunset"] | "");

  if (data.sunrise.length() == 0 || data.sunset.length() == 0) {
    Serial.println("weatherapi response missing sunrise/sunset.");
    return false;
  }

  return true;
}

bool fetchJainTithiFromFreeAstrology(
  const TimeData& timeData,
  float latitude,
  float longitude,
  float timezoneOffset,
  JainTithiData& data
) {
  JsonDocument payloadDoc;
  payloadDoc["year"] = timeData.year;
  payloadDoc["month"] = timeData.month;
  payloadDoc["date"] = timeData.day;
  payloadDoc["hours"] = timeData.hour;
  payloadDoc["minutes"] = timeData.minute;
  payloadDoc["seconds"] = timeData.second;
  payloadDoc["latitude"] = latitude;
  payloadDoc["longitude"] = longitude;
  payloadDoc["timezone"] = timezoneOffset;
  payloadDoc["config"]["observation_point"] = "topocentric";
  payloadDoc["config"]["ayanamsha"] = "lahiri";

  String payload;
  serializeJson(payloadDoc, payload);

  String response;
  int statusCode = 0;
  bool ok = httpPostJson(
    FREE_ASTROLOGY_HOST,
    FREE_ASTROLOGY_PORT,
    FREE_ASTROLOGY_PATH,
    true,
    payload,
    "x-api-key",
    FREE_ASTROLOGY_API_KEY,
    response,
    statusCode
  );

  if (!ok) {
    Serial.print("freeastrologyapi request failed. HTTP ");
    Serial.println(statusCode);
    return false;
  }

  String jsonBody = extractHttpBody(response);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.print("freeastrologyapi JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  data.number = doc["number"] | -1;
  data.name = String(doc["name"] | "");
  data.paksha = String(doc["paksha"] | "");
  data.completesAt = String(doc["completes_at"] | "");
  data.leftPercentage = doc["left_precentage"] | doc["left_percentage"] | 0.0;

  if (data.name.length() == 0 || data.number < 1) {
    Serial.println("freeastrologyapi response missing tithi fields.");
    return false;
  }

  return true;
}

void connectWiFi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  // attempt to connect to WiFi network:
  unsigned long connectStart = millis();
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open network:
    status = WiFi.begin(ssid);

    // wait 3 seconds for connection:
    delay(3000);
    if (millis() - connectStart >= WIFI_TIMEOUT_MS) {
      Serial.println("WiFi connection timed out, retrying...");
      connectStart = millis();
    }
  }
  Serial.println("Connected to WiFi");
}

void printApiSnapshot(const GeoData& geo, const TimeData& timeData, const SunData& sun, const JainTithiData& tithi) {
  Serial.println("----- Jain Clock Data Snapshot -----");
  Serial.print("ZIP: ");
  Serial.println(geo.zip);
  Serial.print("Timezone offset: ");
  Serial.println(geo.timezoneOffset, 2);
  Serial.print("Lat/Lon: ");
  Serial.print(geo.lat, 6);
  Serial.print(", ");
  Serial.println(geo.lon, 6);

  Serial.print("Current time: ");
  if (timeData.localTime.length()) {
    Serial.println(timeData.localTime);
  } else {
    Serial.println(timeData.iso);
  }

  Serial.print("Sunrise: ");
  Serial.println(sun.sunrise);
  Serial.print("Sunset: ");
  Serial.println(sun.sunset);

  Serial.print("Jain tithi: ");
  Serial.print(tithi.name);
  Serial.print(" (#");
  Serial.print(tithi.number);
  Serial.println(")");
  Serial.print("Paksha: ");
  Serial.println(tithi.paksha);
  Serial.print("Completes at: ");
  Serial.println(tithi.completesAt);
  Serial.print("Left (%): ");
  Serial.println(tithi.leftPercentage, 2);
  Serial.println("------------------------------------");
}

void setup() {
  Serial.begin(9600);
  delay(2000);  // Give serial time to stabilize
  
  connectWiFi();
}

void loop() {
  // ========== FETCH IP (once, at startup) ==========
  if (ipFetchedAtMs == 0) {
    if (!fetchPublicIp(cachedPublicIp)) {
      delay(30000);
      return;
    }
    ipFetchedAtMs = millis();
  }

  // ========== FETCH TIME & GEO (first boot + every 6 hours) ==========
  bool needsTimeRefresh = (timeFetchedAtMs == 0) || (secondsElapsedSince(timeFetchedAtMs) >= SIX_HOURS_MS / 1000);
  
  if (needsTimeRefresh) {
    if (!fetchTimeFromTimeApi(cachedPublicIp, cachedTimeData)) {
      delay(30000);
      return;
    }

    if (!fetchGeoFromIpApi(cachedGeo)) {
      delay(30000);
      return;
    }

    timeFetchedAtMs = millis();
    
    // Generate date string for weather API
    String dateIso = String(cachedTimeData.year) + "-";
    if (cachedTimeData.month < 10) dateIso += "0";
    dateIso += String(cachedTimeData.month) + "-";
    if (cachedTimeData.day < 10) dateIso += "0";
    dateIso += String(cachedTimeData.day);
    
    // ========== FETCH WEATHER (with time refresh + every 6 hours) ==========
    if (!fetchSunTimesFromWeatherApi(cachedPublicIp, dateIso, cachedSun)) {
      delay(30000);
      return;
    }
  } else {
    // Update local time based on elapsed milliseconds since fetch
    updateLocalTimeFromCache();
  }

  // ========== FETCH TITHI (first boot + 1 minute after expiry) ==========
  bool needsTithiRefresh = false;
  
  if (tithiFetchedAtMs == 0) {
    // First fetch
    needsTithiRefresh = true;
  } else if (tithiExpiresAtMs > 0 && millis() >= tithiExpiresAtMs) {
    // Tithi has expired, wait 1 minute then refresh
    unsigned long timeSinceExpiry = millis() - tithiExpiresAtMs;
    if (timeSinceExpiry >= ONE_MINUTE_MS) {
      needsTithiRefresh = true;
    }
  }
  
  if (needsTithiRefresh) {
    if (!fetchJainTithiFromFreeAstrology(cachedTimeData, cachedGeo.lat, cachedGeo.lon, cachedGeo.timezoneOffset, cachedTithi)) {
      delay(30000);
      return;
    }
    tithiFetchedAtMs = millis();
    
    // Calculate when this tithi expires
    tithiExpiresAtMs = millis() + parseCompleteTimeToFutureMs(cachedTithi.completesAt);
  }

  // ========== DISPLAY CURRENT STATE ==========
  printApiSnapshot(cachedGeo, cachedTimeData, cachedSun, cachedTithi);

  // ========== SMART SLEEP ==========
  // Calculate next wake-up time based on nearest refresh deadline
  unsigned long nextWakeUpMs = ULONG_MAX;
  
  // Time/weather refresh in 6 hours
  if (timeFetchedAtMs > 0) {
    unsigned long nextTimeRefresh = timeFetchedAtMs + SIX_HOURS_MS;
    if (nextTimeRefresh < nextWakeUpMs) nextWakeUpMs = nextTimeRefresh;
  }
  
  // Tithi refresh (expiry + 1 minute)
  if (tithiExpiresAtMs > 0) {
    unsigned long nextTithiRefresh = tithiExpiresAtMs + ONE_MINUTE_MS;
    if (nextTithiRefresh < nextWakeUpMs) nextWakeUpMs = nextTithiRefresh;
  }
  
  unsigned long sleepMs = 300000;  // Default: 5 minutes (for display updates, etc.)
  if (nextWakeUpMs != ULONG_MAX) {
    long secondsUntilRefresh = (long)(nextWakeUpMs - millis()) / 1000;
    if (secondsUntilRefresh > 0) {
      sleepMs = min((unsigned long)secondsUntilRefresh * 1000, 300000UL);  // Cap at 5 min
    }
  }
  
  delay(sleepMs);
}
