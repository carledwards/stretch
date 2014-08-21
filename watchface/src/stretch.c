#include "pebble.h"
#include "string.h"
#include "stdlib.h"
#define CUSTOM_FONT_ID       RESOURCE_ID_PM_32
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
  
// app messages
enum {
  STRETCH_KEY_BLINK_DOTS = 0x0,
  STRETCH_KEY_IS_24HOUR = 0x1,
  STRETCH_KEY_TEXT_COLOR_BLACK = 0x2,
  STRETCH_KEY_BATTERY_BAR = 0x3,
  STRETCH_KEY_VIBRATE_BT_DIS = 0x4,
  STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY = 0x5
};

// config
bool config_is_24hour = true;
bool config_blink_dots = true;
bool config_text_color_black = false;
bool config_battery_bar = true;
bool config_vibrate_bt_dis = false;
bool config_vibrate_bt_dis_when_activity = true;

Window *window;
BitmapLayer *hour_tens_image_layer;
BitmapLayer *hour_ones_image_layer;
BitmapLayer *min_tens_image_layer;
BitmapLayer *min_ones_image_layer;
GBitmap * digit_images[10];
GBitmap * dot_image;
GBitmap * no_bt_image;
TextLayer *date_label;
TextLayer *battery_label;
GFont s_digital_font;
InverterLayer *inv_layer;
BitmapLayer *top_dot_image_layer;
BitmapLayer *bottom_dot_image_layer;
BitmapLayer *no_bt_image_layer;
Layer *battery_bar_layer;
Layer *tens_digit_layer;
Layer *remaining_digit_layer;
int last_update_min = 99;
AppTimer *second_hand_timer = (AppTimer *)0;
bool seen_bt_disconnected = false;
static PropertyAnimation *battery_bar_down_prop_animation;
static PropertyAnimation *battery_bar_up_prop_animation;
int accel_sampling_tries = 0;

char *itoa(int num)
{
	static char buff[20] = {};
	int i = 0, temp_num = num, length = 0;
	char *string = buff;
	
	if(num >= 0) {
		// count how many characters in the number
		while(temp_num) {
			temp_num /= 10;
			length++;
		}
		
		// assign the number to the buffer starting at the end of the 
		// number and going to the begining since we are doing the
		// integer to character conversion on the last number in the
		// sequence
		for(i = 0; i < length; i++) {
		 	buff[(length-1)-i] = '0' + (num % 10);
			num /= 10;
		}
		buff[i] = '\0';
	}
	else
		return "o_O"; // hmph face

	return string;
}

GRect get_battery_bar_layer_frame_for_percent(int percent) {
  GRect battery_bar_layer_frame = layer_get_frame(battery_bar_layer);
  //APP_LOG(APP_LOG_LEVEL_INFO, "get_battery_bar_layer_frame_for_percent, per: %d, frame: (%d,%d,%d,%d)",
  //       percent, battery_bar_layer_frame.origin.x, battery_bar_layer_frame.origin.y, battery_bar_layer_frame.size.w, battery_bar_layer_frame.size.h
  //       );
  int line_length = (BATTERY_WIDTH_IN_PIXELS * percent) / 100;
  int line_start = (BATTERY_WIDTH_IN_PIXELS - line_length) / 2;
  GRect retVal = GRect(line_start, battery_bar_layer_frame.origin.y, line_length, battery_bar_layer_frame.size.h);
  //APP_LOG(APP_LOG_LEVEL_INFO, "get_battery_bar_layer_frame_for_percent, per: %d, retVal: (%d,%d,%d,%d)",
  //       percent, retVal.origin.x, retVal.origin.y, retVal.size.w, retVal.size.h
  //       );
  return retVal;
}

static void battery_bar_layer_update_callback(Layer *me, GContext* ctx ) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "battery_bar_layer_update_callback() called");

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);

  GRect me_bounds = layer_get_bounds(me);
  graphics_fill_rect(ctx, GRect(me_bounds.origin.x, me_bounds.origin.y, me_bounds.size.w, me_bounds.size.h), 4, GCornerNone);
}

