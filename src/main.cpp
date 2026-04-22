#include <WiFi.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>

char ssid[] = "NB_Guest";
char password[] = "";

const unsigned long WIFI_TIMEOUT_MS = 15000;
const unsigned long HTTP_TIMEOUT_MS = 10000;

const char AGG_API_HOST[] = "jain-clock-server.vercel.app";
const int AGG_API_PORT = 443;
const char AGG_API_PATH[] = "/api/data";
const bool AGG_API_USE_HTTPS = true;

int status = WL_IDLE_STATUS;

void logOk(const String& scope, const String& details) {
  Serial.println("OK " + scope + " " + details);
}

void logFail(const String& scope, int statusCode) {
  Serial.println("FAIL " + scope + " status=" + String(statusCode));
}

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
  String city;
  String country;
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

void printFetchedData(const String& ip, const GeoData& geo, const TimeData& timeData, const SunData& sun, const JainTithiData& tithi) {
  Serial.println("----- Fetched Data -----");
  Serial.println("ip: " + ip);
  Serial.println("geo.status: success");
  Serial.println("geo.country: " + geo.country);
  Serial.println("geo.city: " + geo.city);
  Serial.println("geo.zip: " + geo.zip);
  Serial.println("geo.lat: " + String(geo.lat, 6));
  Serial.println("geo.lon: " + String(geo.lon, 6));
  Serial.println("geo.offset_hours: " + String(geo.timezoneOffset, 2));
  Serial.println("time.year: " + String(timeData.year));
  Serial.println("time.month: " + String(timeData.month));
  Serial.println("time.day: " + String(timeData.day));
  Serial.println("time.hour: " + String(timeData.hour));
  Serial.println("time.minute: " + String(timeData.minute));
  Serial.println("time.second: " + String(timeData.second));
  Serial.println("time.iso: " + timeData.iso);
  Serial.println("sun.sunrise: " + sun.sunrise);
  Serial.println("sun.sunset: " + sun.sunset);
  Serial.println("tithi.number: " + String(tithi.number));
  Serial.println("tithi.name: " + tithi.name);
  Serial.println("tithi.paksha: " + tithi.paksha);
  Serial.println("tithi.completesAt: " + tithi.completesAt);
  Serial.println("tithi.leftPercentage: " + String(tithi.leftPercentage, 2));
  Serial.println("------------------------");
}

// -------------------------
// State tracking for smart refresh scheduling
// -------------------------
const unsigned long SIX_HOURS_MS = 6 * 60 * 60 * 1000UL;
const unsigned long ONE_MINUTE_MS = 60 * 1000UL;

// Cached data (persists across loop iterations)
GeoData cachedGeo;
TimeData cachedTimeData;
SunData cachedSun;
JainTithiData cachedTithi;

// Timestamps (millis() when each data was fetched)
unsigned long dataFetchedAtMs = 0;
unsigned long tithiExpiresAtMs = 0;

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

bool isLeapYear(int year) {
  if (year % 400 == 0) return true;
  if (year % 100 == 0) return false;
  return (year % 4 == 0);
}

int daysInMonth(int year, int month) {
  if (month < 1 || month > 12) return 31;

  static const int monthDays[12] = {
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
  };

  if (month == 2 && isLeapYear(year)) {
    return 29;
  }

  return monthDays[month - 1];
}

