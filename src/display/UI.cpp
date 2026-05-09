#include "UI.h"
#include "../Models.h"
#include "../Pachkhan.h"
#include <Arduino_H7_Video.h>
#include <Arduino_GigaDisplayTouch.h>
#include "lvgl.h"

Arduino_H7_Video Display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

UIManager UI;

static void format12Hour(int hour24, int minute, int second, char* out, size_t outSize) {
  int hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;
  const char* ampm = (hour24 >= 12) ? "PM" : "AM";
  snprintf(out, outSize, "%02d:%02d:%02d %s", hour12, minute, second, ampm);
}

static void pachkhanButtonEventHandler(lv_event_t* e);
static void pachkhanListItemEventHandler(lv_event_t* e);
static void pachkhanListExitEventHandler(lv_event_t* e);
static void pachkhanDetailBackEventHandler(lv_event_t* e);

void UIManager::createLayout() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  lblTime = lv_label_create(scr);
  lv_obj_set_width(lblTime, 790);
  lv_label_set_long_mode(lblTime, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblTime, LV_ALIGN_CENTER, 0, -70);
  lv_obj_set_style_text_color(lblTime, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_align(lblTime, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(lblTime, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_label_set_text(lblTime, "");

  lbDate = lv_label_create(scr);
  lv_obj_set_width(lbDate, 380);
  lv_label_set_long_mode(lbDate, LV_LABEL_LONG_DOT);
  lv_obj_align(lbDate, LV_ALIGN_BOTTOM_LEFT, 10, -56);
  lv_obj_set_style_text_color(lbDate, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbDate, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_line_space(lbDate, 3, LV_PART_MAIN);
  lv_label_set_text(lbDate, "");

  lblSun = lv_label_create(scr);
  lv_obj_set_width(lblSun, 380);
  lv_label_set_long_mode(lblSun, LV_LABEL_LONG_DOT);
  lv_obj_align(lblSun, LV_ALIGN_BOTTOM_RIGHT, -10, -56);
  lv_obj_set_style_text_color(lblSun, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(lblSun, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_line_space(lblSun, 3, LV_PART_MAIN);
  lv_label_set_text(lblSun, "");

  lblTithi = lv_label_create(scr);
  lv_obj_set_width(lblTithi, 780);
  lv_label_set_long_mode(lblTithi, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(lblTithi, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_align(lblTithi, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_set_style_text_font(lblTithi, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(lblTithi, LV_ALIGN_BOTTOM_LEFT, 10, -16);
  lv_label_set_text(lblTithi, "");

  btnPachkhan = lv_btn_create(scr);
  lv_obj_set_size(btnPachkhan, 120, 40);
  lv_obj_align(btnPachkhan, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_set_style_bg_color(btnPachkhan, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_border_color(btnPachkhan, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_width(btnPachkhan, 2, LV_PART_MAIN);
  lv_obj_add_flag(btnPachkhan, LV_OBJ_FLAG_CLICKABLE);
  
  lv_obj_t* label = lv_label_create(btnPachkhan);
  lv_label_set_text(label, "Pachkhan");
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(label);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
  
  lv_obj_add_event_cb(btnPachkhan, pachkhanButtonEventHandler, LV_EVENT_CLICKED, nullptr);
}

void UIManager::createPachkhanListView() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_t* lblTitle = lv_label_create(scr);
  lv_label_set_text(lblTitle, "Pachkhan Practices");
  lv_obj_set_style_text_color(lblTitle, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(lblTitle, LV_ALIGN_TOP_MID, 0, 5);
  
  listContainer = lv_obj_create(scr);
  lv_obj_set_size(listContainer, 780, 300);
  lv_obj_align(listContainer, LV_ALIGN_TOP_MID, 0, 55);
  lv_obj_set_style_bg_color(listContainer, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_layout(listContainer, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(listContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(listContainer, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_row(listContainer, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_column(listContainer, 12, LV_PART_MAIN);
  
  for (int i = 0; i < 16; i++) {
    pachkhanButtons[i] = lv_btn_create(listContainer);
    lv_obj_set_size(pachkhanButtons[i], 230, 100);
    lv_obj_set_style_bg_color(pachkhanButtons[i], lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_border_color(pachkhanButtons[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(pachkhanButtons[i], 2, LV_PART_MAIN);
    lv_obj_add_flag(pachkhanButtons[i], LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t* label = lv_label_create(pachkhanButtons[i]);
    lv_label_set_text(label, pachkhanData[i].name);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(label, 4, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 220);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_set_user_data(pachkhanButtons[i], (void*)(intptr_t)i);
    lv_obj_add_event_cb(pachkhanButtons[i], pachkhanListItemEventHandler, LV_EVENT_CLICKED, nullptr);
  }
  
  btnExitFromList = lv_btn_create(scr);
  lv_obj_set_size(btnExitFromList, 100, 40);
  lv_obj_align(btnExitFromList, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_set_style_bg_color(btnExitFromList, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_border_color(btnExitFromList, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_width(btnExitFromList, 2, LV_PART_MAIN);
  lv_obj_add_flag(btnExitFromList, LV_OBJ_FLAG_CLICKABLE);
  
  lv_obj_t* exitLabel = lv_label_create(btnExitFromList);
  lv_label_set_text(exitLabel, "Exit");
  lv_obj_set_style_text_color(exitLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(exitLabel, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(exitLabel);
  lv_obj_clear_flag(exitLabel, LV_OBJ_FLAG_CLICKABLE);
  
  lv_obj_add_event_cb(btnExitFromList, pachkhanListExitEventHandler, LV_EVENT_CLICKED, nullptr);
}

void UIManager::createPachkhanDetailView() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  
  lblDetailName = lv_label_create(scr);
  lv_label_set_text(lblDetailName, pachkhanData[selectedPachkhanIndex].name);
  lv_obj_set_style_text_color(lblDetailName, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(lblDetailName, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_width(lblDetailName, 790);
  lv_label_set_long_mode(lblDetailName, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblDetailName, LV_ALIGN_TOP_MID, 0, 10);
  
  detailContainer = lv_obj_create(scr);
  lv_obj_set_size(detailContainer, 780, 350);
  lv_obj_align(detailContainer, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_bg_color(detailContainer, lv_color_hex(0x0a0a0a), LV_PART_MAIN);
  lv_obj_set_style_border_color(detailContainer, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_border_width(detailContainer, 1, LV_PART_MAIN);
  lv_obj_set_flex_flow(detailContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollbar_mode(detailContainer, LV_SCROLLBAR_MODE_AUTO);
  
  lblDetailDescription = lv_label_create(detailContainer);
  lv_label_set_text(lblDetailDescription, pachkhanData[selectedPachkhanIndex].description);
  lv_obj_set_style_text_color(lblDetailDescription, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
  lv_obj_set_style_text_font(lblDetailDescription, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_line_space(lblDetailDescription, 5, LV_PART_MAIN);
  lv_label_set_long_mode(lblDetailDescription, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblDetailDescription, 770);
  
  lblDetailTranscript = lv_label_create(detailContainer);
  lv_label_set_text(lblDetailTranscript, pachkhanData[selectedPachkhanIndex].transcript);
  lv_obj_set_style_text_color(lblDetailTranscript, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(lblDetailTranscript, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_line_space(lblDetailTranscript, 5, LV_PART_MAIN);
  lv_label_set_long_mode(lblDetailTranscript, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblDetailTranscript, 770);
  
  btnBackFromDetail = lv_btn_create(scr);
  lv_obj_set_size(btnBackFromDetail, 100, 40);
  lv_obj_align(btnBackFromDetail, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_set_style_bg_color(btnBackFromDetail, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_border_color(btnBackFromDetail, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_width(btnBackFromDetail, 2, LV_PART_MAIN);
  lv_obj_add_flag(btnBackFromDetail, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* backLabel = lv_label_create(btnBackFromDetail);
  lv_label_set_text(backLabel, "Back");
  lv_obj_set_style_text_color(backLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(backLabel);
  lv_obj_clear_flag(backLabel, LV_OBJ_FLAG_CLICKABLE);
  
  lv_obj_add_event_cb(btnBackFromDetail, pachkhanDetailBackEventHandler, LV_EVENT_CLICKED, nullptr);
}

void UIManager::begin() {
  lv_init();
  if (Display.begin()) {
    while (true) {
      delay(200);
    }
  }
  TouchDetector.begin();
  createLayout();
}

void UIManager::showLoadingState() {
  lv_obj_t* scr = lv_scr_act();
  if (!lblLoading) {
    lblLoading = lv_label_create(scr);
    lv_obj_align(lblLoading, LV_ALIGN_CENTER, 0, -100);
    lv_obj_set_style_text_color(lblLoading, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lblLoading, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(lblLoading, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  }
  static int dotCount = 0;
  String loadingText = "Connecting";
  for (int i = 0; i < (dotCount % 4); i++) {
    loadingText += ".";
  }
  lv_label_set_text(lblLoading, loadingText.c_str());
  dotCount++;
}

void UIManager::showData(const String& ip, const TimeData* time, const GeoData* geo, const SunData* sun, const JainTithiData* tithi) {
  if (lblLoading) {
    lv_obj_del(lblLoading);
    lblLoading = nullptr;
  }
  
  // Only update clock view
  if (currentView != CLOCK_VIEW) return;
  
  if (lblTime && time) {
    char time12[24];
    format12Hour(time->hour, time->minute, time->second, time12, sizeof(time12));
    char buf[48];
    snprintf(buf, sizeof(buf), "%s", time12);
    lv_label_set_text(lblTime, buf);
  }

  if (lbDate && geo) {
    String loc = "";
    if (time) {
      char dateBuf[24];
      snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%04d", time->month, time->day, time->year);
      loc += String(dateBuf) + "  |  ";
    }
    loc += geo->city.substring(0, 16);
    lv_label_set_text(lbDate, loc.c_str());
  }

  if (lblSun && sun) {
    String s = "Sunrise ";
    s += sun->sunrise;
    s += "  |  Sunset ";
    s += sun->sunset;
    lv_label_set_text(lblSun, s.c_str());
  }

  if (lblTithi && tithi) {
    String d = "Tithi: " + String(tithi->number) + " " + tithi->name;
    lv_label_set_text(lblTithi, d.c_str());
  }
}

void UIManager::handlePachkhanButton() {
  if (currentView == CLOCK_VIEW) {
    showPachkhanList();
  }
}

void UIManager::showPachkhanList() {
  currentView = PACHKHAN_LIST_VIEW;
  createPachkhanListView();
}

void UIManager::showPachkhanDetail(int index) {
  selectedPachkhanIndex = index;
  currentView = PACHKHAN_DETAIL_VIEW;
  createPachkhanDetailView();
}

void UIManager::showClockView() {
  currentView = CLOCK_VIEW;
  lv_obj_t* scr = lv_scr_act();
  lv_obj_clean(scr);
  createLayout();
}

// Event handlers (outside class)
static void pachkhanButtonEventHandler(lv_event_t* e) {
  UI.handlePachkhanButton();
}

static void pachkhanListItemEventHandler(lv_event_t* e) {
  lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
  int index = (int)(intptr_t)lv_obj_get_user_data(btn);
  UI.showPachkhanDetail(index);
}

static void pachkhanDetailBackEventHandler(lv_event_t* e) {
  UI.showPachkhanList();
}

static void pachkhanListExitEventHandler(lv_event_t* e) {
  UI.showClockView();
}