static void set_battery_bar_layer_percent(int percent) {
  GRect to_rect = get_battery_bar_layer_frame_for_percent(percent);

  //GRect from_rect = layer_get_frame(battery_bar_layer);
  //APP_LOG(APP_LOG_LEVEL_INFO, "set_battery_bay_layer_size, per: %d, from: (%d,%d,%d,%d) to: (%d,%d,%d,%d)",
  //       percent, from_rect.origin.x, from_rect.origin.y, from_rect.size.w, from_rect.size.h,
  //       to_rect.origin.x, to_rect.origin.y, to_rect.size.w, to_rect.size.h
  //       );

  layer_set_frame(battery_bar_layer, to_rect);
}

static void handle_battery_event(BatteryChargeState charge_state) {
  set_battery_bar_layer_percent(charge_state.charge_percent);
}

static void update_time() {
  //APP_LOG(APP_LOG_LEVEL_INFO, "update_time() called");
  struct tm *t;
  time_t temp;
  temp = time(NULL);
  t = localtime(&temp);

  static char *time_buffer = "012345678901";
  time_buffer[0] = '\0';
    
  if (bluetooth_connection_service_peek()) {
    // get the DOW
    strftime(time_buffer, 4, "%a", t);
    time_buffer[3] = '\0';

    if (t->tm_mon < 9 && t->tm_mday < 10) {
      strcat(time_buffer, "    ");
    }
    else if (t->tm_mon >= 9 && t->tm_mday >= 10) {
      strcat(time_buffer, "   ");
    }
    else {
      strcat(time_buffer, "  ");
    }
    layer_set_hidden(bitmap_layer_get_layer(no_bt_image_layer), true);  
  }
  else {
    strcat(time_buffer, "    ");
    layer_set_hidden(bitmap_layer_get_layer(no_bt_image_layer), false);  
  }
  strcat(time_buffer, itoa(t->tm_mon + 1));
  strcat(time_buffer, "/");
  strcat(time_buffer, itoa(t->tm_mday));
  
  text_layer_set_text(date_label, time_buffer);

  int hour = t->tm_hour;
  if (!config_is_24hour && hour > 12) {
    hour = hour - 12;
  }
  
  bitmap_layer_set_bitmap(hour_tens_image_layer, digit_images[hour/10]);
  bitmap_layer_set_bitmap(hour_ones_image_layer, digit_images[hour - ((hour/10)*10)]);
  bitmap_layer_set_bitmap(min_tens_image_layer, digit_images[t->tm_min/10]);
  bitmap_layer_set_bitmap(min_ones_image_layer, digit_images[t->tm_min - ((t->tm_min/10)*10)]);
  
  if (hour < 10) {
    layer_set_frame(tens_digit_layer, THREE_DIGITS_HOUR_TENS_LAYER_GRECT);
    layer_set_frame(remaining_digit_layer, THREE_DIGITS_REMAINING_LAYER_GRECT);
  }
  else {
    layer_set_frame(tens_digit_layer, FOUR_DIGITS_HOUR_TENS_LAYER_GRECT);
    layer_set_frame(remaining_digit_layer, FOUR_DIGITS_REMAINING_LAYER_GRECT);
  }
}

static void show_blink_dots(void) {
  layer_set_hidden(bitmap_layer_get_layer(top_dot_image_layer), false);
  layer_set_hidden(bitmap_layer_get_layer(bottom_dot_image_layer), false);
}

static void blink_timer_callback(void *data) {
  // hide the dots
  if (config_blink_dots) {
    layer_set_hidden(bitmap_layer_get_layer(top_dot_image_layer), true);
    layer_set_hidden(bitmap_layer_get_layer(bottom_dot_image_layer), true);
  }
  second_hand_timer = (AppTimer *)0;
}

static void notify_disconnect(void) {
  vibes_short_pulse(); 
}

