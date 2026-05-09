#ifndef LV_CONF_H
#define LV_CONF_H

/* Display */
#define LV_HOR_RES_MAX 800
#define LV_VER_RES_MAX 480
#define LV_COLOR_DEPTH 16

/* Rendering */
#define LV_DRAW_SW_SUPPORT_MASK 0
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_REFR_PERIOD_MINIMAL 16

/* Disable unused features */
#define LV_USE_SHADOW 0
#define LV_USE_BLEND_MODES 0
#define LV_USE_IMGFONT 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_MONO 0
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_DRAW_SDL 0
#define LV_USE_DRAW_NXPVGLITE 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_ANIM_TICK_DEFAULT 1
#define LV_MEM_SIZE (40 * 1024)

/* Fonts */
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#endif