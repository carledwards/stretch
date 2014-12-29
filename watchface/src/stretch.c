#include "pebble.h"
#include "string.h"
#include "stdlib.h"
#include "stretch.h"
#include "basebar.h"

#define DEBUG 0

StretchApplication app;

void accel_tap_handler(AccelAxisType axis, int32_t direction);
  
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
  GRect battery_bar_layer_frame = layer_get_frame(app.battery_bar_layer);
#if DEBUG
    APP_LOG(APP_LOG_LEVEL_INFO, "get_battery_bar_layer_frame_for_percent, per: %d, frame: (%d,%d,%d,%d)",
           percent, battery_bar_layer_frame.origin.x, battery_bar_layer_frame.origin.y, battery_bar_layer_frame.size.w, battery_bar_layer_frame.size.h
           );
#endif
  int line_length = (BATTERY_WIDTH_IN_PIXELS * percent) / 100;
  int line_start = (BATTERY_WIDTH_IN_PIXELS - line_length) / 2;
  GRect retVal = GRect(line_start, battery_bar_layer_frame.origin.y, line_length, battery_bar_layer_frame.size.h);
#if DEBUG
  APP_LOG(APP_LOG_LEVEL_INFO, "get_battery_bar_layer_frame_for_percent, per: %d, retVal: (%d,%d,%d,%d)",
         percent, retVal.origin.x, retVal.origin.y, retVal.size.w, retVal.size.h
         );
#endif
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

#if DEBUG
  GRect from_rect = layer_get_frame(app.battery_bar_layer);
  APP_LOG(APP_LOG_LEVEL_INFO, "set_battery_bay_layer_size, per: %d, from: (%d,%d,%d,%d) to: (%d,%d,%d,%d)",
         percent, from_rect.origin.x, from_rect.origin.y, from_rect.size.w, from_rect.size.h,
         to_rect.origin.x, to_rect.origin.y, to_rect.size.w, to_rect.size.h
         );
#endif

  layer_set_frame(app.battery_bar_layer, to_rect);
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

  static char *dow_buffer = "Wed ";
  dow_buffer[0] = '\0';
    
  // get the DOW
  strftime(dow_buffer, 4, "%a", t);
  dow_buffer[3] = '\0';
  basebar_set_dow_text(dow_buffer);
  
  static char *date_buffer = "12/31 ";
  date_buffer[0] = '\0';
  strcat(date_buffer, itoa(t->tm_mon + 1));

  if (bluetooth_connection_service_peek()) {
    strcat(date_buffer, "/");
  }
  else {
    strcat(date_buffer, "}");  // this is the no-bt font character
  }

  strcat(date_buffer, itoa(t->tm_mday));
  basebar_set_date_text(date_buffer);

  int hour = t->tm_hour;

  // special handling for AM/PM mode
  if (!app.settings.is_24hour) {
    if (hour > 12) {
      hour = hour - 12;
    }
    else if (hour == 0) {
      hour = 12;
    }
  }
  
  bitmap_layer_set_bitmap(app.hour_tens_image_layer, app.digit_images[hour/10]);
  bitmap_layer_set_bitmap(app.hour_ones_image_layer, app.digit_images[hour - ((hour/10)*10)]);
  bitmap_layer_set_bitmap(app.min_tens_image_layer, app.digit_images[t->tm_min/10]);
  bitmap_layer_set_bitmap(app.min_ones_image_layer, app.digit_images[t->tm_min - ((t->tm_min/10)*10)]);
  
  if (hour < 10) {
    layer_set_frame(app.tens_digit_layer, THREE_DIGITS_HOUR_TENS_LAYER_GRECT);
    layer_set_frame(app.remaining_digit_layer, THREE_DIGITS_REMAINING_LAYER_GRECT);
  }
  else {
    layer_set_frame(app.tens_digit_layer, FOUR_DIGITS_HOUR_TENS_LAYER_GRECT);
    layer_set_frame(app.remaining_digit_layer, FOUR_DIGITS_REMAINING_LAYER_GRECT);
  }
}