static void accel_handler(AccelData *data, uint32_t num_samples) {
  int16_t x_low = data[1].x;
  int16_t x_high = data[1].x;
  int16_t y_low = data[1].y;
  int16_t y_high = data[1].y;
  int16_t z_low = data[1].z;
  int16_t z_high = data[1].z;
  
  for (int i = 2; i < (int)num_samples; i++) {
    x_low = data[i].x < x_low ? data[i].x : x_low;
    x_high = data[i].x > x_high ? data[i].x : x_high;
    y_low = data[i].y < y_low ? data[i].y : y_low;
    y_high = data[i].y > y_high ? data[i].y : y_high;
    z_low = data[i].z < z_low ? data[i].z : z_low;
    z_high = data[i].z > z_high ? data[i].z : z_high;
  }
  
//  APP_LOG(APP_LOG_LEVEL_INFO, "%d %d %d %d %d %d", x_low, x_high, y_low, y_high, z_low, z_high);
    
  if (x_high - x_low > 100 || y_high - y_low > 100 || z_high - z_low > 100) {
    if (seen_bt_disconnected && config_vibrate_bt_dis && config_vibrate_bt_dis_when_activity) {
      notify_disconnect();
    }
    accel_data_service_unsubscribe();
  }
  else {
    // stop the sampling if the watch isn't moving
    accel_sampling_tries++;
    if (accel_sampling_tries > 10) {
      accel_data_service_unsubscribe();
    }
  }
}

static void bluetooth_connection_handler(bool is_connected) {
  update_time();
  if (!config_vibrate_bt_dis) {
    return;
  }
  if (is_connected) {
    seen_bt_disconnected = false;
    return;
  }
  if (!seen_bt_disconnected) {
    seen_bt_disconnected = true;
    
    if (config_vibrate_bt_dis_when_activity) {
      accel_sampling_tries = 0;
      accel_data_service_subscribe(12, accel_handler);
      accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
    }
    else {
      notify_disconnect();
    }
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "tick_handler() called");
  if (last_update_min != tick_time->tm_min) {
    update_time();
    last_update_min = tick_time->tm_min;
  }
  show_blink_dots();
  if (config_blink_dots == true) {
    second_hand_timer = app_timer_register(500, blink_timer_callback, (void *)0);
  }
}

static void configure_tick_handler() {
  tick_timer_service_subscribe(config_blink_dots ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static GRect get_battery_bar_full_frame(void) {
  return GRect(2, 135, BATTERY_WIDTH_IN_PIXELS, 3); 
}

static void battery_bar_up_animation_stopped(Animation *animation, bool finished, void *data) {
  battery_state_service_subscribe(&handle_battery_event);
}

static void battery_bar_down_animation_stopped(Animation *animation, bool finished, void *data) {
  // don't continue if the battery bar is disabled
  if (config_battery_bar == false) {
    return;
  }
  GRect to_rect = get_battery_bar_layer_frame_for_percent(battery_state_service_peek().charge_percent);
  battery_bar_up_prop_animation = property_animation_create_layer_frame(battery_bar_layer, NULL, &to_rect);
  animation_set_duration((Animation *)battery_bar_up_prop_animation, 1200);
  animation_set_handlers((Animation*) battery_bar_up_prop_animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler) battery_bar_up_animation_stopped,
  }, NULL /* callback data */);
  animation_schedule((Animation*) battery_bar_up_prop_animation);
}

static void start_battery_bar_animation(void) {
  // disable any updates to the battery state event listeners
  battery_state_service_unsubscribe();

  // start the battery bar animation from full to 0
  GRect to_rect = get_battery_bar_layer_frame_for_percent(0);
  battery_bar_down_prop_animation = property_animation_create_layer_frame(battery_bar_layer, NULL, &to_rect);
  animation_set_duration((Animation *)battery_bar_down_prop_animation, 1200);
  animation_set_handlers((Animation*) battery_bar_down_prop_animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler) battery_bar_down_animation_stopped,
  }, NULL /* callback data */);
  animation_schedule((Animation*) battery_bar_down_prop_animation);
}

static void set_blink_dots(bool new_value) {
  if (new_value == config_blink_dots) {
    return;
  }
  config_blink_dots = new_value;
  persist_write_bool(STRETCH_KEY_BLINK_DOTS, new_value);
  
  if (new_value == false) {
    // force the dots to turn back on (as they could be in the off state)
    show_blink_dots();
  }
  configure_tick_handler();
}

