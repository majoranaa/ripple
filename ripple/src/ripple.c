/*
 * main.c
 * Constructs a Window housing an output TextLayer to show data from 
 * either modes of operation of the accelerometer.
 */

#include <pebble.h>

#define NUM_SAMPLES 25
//#define ACCEL_STEP_MS 50
#define KEY_DATA 0

static Window *s_main_window;
static TextLayer *s_output_layer;
static TextLayer *s_output_layer2;

static void data_handler(AccelData *data, uint32_t num_samples) {
  // Long lived buffer
  static char s_buffer[128];

  // Compose string of all data
  snprintf(s_buffer, sizeof(s_buffer), 
	   "X: %d\nY: %d\nZ: %d\n",
	   data[0].x, data[0].y, data[0].z
	   );

  uint8_t data_buffer[dict_calc_buffer_size(1, sizeof(AccelData) * NUM_SAMPLES)];
  DictionaryIterator iter;
  DictionaryIterator *iter_p = &iter;
  dict_write_begin(iter_p, data_buffer, sizeof(data_buffer));
  app_message_outbox_begin(&iter_p);
  dict_write_data(iter_p, KEY_DATA, (uint8_t *)data, sizeof(AccelData) * NUM_SAMPLES);
  app_message_outbox_send();
  dict_write_end(iter_p);

  //Show the data
  text_layer_set_text(s_output_layer, s_buffer);
}

/*
  static void timer_callback(void *data) {
  // Long lived buffer
  static char s_buffer[128];

  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);

  // Compose string of all data
  snprintf(s_buffer, sizeof(s_buffer), 
  "X: %d\nY: %d\nZ: %d\n",
  accel.x, accel.y, accel.z
  );
  
  //Show the data
  text_layer_set_text(s_output_layer, s_buffer);

  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
  }
*/

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create output TextLayer
  s_output_layer = text_layer_create(GRect(5, 0, window_bounds.size.w - 10, window_bounds.size.h));
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer, "No data yet.");
  text_layer_set_overflow_mode(s_output_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer));

  s_output_layer2 = text_layer_create(GRect(5, 75, window_bounds.size.w - 10, window_bounds.size.h - 75));
  text_layer_set_font(s_output_layer2, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer2, "Nothing going on");
  text_layer_set_overflow_mode(s_output_layer2, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer2));
}

static void main_window_unload(Window *window) {
  // Destroy output TextLayer
  text_layer_destroy(s_output_layer);
  text_layer_destroy(s_output_layer2);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message received!");
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_output_layer2, "Up pressed!");
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_output_layer2, "Select pressed!");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_output_layer2, "Down pressed!");
}

static void click_config_provider(void *context) {
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void init() {
  // Create main Window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
      .load = main_window_load,
	.unload = main_window_unload
	});
  window_stack_push(s_main_window, true);

  // Subscribe to the accelerometer data service
  accel_data_service_subscribe(NUM_SAMPLES, data_handler);
  
  //accel_data_service_subscribe(0,NULL);
  //app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());


  // Choose update rate
  // accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  APP_LOG(APP_LOG_LEVEL_INFO, "App opened!");
}

static void deinit() {
  APP_LOG(APP_LOG_LEVEL_INFO, "App closed!");

  // Destroy main Window
  window_destroy(s_main_window);

  accel_data_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