static void show_blink_dots(void) {
  layer_set_hidden(bitmap_layer_get_layer(app.top_dot_image_layer), false);
  layer_set_hidden(bitmap_layer_get_layer(app.bottom_dot_image_layer), false);
}

static void blink_timer_callback(void *data) {
  // hide the dots
  if (app.settings.blink_dots) {
    layer_set_hidden(bitmap_layer_get_layer(app.top_dot_image_layer), true);
    layer_set_hidden(bitmap_layer_get_layer(app.bottom_dot_image_layer), true);
  }
  app.second_hand_timer = (AppTimer *)0;
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

#if DEBUG  
  APP_LOG(APP_LOG_LEVEL_INFO, "%d %d %d %d %d %d", x_low, x_high, y_low, y_high, z_low, z_high);
#endif
    
  if (x_high - x_low > 100 || y_high - y_low > 100 || z_high - z_low > 100) {
    if (app.seen_bt_disconnected && app.settings.vibrate_bt_dis && app.settings.vibrate_bt_dis_when_activity) {
      notify_disconnect();
    }
    accel_data_service_unsubscribe();
  }
  else {
    // stop the sampling if the watch isn't moving
    app.accel_sampling_tries++;
    if (app.accel_sampling_tries > 10) {
      accel_data_service_unsubscribe();
    }
  }
}

static void bluetooth_connection_handler(bool is_connected) {
  update_time();
  if (!app.settings.vibrate_bt_dis) {
    return;
  }
  if (is_connected) {
    app.seen_bt_disconnected = false;
    return;
  }
  if (!app.seen_bt_disconnected) {
    app.seen_bt_disconnected = true;
    
    if (app.settings.vibrate_bt_dis_when_activity) {
      app.accel_sampling_tries = 0;
      accel_data_service_subscribe(12, accel_handler);
      accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
    }
    else {
      notify_disconnect();
    }
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
#if DEBUG
  APP_LOG(APP_LOG_LEVEL_INFO, "tick_handler() called");
#endif
  if (app.last_update_min != tick_time->tm_min) {
    update_time();
    app.last_update_min = tick_time->tm_min;
  }
  show_blink_dots();
  if (app.settings.blink_dots == true) {
    app.second_hand_timer = app_timer_register(500, blink_timer_callback, (void *)0);
  }
}

static void configure_tick_handler() {
  tick_timer_service_subscribe(app.settings.blink_dots ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static GRect get_battery_bar_full_frame(void) {
  return GRect(2, 135, BATTERY_WIDTH_IN_PIXELS, 3); 
}

static void battery_bar_up_animation_stopped(Animation *animation, bool finished, void *data) {
  battery_state_service_subscribe(&handle_battery_event);
  basebar_hide_battery();
  accel_tap_service_subscribe(accel_tap_handler);
}

static void battery_bar_down_animation_stopped(Animation *animation, bool finished, void *data) {

  // don't continue if the battery bar is disabled
  if (app.settings.battery_bar == false) {
    basebar_hide_battery();
    return;
  }
  GRect to_rect = get_battery_bar_layer_frame_for_percent(battery_state_service_peek().charge_percent);
  app.battery_bar_up_prop_animation = property_animation_create_layer_frame(app.battery_bar_layer, NULL, &to_rect);
  animation_set_duration((Animation *)app.battery_bar_up_prop_animation, 1200);
  animation_set_handlers((Animation*) app.battery_bar_up_prop_animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler) battery_bar_up_animation_stopped,
  }, NULL /* callback data */);
  animation_schedule((Animation*) app.battery_bar_up_prop_animation);
}

static void start_battery_bar_animation(void) {
  basebar_show_battery();

  // disable any updates to the battery state event listeners
  battery_state_service_unsubscribe();

  // start the battery bar animation from full to 0
  GRect to_rect = get_battery_bar_layer_frame_for_percent(0);
  app.battery_bar_down_prop_animation = property_animation_create_layer_frame(app.battery_bar_layer, NULL, &to_rect);
  animation_set_duration((Animation *)app.battery_bar_down_prop_animation, 1200);
  animation_set_handlers((Animation*) app.battery_bar_down_prop_animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler) battery_bar_down_animation_stopped,
  }, NULL /* callback data */);
  animation_schedule((Animation*) app.battery_bar_down_prop_animation);
}