static void set_is_24hour(bool new_value) {
  if (new_value == config_is_24hour) {
    return;
  }
  config_is_24hour = new_value;
  persist_write_bool(STRETCH_KEY_IS_24HOUR, new_value);
  update_time();
}

static void set_text_color_black(bool new_value) {
  if (new_value == config_text_color_black) {
    return;
  }
  config_text_color_black = new_value;
  persist_write_bool(STRETCH_KEY_TEXT_COLOR_BLACK, new_value);
  layer_set_hidden((Layer *)inv_layer, new_value);
  update_time();
}

static void set_battery_bar(bool new_value) {
  if (new_value == config_battery_bar) {
    return;
  }
  config_battery_bar = new_value;
  persist_write_bool(STRETCH_KEY_BATTERY_BAR, new_value);
  if (new_value) {
    battery_bar_down_animation_stopped(NULL, true, NULL);
  }
  else {
    start_battery_bar_animation();
  }
}

static void set_vibrate_bt_dis(bool new_value) {
  if (new_value == config_vibrate_bt_dis) {
    return;
  }
  config_vibrate_bt_dis = new_value;
  persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS, new_value);
  // TODO
}

static void set_vibrate_bt_dis_when_activity(bool new_value) {
  if (new_value == config_vibrate_bt_dis_when_activity) {
    return;
  }
  config_vibrate_bt_dis_when_activity = new_value;
  persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY, new_value);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "in_received_handler() called");
	Tuple* tuple = dict_read_first(iter);
	if(!tuple) return;
	do {
    switch(tuple->key) {
      case STRETCH_KEY_BLINK_DOTS:
        //APP_LOG(APP_LOG_LEVEL_INFO, "STRETCH_KEY_BLINK_DOTS = %d", tuple->value->int8);
        set_blink_dots(tuple->value->int8 == 1 ? true : false);
        break;
      case STRETCH_KEY_IS_24HOUR:
        //APP_LOG(APP_LOG_LEVEL_INFO, "STRETCH_KEY_IS_24HOUR = %d", tuple->value->int8);
        set_is_24hour(tuple->value->int8 == 1 ? true : false);
        break;
      case STRETCH_KEY_TEXT_COLOR_BLACK:
        //APP_LOG(APP_LOG_LEVEL_INFO, "STRETCH_KEY_TEXT_COLOR_BLACK = %d", tuple->value->int8);
        set_text_color_black(tuple->value->int8 == 1 ? true : false);
        break;
      case STRETCH_KEY_BATTERY_BAR:
        //APP_LOG(APP_LOG_LEVEL_INFO, "STRETCH_KEY_BATTERY_BAR = %d", tuple->value->int8);
        set_battery_bar(tuple->value->int8 == 1 ? true : false);
        break;
      case STRETCH_KEY_VIBRATE_BT_DIS:
        //APP_LOG(APP_LOG_LEVEL_INFO, "STRETCH_KEY_VIBRATE_BT_DIS = %d", tuple->value->int8);
        set_vibrate_bt_dis(tuple->value->int8 == 1 ? true : false);
        break;
      case STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY:
        //APP_LOG(APP_LOG_LEVEL_INFO, "STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY = %d", tuple->value->int8);
        set_vibrate_bt_dis_when_activity(tuple->value->int8 == 1 ? true : false);
        break;
    }
	} while((tuple = dict_read_next(iter)));
}

