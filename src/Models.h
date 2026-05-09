#pragma once

#include <Arduino.h>

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