void basebar_hide_battery_timer_cb(void *data) {
  basebar_hide_battery();
}

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
#if DEBUG
  APP_LOG(APP_LOG_LEVEL_INFO, "accel_tap_handler");
#endif
  static time_t last_shake_time = 0;
  time_t now = time(NULL);
  if (now - last_shake_time <= 6) {
    basebar_show_battery();
    app_timer_register(3000, basebar_hide_battery_timer_cb, (void *)0);
  }
  last_shake_time = now;
}

static void set_blink_dots(bool new_value) {
  if (new_value == app.settings.blink_dots) {
    return;
  }
  app.settings.blink_dots = new_value;
  persist_write_bool(STRETCH_KEY_BLINK_DOTS, new_value);
  
  if (new_value == false) {
    // force the dots to turn back on (as they could be in the off state)
    show_blink_dots();
  }
  configure_tick_handler();
}

static void set_is_24hour(bool new_value) {
  if (new_value == app.settings.is_24hour) {
    return;
  }
  app.settings.is_24hour = new_value;
  persist_write_bool(STRETCH_KEY_IS_24HOUR, new_value);
  update_time();
}

static void set_text_color_black(bool new_value) {
  if (new_value == app.settings.text_color_black) {
    return;
  }
  app.settings.text_color_black = new_value;
  persist_write_bool(STRETCH_KEY_TEXT_COLOR_BLACK, new_value);
  layer_set_hidden((Layer *)app.inv_layer, new_value);
  update_time();
}

static void set_battery_bar(bool new_value) {
  if (new_value == app.settings.battery_bar) {
    return;
  }
  app.settings.battery_bar = new_value;
  persist_write_bool(STRETCH_KEY_BATTERY_BAR, new_value);
  if (new_value) {
    battery_bar_down_animation_stopped(NULL, true, NULL);
  }
  else {
    start_battery_bar_animation();
  }
}

static void set_vibrate_bt_dis(bool new_value) {
  if (new_value == app.settings.vibrate_bt_dis) {
    return;
  }
  app.settings.vibrate_bt_dis = new_value;
  persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS, new_value);
}

static void set_vibrate_bt_dis_when_activity(bool new_value) {
  if (new_value == app.settings.vibrate_bt_dis_when_activity) {
    return;
  }
  app.settings.vibrate_bt_dis_when_activity = new_value;
  persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY, new_value);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
#if DEBUG
  APP_LOG(APP_LOG_LEVEL_INFO, "in_received_handler() called");
#endif
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
  
  dict_write_uint8(iter, STRETCH_KEY_BLINK_DOTS, app.settings.blink_dots == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_IS_24HOUR, app.settings.is_24hour == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_TEXT_COLOR_BLACK, app.settings.text_color_black == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_BATTERY_BAR, app.settings.battery_bar == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_VIBRATE_BT_DIS, app.settings.vibrate_bt_dis == true ? 1 : 0);
  dict_write_uint8(iter, STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY, app.settings.vibrate_bt_dis_when_activity == true ? 1 : 0);
  dict_write_end(iter);
  app_message_outbox_send();
}

void start_battery_bar_animation_timer_cb(void *data) {
  start_battery_bar_animation();
}