static void send_config_to_js(void) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL)
    return;
  
  dict_write_uint8(iter, STRETCH_KEY_BLINK_DOTS, config_blink_dots == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_IS_24HOUR, config_is_24hour == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_TEXT_COLOR_BLACK, config_text_color_black == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_BATTERY_BAR, config_battery_bar == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_VIBRATE_BT_DIS, config_vibrate_bt_dis == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY, config_vibrate_bt_dis_when_activity == true ? 1 : 0);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void window_load(Window *window) {
  digit_images[0] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_0);
  digit_images[1] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_1);
  digit_images[2] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_2);
  digit_images[3] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_3);
  digit_images[4] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_4);
  digit_images[5] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_5);
  digit_images[6] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_6);
  digit_images[7] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_7);
  digit_images[8] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_8);
  digit_images[9] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_9);
  dot_image = gbitmap_create_with_resource(RESOURCE_ID_DOT);
  no_bt_image = gbitmap_create_with_resource(RESOURCE_ID_NO_BT);
  
  Layer *window_layer = window_get_root_layer(window);

  // hour ten's digit
  tens_digit_layer = layer_create(FOUR_DIGITS_HOUR_TENS_LAYER_GRECT);
  hour_tens_image_layer = bitmap_layer_create(GRect(0, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(tens_digit_layer, bitmap_layer_get_layer(hour_tens_image_layer));
  layer_add_child(window_layer, tens_digit_layer);

  // remaining digit layer
  remaining_digit_layer = layer_create(FOUR_DIGITS_REMAINING_LAYER_GRECT);
  
  // hour one's digit
  hour_ones_image_layer = bitmap_layer_create(GRect(0, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(remaining_digit_layer, bitmap_layer_get_layer(hour_ones_image_layer));

  // top part of the time colon
  top_dot_image_layer = bitmap_layer_create(GRect(DIGIT_WIDTH+10, 42, COLON_WIDTH, COLON_HEIGHT));
  bitmap_layer_set_bitmap(top_dot_image_layer, dot_image);
  layer_add_child(remaining_digit_layer, bitmap_layer_get_layer(top_dot_image_layer));

  // bottom part of the time colon
  bottom_dot_image_layer = bitmap_layer_create(GRect(DIGIT_WIDTH+10, 77, COLON_WIDTH, COLON_HEIGHT));
  bitmap_layer_set_bitmap(bottom_dot_image_layer, dot_image);
  layer_add_child(remaining_digit_layer, bitmap_layer_get_layer(bottom_dot_image_layer));

  // minute ten's digit
  min_tens_image_layer = bitmap_layer_create(GRect(DIGIT_WIDTH+SPACE_BETWEEN_DIGIT+COLON_PADDING+COLON_WIDTH, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(remaining_digit_layer, bitmap_layer_get_layer(min_tens_image_layer));

  // minute one's digit
  min_ones_image_layer = bitmap_layer_create(GRect(2*DIGIT_WIDTH+2*SPACE_BETWEEN_DIGIT+COLON_PADDING+COLON_WIDTH, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(remaining_digit_layer, bitmap_layer_get_layer(min_ones_image_layer));

  // add on the remainging digit layer
  layer_add_child(window_layer, remaining_digit_layer);

  // load our custom font for the date/battery 
  s_digital_font = fonts_load_custom_font(resource_get_handle(CUSTOM_FONT_ID));

   // date label
  date_label = text_layer_create(GRect(0, 134, 144, 34));
  text_layer_set_text_color(date_label, GColorBlack);
  text_layer_set_font(date_label, s_digital_font);
  text_layer_set_text_alignment(date_label, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(date_label));

  // no bluetooth icon
  no_bt_image_layer = bitmap_layer_create(GRect(18, 141, 21, 26));
  bitmap_layer_set_bitmap(no_bt_image_layer, no_bt_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(no_bt_image_layer));
  layer_set_hidden(bitmap_layer_get_layer(no_bt_image_layer), true);  
  
  /*
  // battery label
  battery_label = text_layer_create(GRect(0, 134, 144, 34));
  text_layer_set_text_color(battery_label, GColorBlack);
  text_layer_set_font(battery_label, s_digital_font);
  text_layer_set_text_alignment(battery_label, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(battery_label));

  // set the value of the battery percentage
  static char *battery_label_buffer = "012345678901";
  battery_label_buffer[0] = '\0';
  strcat(battery_label_buffer, "BAT  ");
  strcat(battery_label_buffer, itoa(battery_state_service_peek().charge_percent));
  strcat(battery_label_buffer, "%");
  text_layer_set_text(battery_label, battery_label_buffer);
*/  
  // battery bar
  battery_bar_layer = layer_create(get_battery_bar_full_frame());
  layer_set_update_proc(battery_bar_layer, &battery_bar_layer_update_callback);
  layer_add_child(window_layer, battery_bar_layer);
  
  //Inverter layer
  inv_layer = inverter_layer_create(layer_get_bounds(window_layer));
  layer_add_child(window_get_root_layer(window), (Layer*) inv_layer);
  if (config_text_color_black) {
    layer_set_hidden((Layer *)inv_layer, true);
  }

  configure_tick_handler();

  // listen for bluetooth connection events
  bluetooth_connection_service_subscribe(bluetooth_connection_handler);

  // force an update
  update_time();

  start_battery_bar_animation();
}

static void window_unload(Window *window) {
  if (second_hand_timer) {
    app_timer_cancel(second_hand_timer);
  }
  property_animation_destroy(battery_bar_down_prop_animation);
  property_animation_destroy(battery_bar_up_prop_animation);
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  text_layer_destroy(date_label);
//  text_layer_destroy(battery_label);
  fonts_unload_custom_font(s_digital_font);
  bitmap_layer_destroy(min_ones_image_layer);
  bitmap_layer_destroy(min_tens_image_layer);
  bitmap_layer_destroy(hour_ones_image_layer);
  bitmap_layer_destroy(hour_tens_image_layer);
  bitmap_layer_destroy(bottom_dot_image_layer);
  bitmap_layer_destroy(top_dot_image_layer);
  bitmap_layer_destroy(no_bt_image_layer);
  inverter_layer_destroy(inv_layer);
  layer_destroy(battery_bar_layer);
  layer_destroy(tens_digit_layer);
  layer_destroy(remaining_digit_layer);
  gbitmap_destroy(digit_images[0]);
  gbitmap_destroy(digit_images[1]);
  gbitmap_destroy(digit_images[2]);
  gbitmap_destroy(digit_images[3]);
  gbitmap_destroy(digit_images[4]);
  gbitmap_destroy(digit_images[5]);
  gbitmap_destroy(digit_images[6]);
  gbitmap_destroy(digit_images[7]);
  gbitmap_destroy(digit_images[8]);
  gbitmap_destroy(digit_images[9]);
  gbitmap_destroy(dot_image);
  gbitmap_destroy(no_bt_image);
}

static void load_config(void) {
  if (!persist_exists(STRETCH_KEY_BLINK_DOTS)) {
    persist_write_bool(STRETCH_KEY_BLINK_DOTS, false);
  }
  config_blink_dots = persist_read_bool(STRETCH_KEY_BLINK_DOTS);

  if (!persist_exists(STRETCH_KEY_IS_24HOUR)) {
    persist_write_bool(STRETCH_KEY_IS_24HOUR, true);
  }
  config_is_24hour = persist_read_bool(STRETCH_KEY_IS_24HOUR);

  if (!persist_exists(STRETCH_KEY_TEXT_COLOR_BLACK)) {
    persist_write_bool(STRETCH_KEY_TEXT_COLOR_BLACK, false);
  }
  config_text_color_black = persist_read_bool(STRETCH_KEY_TEXT_COLOR_BLACK);

  if (!persist_exists(STRETCH_KEY_BATTERY_BAR)) {
    persist_write_bool(STRETCH_KEY_BATTERY_BAR, true);
  }
  config_battery_bar = persist_read_bool(STRETCH_KEY_BATTERY_BAR);

  if (!persist_exists(STRETCH_KEY_VIBRATE_BT_DIS)) {
    persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS, false);
  }
  config_vibrate_bt_dis = persist_read_bool(STRETCH_KEY_VIBRATE_BT_DIS);

  if (!persist_exists(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY)) {
    persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY, true);
  }
  config_vibrate_bt_dis_when_activity = persist_read_bool(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  app_message_register_inbox_received(in_received_handler);
  const uint32_t inbound_size = 128;
  const uint32_t outbound_size = 128;
  app_message_open(inbound_size, outbound_size);
  
  // load the config from persistence 
  load_config();
  
  // send the config to the javascript layer
  send_config_to_js();
    
  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  app_message_deregister_callbacks();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