// Helper: Update local time based on elapsed ms since fetch
void updateLocalTimeFromCache() {
  unsigned long secondsElapsed = secondsElapsedSince(dataFetchedAtMs);

  if (secondsElapsed == ULONG_MAX) {
    return;
  }

  unsigned long long baseSeconds =
    (unsigned long long)(cachedTimeData.hour * 3600) +
    (unsigned long long)(cachedTimeData.minute * 60) +
    (unsigned long long)cachedTimeData.second;

  unsigned long long totalSeconds = baseSeconds + (unsigned long long)secondsElapsed;
  unsigned long long daysToAdvance = totalSeconds / 86400ULL;
  unsigned long secondsOfDay = (unsigned long)(totalSeconds % 86400ULL);

  cachedTimeData.hour = (int)(secondsOfDay / 3600UL);
  cachedTimeData.minute = (int)((secondsOfDay % 3600UL) / 60UL);
  cachedTimeData.second = (int)(secondsOfDay % 60UL);

  int year = cachedTimeData.year;
  int month = cachedTimeData.month;
  int day = cachedTimeData.day;

  if (year < 1) year = 1970;
  if (month < 1) month = 1;
  if (month > 12) month = 12;
  int maxDay = daysInMonth(year, month);
  if (day < 1) day = 1;
  if (day > maxDay) day = maxDay;

  while (daysToAdvance > 0) {
    maxDay = daysInMonth(year, month);
    int remainingInMonth = maxDay - day;

    if (daysToAdvance <= (unsigned long long)remainingInMonth) {
      day += (int)daysToAdvance;
      daysToAdvance = 0;
    } else {
      daysToAdvance -= (unsigned long long)(remainingInMonth + 1);
      day = 1;
      month++;
      if (month > 12) {
        month = 1;
        year++;
      }
    }
  }

  cachedTimeData.year = year;
  cachedTimeData.month = month;
  cachedTimeData.day = day;
}

// Helper: Parse datetime string "YYYY-MM-DD HH:MM:SS" to milliseconds from now
unsigned long parseCompleteTimeToFutureMs(const String& dateTimeStr) {
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

bool httpGet(const char* host, int port, const String& path, bool useHttps, String& response, int& statusCode) {
  WiFiSSLClient httpsClient;
  WiFiClient httpClient;
  WiFiClient* client = useHttps ? (WiFiClient*)&httpsClient : &httpClient;

  client->setSocketTimeout(HTTP_TIMEOUT_MS);
  client->setTimeout(HTTP_TIMEOUT_MS);
  
  if (!client->connect(host, port)) {
    statusCode = 0;
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

// Single call to aggregation API returns: geo, time, sun, tithi data in one response
bool fetchAggregatedDataFromServer(String& ip, TimeData& timeData, GeoData& geo, SunData& sun, JainTithiData& tithi) {
  String response;
  int statusCode = 0;

  bool ok = httpGet(AGG_API_HOST, AGG_API_PORT, AGG_API_PATH, AGG_API_USE_HTTPS, response, statusCode);
  
  // Retry once on transport failure (status=0)
  if (!ok && statusCode == 0) {
    delay(300);
    ok = httpGet(AGG_API_HOST, AGG_API_PORT, AGG_API_PATH, AGG_API_USE_HTTPS, response, statusCode);
  }

  if (!ok) {
    logFail("aggapi", statusCode);
    return false;
  }

  String jsonBody = extractHttpBody(response);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    logFail("aggapi", -2);
    return false;
  }

  // Parse GEO data
  JsonObject geoObj = doc["geo"].as<JsonObject>();
  if (!geoObj.isNull()) {
    ip = String(doc["ip"] | "");
    geo.zip = String(geoObj["zip"] | "");
    geo.city = String(geoObj["city"] | "");
    geo.country = String(geoObj["country"] | "");
    geo.lat = geoObj["lat"] | 0.0;
    geo.lon = geoObj["lon"] | 0.0;
    geo.timezoneOffset = geoObj["offset"] | 0.0;
  }

  // Parse TIME data
  JsonObject timeObj = doc["time"].as<JsonObject>();
  if (!timeObj.isNull()) {
    // Read individual fields
    timeData.year = timeObj["year"] | 0;
    timeData.month = timeObj["month"] | 0;
    timeData.day = timeObj["day"] | 0;
    timeData.hour = timeObj["hour"] | 0;
    timeData.minute = timeObj["minute"] | 0;
    timeData.second = timeObj["seconds"] | 0;

    // Format time as HH:MM:SS for local display
    char buff[9];
    snprintf(buff, sizeof(buff), "%02d:%02d:%02d", timeData.hour, timeData.minute, timeData.second);
    timeData.localTime = String(buff);

    // Format ISO datetime
    char isoBuff[20];
    snprintf(isoBuff, sizeof(isoBuff), "%04d-%02d-%02d %02d:%02d:%02d", 
             timeData.year, timeData.month, timeData.day, 
             timeData.hour, timeData.minute, timeData.second);
    timeData.iso = String(isoBuff);
  }

  // Parse SUN data
  JsonObject sunObj = doc["sun"].as<JsonObject>();
  if (!sunObj.isNull()) {
    sun.sunrise = String(sunObj["sunrise"] | "");
    sun.sunset = String(sunObj["sunset"] | "");
  }

  // Parse TITHI data (returned as JSON-stringified string, need to parse it again)
  String tithiJsonStr = String(doc["tithi"] | "");
  if (tithiJsonStr.length() > 0) {
    JsonDocument tithiDoc;
    DeserializationError tithiErr = deserializeJson(tithiDoc, tithiJsonStr);
    if (!tithiErr) {
      tithi.number = tithiDoc["number"] | -1;
      tithi.name = String(tithiDoc["name"] | "");
      tithi.paksha = String(tithiDoc["paksha"] | "");
      tithi.completesAt = String(tithiDoc["completes_at"] | "");
      tithi.leftPercentage = tithiDoc["left_precentage"] | 0.0;
    }
  }

  // Validation: Ensure we got the essential data
  if (timeData.year < 1 || geo.lat < -90.0 || geo.lat > 90.0 || 
      tithi.number < 1 || tithi.name.length() == 0) {
    logFail("aggapi", -2);
    return false;
  }

  printFetchedData(ip, geo, timeData, sun, tithi);
  logOk("aggapi", "all_data_fetched");

  return true;
}

// Connect to WiFi with retries and logging
void connectWiFi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    logFail("wifi", WL_NO_MODULE);
    // don't continue
    while (true);
  }

  // attempt to connect to WiFi network:
  unsigned long connectStart = millis();
  while (status != WL_CONNECTED) {
    if (password == nullptr || strlen(password) == 0) {
      status = WiFi.begin(ssid);
    } else {
      status = WiFi.begin(ssid, password);
    }

    // wait 3 seconds for connection:
    delay(3000);
    if (millis() - connectStart >= WIFI_TIMEOUT_MS) {
      logFail("wifi", status);
      connectStart = millis();
    }
  }
  logOk("wifi", "ssid=" + String(ssid));
}

