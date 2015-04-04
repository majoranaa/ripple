/*
 * main.c
 * Constructs a Window housing an output TextLayer to show data from 
 * either modes of operation of the accelerometer.
 */

#include <pebble.h>

#define NUM_SAMPLES 3
#define ACCEL_STEP_MS 50

static Window *s_main_window;
static TextLayer *s_output_layer;

/*
  static void data_handler(AccelData *data, uint32_t num_samples) {
  // Long lived buffer
  static char s_buffer[128];

  // Compose string of all data
  snprintf(s_buffer, sizeof(s_buffer), 
  "N X,Y,Z\n0 %d,%d,%d\n1 %d,%d,%d\n2 %d,%d,%d", 
  data[0].x, data[0].y, data[0].z, 
  data[1].x, data[1].y, data[1].z, 
  data[2].x, data[2].y, data[2].z
  );

  //Show the data
  text_layer_set_text(s_output_layer, s_buffer);
  }
*/

static void timer_callback(void *data) {
  // Long lived buffer
  static char s_buffer[128];

  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);

  // Compose string of all data
  snprintf(s_buffer, sizeof(s_buffer), 
	   "N X,Y,Z\n%d,%d,%d\n",
	   data[0].x, data[0].y, data[0].z
	   );
  
  //Show the data
  text_layer_set_text(s_output_layer, s_buffer);
  
}
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create output TextLayer
  s_output_layer = text_layer_create(GRect(5, 0, window_bounds.size.w - 10, window_bounds.size.h));
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer, "No data yet.");
  text_layer_set_overflow_mode(s_output_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer));
}

static void main_window_unload(Window *window) {
  // Destroy output TextLayer
  text_layer_destroy(s_output_layer);
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
  //accel_data_service_subscribe(NUM_SAMPLES, data_handler);
  accel_data_service_subscribe(0,NULL);
  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
  
  // Choose update rate
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
}

static void deinit() {
  // Destroy main Window
  window_destroy(s_main_window);

  accel_data_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
