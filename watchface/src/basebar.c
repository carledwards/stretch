#include "stretch.h"
#include "basebar.h"

#define CUSTOM_FONT_ID       RESOURCE_ID_PM_32

TextLayer *date_label;
TextLayer *battery_label;
Layer *clip_layer;
BitmapLayer *no_bt_image_layer;
GBitmap * no_bt_image;
GFont s_digital_font;

PropertyAnimation *batt_animation = NULL;
PropertyAnimation *date_animation = NULL;

void basebar_setup(Layer *window_layer) {
  // load our custom font for the date/battery 
  s_digital_font = fonts_load_custom_font(resource_get_handle(CUSTOM_FONT_ID));

  no_bt_image = gbitmap_create_with_resource(RESOURCE_ID_NO_BT);
	
  //clip layer
  clip_layer = layer_create(GRect(0, 140, 144, 34));
  layer_add_child(window_layer, clip_layer);

  // date label
  date_label = text_layer_create(GRect(0, -40, 144, 34));
  text_layer_set_text_color(date_label, GColorBlack);
  text_layer_set_font(date_label, s_digital_font);
  text_layer_set_text_alignment(date_label, GTextAlignmentCenter);
  layer_add_child(clip_layer, text_layer_get_layer(date_label));

  // no BT connection icon
  no_bt_image_layer = bitmap_layer_create(GRect(18, 1, 21, 26));
  bitmap_layer_set_bitmap(no_bt_image_layer, no_bt_image);
  layer_add_child(clip_layer, bitmap_layer_get_layer(no_bt_image_layer));
  layer_set_hidden(bitmap_layer_get_layer(no_bt_image_layer), true);  
  
  // battery label
  battery_label = text_layer_create(GRect(0, -6, 144, 34));
  text_layer_set_text_color(battery_label, GColorBlack);
  text_layer_set_font(battery_label, s_digital_font);
  text_layer_set_text_alignment(battery_label, GTextAlignmentCenter);
  layer_add_child(clip_layer, text_layer_get_layer(battery_label));

  // set the value of the battery percentage
  static char battery_label_buffer[9] = "BAT 100%";
  battery_label_buffer[0] = '\0';
  snprintf(battery_label_buffer, 9, "BAT %d%%", app.battery_charge_percent);
  text_layer_set_text(battery_label, battery_label_buffer);  
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
  text_layer_destroy(date_label);
  text_layer_destroy(battery_label);
  layer_destroy(clip_layer);
  fonts_unload_custom_font(s_digital_font);
  bitmap_layer_destroy(no_bt_image_layer);
  gbitmap_destroy(no_bt_image);
  destroy_animations();
}

void basebar_set_date_text(char *time_buffer) {
  text_layer_set_text(date_label, time_buffer);
}

void basebar_hide_no_bt_image() {
  layer_set_hidden(bitmap_layer_get_layer(no_bt_image_layer), true);  
}

void basebar_show_no_bt_image() {
  layer_set_hidden(bitmap_layer_get_layer(no_bt_image_layer), false);  
}

void basebar_hide_battery() {
  destroy_animations();
  GRect battery_label_to_frame = GRect(0, 28, 144, 34);
  batt_animation = property_animation_create_layer_frame(text_layer_get_layer(battery_label), NULL, &battery_label_to_frame);
  animation_set_curve(&(batt_animation->animation), AnimationCurveLinear);
  animation_set_duration((Animation *)batt_animation, 700);
  animation_schedule(&(batt_animation->animation));
  GRect date_label_to_frame = GRect(0, -6, 144, 34);
  date_animation = property_animation_create_layer_frame(text_layer_get_layer(date_label), NULL, &date_label_to_frame);
  animation_set_curve(&(date_animation->animation), AnimationCurveLinear);
  animation_set_duration((Animation *)date_animation, 700);
  animation_schedule(&(date_animation->animation));
}

void basebar_show_battery() {
  destroy_animations();
  GRect battery_label_to_frame = GRect(0, -6, 144, 34);
  batt_animation = property_animation_create_layer_frame(text_layer_get_layer(battery_label), NULL, &battery_label_to_frame);
  animation_set_curve(&(batt_animation->animation), AnimationCurveLinear);
  animation_set_duration((Animation *)batt_animation, 700);
  animation_schedule(&(batt_animation->animation));
  GRect date_label_to_frame = GRect(0, -40, 144, 34);
  date_animation = property_animation_create_layer_frame(text_layer_get_layer(date_label), NULL, &date_label_to_frame);
  animation_set_curve(&(date_animation->animation), AnimationCurveLinear);
  animation_set_duration((Animation *)date_animation, 700);
  animation_schedule(&(date_animation->animation));
}