void setup() {
  Serial.begin(9600);
  delay(2000); // Give serial time to stabilize
  
  connectWiFi();
}

void loop() {
  bool needsDataRefresh = (dataFetchedAtMs == 0) || (secondsElapsedSince(dataFetchedAtMs) >= SIX_HOURS_MS / 1000);
  
  if (needsDataRefresh) {
    String cachedPublicIp;
    if (!fetchAggregatedDataFromServer(cachedPublicIp, cachedTimeData, cachedGeo, cachedSun, cachedTithi)) {
      delay(30000);
      return;
    }

    dataFetchedAtMs = millis();
    
    // Calculate when this tithi expires
    tithiExpiresAtMs = millis() + parseCompleteTimeToFutureMs(cachedTithi.completesAt);
    
    logOk("schedule", "data_refreshed");
  } else {
    // Update local time based on elapsed milliseconds since fetch
    updateLocalTimeFromCache();
  }
  // Calculate next wake-up time based on nearest refresh deadline
  unsigned long nextWakeUpMs = ULONG_MAX;
  
  // Data refresh in 6 hours
  if (dataFetchedAtMs > 0) {
    unsigned long nextDataRefresh = dataFetchedAtMs + SIX_HOURS_MS;
    if (nextDataRefresh < nextWakeUpMs) nextWakeUpMs = nextDataRefresh;
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
