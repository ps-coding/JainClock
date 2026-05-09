#pragma once

struct GeoData;
struct TimeData;
struct SunData;
struct JainTithiData;

#include <Arduino.h>
#include <lvgl.h>

enum UIView {
  CLOCK_VIEW,
  PACHKHAN_LIST_VIEW,
  PACHKHAN_DETAIL_VIEW
};

class UIManager {
public:
  void begin();
  void showLoadingState();  // Show loading screen before WiFi connects
  void showData(const String& ip, const TimeData* time, const GeoData* geo, const SunData* sun, const JainTithiData* tithi);
  void handlePachkhanButton();
  void showPachkhanList();
  void showPachkhanDetail(int index);
  void showClockView();
  UIView getCurrentView() const { return currentView; }
  
private:
  void createLayout();
  void createPachkhanListView();
  void createPachkhanDetailView();
  
  UIView currentView = CLOCK_VIEW;
  int selectedPachkhanIndex = 0;
  
  lv_obj_t* lblTime = nullptr;
  lv_obj_t* lbDate = nullptr;
  lv_obj_t* lblSun = nullptr;
  lv_obj_t* lblTithi = nullptr;
  lv_obj_t* btnPachkhan = nullptr;
  lv_obj_t* lblLoading = nullptr;
  
  // List view components
  lv_obj_t* listContainer = nullptr;
  lv_obj_t* pachkhanButtons[16] = {nullptr};
  lv_obj_t* btnExitFromList = nullptr;
  
  // Detail view components
  lv_obj_t* detailContainer = nullptr;
  lv_obj_t* lblDetailName = nullptr;
  lv_obj_t* lblDetailDescription = nullptr;
  lv_obj_t* lblDetailTranscript = nullptr;
  lv_obj_t* btnBackFromDetail = nullptr;
};

extern UIManager UI;
