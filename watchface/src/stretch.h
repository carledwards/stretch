#pragma once

#include "pebble.h"

#define DIGIT_HEIGHT         122
#define DIGIT_WIDTH          21
#define COLON_HEIGHT         5
#define COLON_WIDTH          5
#define SPACE_BETWEEN_DIGIT  7
#define COLON_PADDING        14
#define BATTERY_WIDTH_IN_PIXELS  140

#define TIME_TOP_MARGIN      6

#define FOUR_DIGITS_TIME_LEFT_MARGIN      8
#define FOUR_DIGITS_HOUR_TENS_LAYER_GRECT GRect(FOUR_DIGITS_TIME_LEFT_MARGIN, TIME_TOP_MARGIN, DIGIT_WIDTH, DIGIT_HEIGHT)
#define FOUR_DIGITS_REMAINING_LAYER_GRECT GRect(FOUR_DIGITS_TIME_LEFT_MARGIN+DIGIT_WIDTH+SPACE_BETWEEN_DIGIT, TIME_TOP_MARGIN, 3*DIGIT_WIDTH+2*SPACE_BETWEEN_DIGIT+(COLON_PADDING+COLON_WIDTH), DIGIT_HEIGHT)  

#define THREE_DIGITS_TIME_LEFT_MARGIN      24
#define THREE_DIGITS_HOUR_TENS_LAYER_GRECT GRect(-10, -10, 1, 1)
#define THREE_DIGITS_REMAINING_LAYER_GRECT GRect(THREE_DIGITS_TIME_LEFT_MARGIN, TIME_TOP_MARGIN, 3*DIGIT_WIDTH+2*SPACE_BETWEEN_DIGIT+(COLON_PADDING+COLON_WIDTH), DIGIT_HEIGHT)  

enum {
  STRETCH_KEY_BLINK_DOTS = 0x0,
  STRETCH_KEY_IS_24HOUR = 0x1,
  STRETCH_KEY_TEXT_COLOR_BLACK = 0x2,
  STRETCH_KEY_BATTERY_BAR = 0x3,
  STRETCH_KEY_VIBRATE_BT_DIS = 0x4,
  STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY = 0x5
};

typedef struct {
  bool is_24hour;
  bool blink_dots;
  bool text_color_black;
  bool battery_bar;
  bool vibrate_bt_dis;
  bool vibrate_bt_dis_when_activity;
} StretchSettings;

typedef struct {
    Window *window;
    BitmapLayer *hour_tens_image_layer;
    BitmapLayer *hour_ones_image_layer;
    BitmapLayer *min_tens_image_layer;
    BitmapLayer *min_ones_image_layer;
    GBitmap * digit_images[10];
    GBitmap * dot_image;
    InverterLayer *inv_layer;
    BitmapLayer *top_dot_image_layer;
    BitmapLayer *bottom_dot_image_layer;
    Layer *battery_bar_layer;
    Layer *tens_digit_layer;
    Layer *remaining_digit_layer;
    int last_update_min;
    AppTimer *second_hand_timer;
    bool seen_bt_disconnected;
    int battery_charge_percent;
    PropertyAnimation *battery_bar_down_prop_animation;
    PropertyAnimation *battery_bar_up_prop_animation;
    int accel_sampling_tries;

    StretchSettings settings;
} StretchApplication;

extern StretchApplication app;