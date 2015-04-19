#include "stretch.h"
#include "basebar.h"

#define CUSTOM_FONT_ID       RESOURCE_ID_PM_32

Layer *date_layer;
TextLayer *dow_label;
TextLayer *date_label;
TextLayer *battery_label;
Layer *clip_layer;
GFont s_digital_font;

PropertyAnimation *batt_animation = NULL;
PropertyAnimation *date_animation = NULL;

void basebar_setup(Layer *window_layer) {
  // load our custom font for the date/battery 
  s_digital_font = fonts_load_custom_font(resource_get_handle(CUSTOM_FONT_ID));

  //clip layer
  clip_layer = layer_create(GRect(0, 140, 144, 34));
  layer_add_child(window_layer, clip_layer);

  // date layer (contains the date text labels)
  date_layer = layer_create(GRect(0, -40, 144, 34));
  layer_add_child(clip_layer, date_layer);
  

  // dow label
  dow_label = text_layer_create(GRect(6, 0, 50, 34));

#ifdef PBL_COLOR
  text_layer_set_text_color(dow_label, GColorCyan);
#else
  text_layer_set_text_color(dow_label, GColorWhite);
#endif
  text_layer_set_background_color(dow_label, GColorBlack);
  text_layer_set_font(dow_label, s_digital_font);
  text_layer_set_text_alignment(dow_label, GTextAlignmentLeft);
  layer_add_child(date_layer, text_layer_get_layer(dow_label));
  
  // date label
  date_label = text_layer_create(GRect(72, 0, 70, 34));
#ifdef PBL_COLOR
  text_layer_set_text_color(date_label, GColorCyan);
#else
  text_layer_set_text_color(date_label, GColorWhite);
#endif
  text_layer_set_background_color(date_label, GColorBlack);
  text_layer_set_font(date_label, s_digital_font);
  text_layer_set_text_alignment(date_label, GTextAlignmentRight);
  layer_add_child(date_layer, text_layer_get_layer(date_label));

  // battery label
  battery_label = text_layer_create(GRect(0, -6, 144, 34));
#ifdef PBL_COLOR
  text_layer_set_text_color(battery_label, GColorCyan);
#else
  text_layer_set_text_color(battery_label, GColorWhite);
#endif
  text_layer_set_background_color(battery_label, GColorBlack);
  text_layer_set_font(battery_label, s_digital_font);
  text_layer_set_text_alignment(battery_label, GTextAlignmentCenter);
  layer_add_child(clip_layer, text_layer_get_layer(battery_label));
}

void destroy_animations(void) {
  if (batt_animation != NULL) {
    property_animation_destroy(batt_animation);
    batt_animation = NULL;
  }
  if (date_animation != NULL) {
    property_animation_destroy(date_animation);
    date_animation = NULL;
  }
}

void basebar_teardown() {
  text_layer_destroy(dow_label);
  text_layer_destroy(date_label);
  text_layer_destroy(battery_label);
  layer_destroy(date_layer);
  layer_destroy(clip_layer);
  fonts_unload_custom_font(s_digital_font);
  destroy_animations();
}

void basebar_set_dow_text(char *buffer) {
  text_layer_set_text(dow_label, buffer);
}

void basebar_set_date_text(char *buffer) {
  text_layer_set_text(date_label, buffer);
}

void basebar_hide_battery() {
  destroy_animations();
  GRect battery_label_to_frame = GRect(0, 28, 144, 34);
  batt_animation = property_animation_create_layer_frame(text_layer_get_layer(battery_label), NULL, &battery_label_to_frame);
  animation_set_curve((property_animation_get_animation(batt_animation)), AnimationCurveLinear);
  animation_set_duration((Animation *)batt_animation, 700);
  animation_schedule((property_animation_get_animation(batt_animation)));
  GRect date_layer_to_frame = GRect(0, -6, 144, 34);
  date_animation = property_animation_create_layer_frame(date_layer, NULL, &date_layer_to_frame);
  animation_set_curve((property_animation_get_animation(date_animation)), AnimationCurveLinear);
  animation_set_duration((Animation *)date_animation, 700);
  animation_schedule((property_animation_get_animation(date_animation)));
}

void basebar_show_battery() {
  // set the value of the battery percentage
  static char battery_label_buffer[9] = "BAT 100%";
  battery_label_buffer[0] = '\0';
  snprintf(battery_label_buffer, 9, "BAT %d%%", battery_state_service_peek().charge_percent);
  text_layer_set_text(battery_label, battery_label_buffer);  

  destroy_animations();
  GRect battery_label_to_frame = GRect(0, -6, 144, 34);
  batt_animation = property_animation_create_layer_frame(text_layer_get_layer(battery_label), NULL, &battery_label_to_frame);
  animation_set_curve((property_animation_get_animation(batt_animation)), AnimationCurveLinear);
  animation_set_duration((Animation *)batt_animation, 700);
  animation_schedule((property_animation_get_animation(batt_animation)));
  GRect date_layer_to_frame = GRect(0, -40, 144, 34);
  date_animation = property_animation_create_layer_frame(date_layer, NULL, &date_layer_to_frame);
  animation_set_curve((property_animation_get_animation(date_animation)), AnimationCurveLinear);
  animation_set_duration((Animation *)date_animation, 700);
  animation_schedule((property_animation_get_animation(date_animation)));
}