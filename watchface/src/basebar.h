#pragma once

#include "pebble.h"

void setup_basebar(Layer *window_layer);
void basebar_set_date_text(char *time_buffer);

void basebar_hide_no_bt_image();
void basebar_show_no_bt_image();

void basebar_hide_battery();
void basebar_show_battery();