static void window_load(Window *window) {
  app.digit_images[0] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_0);
  app.digit_images[1] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_1);
  app.digit_images[2] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_2);
  app.digit_images[3] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_3);
  app.digit_images[4] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_4);
  app.digit_images[5] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_5);
  app.digit_images[6] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_6);
  app.digit_images[7] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_7);
  app.digit_images[8] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_8);
  app.digit_images[9] = gbitmap_create_with_resource(RESOURCE_ID_LONG_DIGIT_9);
  app.dot_image = gbitmap_create_with_resource(RESOURCE_ID_DOT);
  
  Layer *window_layer = window_get_root_layer(window);

  // hour ten's digit
  app.tens_digit_layer = layer_create(FOUR_DIGITS_HOUR_TENS_LAYER_GRECT);
  app.hour_tens_image_layer = bitmap_layer_create(GRect(0, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(app.tens_digit_layer, bitmap_layer_get_layer(app.hour_tens_image_layer));
  layer_add_child(window_layer, app.tens_digit_layer);

  // remaining digit layer
  app.remaining_digit_layer = layer_create(FOUR_DIGITS_REMAINING_LAYER_GRECT);
  
  // hour one's digit
  app.hour_ones_image_layer = bitmap_layer_create(GRect(0, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(app.remaining_digit_layer, bitmap_layer_get_layer(app.hour_ones_image_layer));

  // top part of the time colon
  app.top_dot_image_layer = bitmap_layer_create(GRect(DIGIT_WIDTH+10, 42, COLON_WIDTH, COLON_HEIGHT));
  bitmap_layer_set_bitmap(app.top_dot_image_layer, app.dot_image);
  layer_add_child(app.remaining_digit_layer, bitmap_layer_get_layer(app.top_dot_image_layer));

  // bottom part of the time colon
  app.bottom_dot_image_layer = bitmap_layer_create(GRect(DIGIT_WIDTH+10, 77, COLON_WIDTH, COLON_HEIGHT));
  bitmap_layer_set_bitmap(app.bottom_dot_image_layer, app.dot_image);
  layer_add_child(app.remaining_digit_layer, bitmap_layer_get_layer(app.bottom_dot_image_layer));

  // minute ten's digit
  app.min_tens_image_layer = bitmap_layer_create(GRect(DIGIT_WIDTH+SPACE_BETWEEN_DIGIT+COLON_PADDING+COLON_WIDTH, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(app.remaining_digit_layer, bitmap_layer_get_layer(app.min_tens_image_layer));

  // minute one's digit
  app.min_ones_image_layer = bitmap_layer_create(GRect(2*DIGIT_WIDTH+2*SPACE_BETWEEN_DIGIT+COLON_PADDING+COLON_WIDTH, 0, DIGIT_WIDTH, DIGIT_HEIGHT));
  layer_add_child(app.remaining_digit_layer, bitmap_layer_get_layer(app.min_ones_image_layer));

  // add on the remainging digit layer
  layer_add_child(window_layer, app.remaining_digit_layer);

  basebar_setup(window_layer);

  // battery bar
  app.battery_bar_layer = layer_create(get_battery_bar_full_frame());
  layer_set_update_proc(app.battery_bar_layer, &battery_bar_layer_update_callback);
  layer_add_child(window_layer, app.battery_bar_layer);
  
  //Inverter layer
  app.inv_layer = inverter_layer_create(layer_get_bounds(window_layer));
  layer_add_child(window_get_root_layer(window), (Layer*) app.inv_layer);
  if (app.settings.text_color_black) {
    layer_set_hidden((Layer *)app.inv_layer, true);
  }

  configure_tick_handler();

  // listen for bluetooth connection events
  bluetooth_connection_service_subscribe(bluetooth_connection_handler);

  // force an update
  update_time();

  app_timer_register(500, start_battery_bar_animation_timer_cb, (void *)0);
}

static void window_unload(Window *window) {
  if (app.second_hand_timer) {
    app_timer_cancel(app.second_hand_timer);
  }
  basebar_teardown();
  property_animation_destroy(app.battery_bar_down_prop_animation);
  property_animation_destroy(app.battery_bar_up_prop_animation);
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  bitmap_layer_destroy(app.min_ones_image_layer);
  bitmap_layer_destroy(app.min_tens_image_layer);
  bitmap_layer_destroy(app.hour_ones_image_layer);
  bitmap_layer_destroy(app.hour_tens_image_layer);
  bitmap_layer_destroy(app.bottom_dot_image_layer);
  bitmap_layer_destroy(app.top_dot_image_layer);
  inverter_layer_destroy(app.inv_layer);
  layer_destroy(app.battery_bar_layer);
  layer_destroy(app.tens_digit_layer);
  layer_destroy(app.remaining_digit_layer);
  gbitmap_destroy(app.digit_images[0]);
  gbitmap_destroy(app.digit_images[1]);
  gbitmap_destroy(app.digit_images[2]);
  gbitmap_destroy(app.digit_images[3]);
  gbitmap_destroy(app.digit_images[4]);
  gbitmap_destroy(app.digit_images[5]);
  gbitmap_destroy(app.digit_images[6]);
  gbitmap_destroy(app.digit_images[7]);
  gbitmap_destroy(app.digit_images[8]);
  gbitmap_destroy(app.digit_images[9]);
  gbitmap_destroy(app.dot_image);
}

static void load_config(void) {
  if (!persist_exists(STRETCH_KEY_BLINK_DOTS)) {
    persist_write_bool(STRETCH_KEY_BLINK_DOTS, false);
  }
  app.settings.blink_dots = persist_read_bool(STRETCH_KEY_BLINK_DOTS);

  if (!persist_exists(STRETCH_KEY_IS_24HOUR)) {
    persist_write_bool(STRETCH_KEY_IS_24HOUR, true);
  }
  app.settings.is_24hour = persist_read_bool(STRETCH_KEY_IS_24HOUR);

  if (!persist_exists(STRETCH_KEY_TEXT_COLOR_BLACK)) {
    persist_write_bool(STRETCH_KEY_TEXT_COLOR_BLACK, false);
  }
  app.settings.text_color_black = persist_read_bool(STRETCH_KEY_TEXT_COLOR_BLACK);

  if (!persist_exists(STRETCH_KEY_BATTERY_BAR)) {
    persist_write_bool(STRETCH_KEY_BATTERY_BAR, true);
  }
  app.settings.battery_bar = persist_read_bool(STRETCH_KEY_BATTERY_BAR);

  if (!persist_exists(STRETCH_KEY_VIBRATE_BT_DIS)) {
    persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS, false);
  }
  app.settings.vibrate_bt_dis = persist_read_bool(STRETCH_KEY_VIBRATE_BT_DIS);

  if (!persist_exists(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY)) {
    persist_write_bool(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY, true);
  }
  app.settings.vibrate_bt_dis_when_activity = persist_read_bool(STRETCH_KEY_VIBRATE_BT_DIS_WHEN_ACTIVITY);
}

void settings_init(void) {
  app.settings.is_24hour = true;
  app.settings.blink_dots = true;
  app.settings.text_color_black = false;
  app.settings.battery_bar = true;
  app.settings.vibrate_bt_dis = false;
  app.settings.vibrate_bt_dis_when_activity = true;
}

static void init(void) {
  settings_init();

  app.last_update_min = 99;
  app.second_hand_timer = (AppTimer *)0;
  app.seen_bt_disconnected = false;
  app.accel_sampling_tries = 0;

  app.window = window_create();
  window_set_window_handlers(app.window, (WindowHandlers) {
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
  window_stack_push(app.window, animated);
}

static void deinit(void) {
  app_message_deregister_callbacks();
  window_destroy(app.window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
