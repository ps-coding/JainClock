#include <WiFi.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>

char ssid[] = "Arihant24g";
char password[] = "7328299909";

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
  unsigned long secondsElapsed = secondsElapsedSince(timeFetchedAtMs);

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

// ========== UNIFIED FETCH FUNCTION ==========
// Single call to aggregation API returns: geo, time, sun, tithi data in one response
bool fetchAggregatedDataFromServer(TimeData& timeData, GeoData& geo, SunData& sun, JainTithiData& tithi) {
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
    geo.zip = String(geoObj["zip"] | "");
    geo.city = String(geoObj["city"] | "");
    geo.country = String(geoObj["country"] | "");
    geo.lat = geoObj["lat"] | 0.0;
    geo.lon = geoObj["lon"] | 0.0;
    geo.timezoneOffset = geoObj["timezoneOffset"] | 0.0;
  }

  // Parse TIME data
  JsonObject timeObj = doc["time"].as<JsonObject>();
  if (!timeObj.isNull()) {
    String dateStr = String(timeObj["date"] | "");
    String timeStr = String(timeObj["time"] | "");
    timeData.iso = String(timeObj["iso"] | "");
    timeData.localTime = timeStr;

    // Parse date: YYYY-MM-DD
    if (dateStr.length() >= 10) {
      timeData.year = atoi(dateStr.substring(0, 4).c_str());
      timeData.month = atoi(dateStr.substring(5, 7).c_str());
      timeData.day = atoi(dateStr.substring(8, 10).c_str());
    }

    // Parse time: HH:MM:SS
    if (timeStr.length() >= 8) {
      timeData.hour = atoi(timeStr.substring(0, 2).c_str());
      timeData.minute = atoi(timeStr.substring(3, 5).c_str());
      timeData.second = atoi(timeStr.substring(6, 8).c_str());
    }
  }

  // Parse SUN data
  JsonObject sunObj = doc["sun"].as<JsonObject>();
  if (!sunObj.isNull()) {
    sun.sunrise = String(sunObj["sunrise"] | "");
    sun.sunset = String(sunObj["sunset"] | "");
  }

  // Parse TITHI data
  JsonObject tithiObj = doc["tithi"].as<JsonObject>();
  if (!tithiObj.isNull()) {
    tithi.number = tithiObj["number"] | -1;
    tithi.name = String(tithiObj["name"] | "");
    tithi.paksha = String(tithiObj["paksha"] | "");
    tithi.completesAt = String(tithiObj["completesAt"] | "");
    tithi.leftPercentage = tithiObj["leftPercentage"] | 0.0;
  }

  // Validation: Ensure we got the essential data
  if (timeData.year < 1 || geo.lat < -90.0 || geo.lat > 90.0 || 
      tithi.number < 1 || tithi.name.length() == 0) {
    logFail("aggapi", -2);
    return false;
  }

  logOk("aggapi", "all_data_fetched");

  return true;
}

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
  delay(2000);  // Give serial time to stabilize
  
  connectWiFi();
}

void loop() {
  // ========== FETCH ALL DATA (first boot + every 6 hours) ==========
  // Unlike the old approach (4 separate calls), this single call gets: geo, time, weather, tithi
  bool needsDataRefresh = (timeFetchedAtMs == 0) || (secondsElapsedSince(timeFetchedAtMs) >= SIX_HOURS_MS / 1000);
  
  if (needsDataRefresh) {
    if (!fetchAggregatedDataFromServer(cachedTimeData, cachedGeo, cachedSun, cachedTithi)) {
      delay(30000);
      return;
    }

    timeFetchedAtMs = millis();
    tithiFetchedAtMs = millis();
    
    // Calculate when this tithi expires
    tithiExpiresAtMs = millis() + parseCompleteTimeToFutureMs(cachedTithi.completesAt);
    
    logOk("schedule", "data_refreshed");
  } else {
    // Update local time based on elapsed milliseconds since fetch
    updateLocalTimeFromCache();
  }

  // ========== SMART SLEEP ==========
  // Calculate next wake-up time based on nearest refresh deadline
  unsigned long nextWakeUpMs = ULONG_MAX;
  
  // Data refresh in 6 hours
  if (timeFetchedAtMs > 0) {
    unsigned long nextDataRefresh = timeFetchedAtMs + SIX_HOURS_MS;
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
