#pragma once

#include "pebble.h"

void basebar_setup(Layer *window_layer);
void basebar_teardown();

void basebar_set_date_text(char *buffer);
void basebar_set_dow_text(char *buffer);

void basebar_hide_battery();
void basebar_show_battery();