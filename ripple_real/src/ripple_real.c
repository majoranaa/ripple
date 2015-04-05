/*
 * main.c
 * Constructs a Window housing an output TextLayer to show data from 
 * either modes of operation of the accelerometer.
 */

#include <pebble.h>

// 25 samples per second
//#define NUM_SAMPLES 25
#define ACCEL_STEP_MS 40

#define KEY_MAKE_NEW_GESTURE 0
#define KEY_NEW_GESTURE_ID 1
#define KEY_NEW_GESTURE_DATA 2
#define KEY_GESTURE 3
#define KEY_OLD_GESTURE_ID 4
#define KEY_OLD_GESTURE_DATA 5

#define MAX_REF_SIZE 20 // this is the max number of samples that can be in a reference
#define MAX_GESTURES 5
#define BUFF_SIZE 500

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} DataVec;

// window and layers
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_output_layer;

// resources
static GFont s_time_font;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

// buffer for accel data
static DataVec accel_buff[BUFF_SIZE];
static int head; // head of buffer
static int start_proc; // begin processing
static int make_gesture;
static const float alpha = 0.1;
static const float still_thresh = 1e5;
static const int count_thresh = 4;

// array of recorded gestures
static DataVec temp_ges[MAX_REF_SIZE];
static DataVec gestures[MAX_GESTURES][MAX_REF_SIZE];

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
}

/*
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
*/

static void timer_callback(void *data) {
  // Long lived buffer
  static char s_buffer[128];
  static float x_mavg;
  static float y_mavg;
  static float z_mavg;
  static float x_diff;
  static float y_diff;
  static float z_diff;
  static float still;
  static int count = 0;
  static int find_ref = 0;
  
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);

  // Compose string of all data
  snprintf(s_buffer, sizeof(s_buffer), 
	   "X: %d Y: %d Z: %d",
	   accel.x, accel.y, accel.z);
  
  //Show the data
  text_layer_set_text(s_output_layer, s_buffer);

  if (!accel.did_vibrate) {
    accel_buff[head].x = accel.x;
    accel_buff[head].y = accel.y;
    accel_buff[head].z = accel.z;
    head = (head+1)%BUFF_SIZE;
  }
  if (head >= MAX_REF_SIZE) {
    start_proc = 1;
    APP_LOG(APP_LOG_LEVEL_INFO, "Starting processing");
    x_mavg = accel_buff[head].x;
    y_mavg = accel_buff[head].y;
    z_mavg = accel_buff[head].z;
    x_diff = 0;
    y_diff = 0;
    z_diff = 0;
  }
  if (start_proc) {
    // Do dsp here
    x_mavg = x_mavg + alpha*((float)accel.x - x_mavg);
    y_mavg = y_mavg + alpha*((float)accel.y - y_mavg);
    z_mavg = z_mavg + alpha*((float)accel.z - z_mavg);
    x_diff = (float)accel.x - x_mavg;
    y_diff = (float)accel.y - y_mavg;
    z_diff = (float)accel.z - z_mavg;
    still = x_diff*x_diff + y_diff*y_diff + z_diff*z_diff;
    if (make_gesture) {
      if (!find_ref) {
	if (still < still_thresh) {
	  count++;
	  if (count >= count_thresh) {
	    count = 0;
	    find_ref = 1;
	  }
	}
      } else {
	
      }
    }
  }
  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  // Create GBitmap, then set to created BitmapLayer
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND);
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));

  // Create output TextLayer
  s_output_layer = text_layer_create(GRect(5, 0, 139, 30));
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer, "No data yet.");
  text_layer_set_overflow_mode(s_output_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer));

  // Put time
  s_time_layer = text_layer_create(GRect(5, 52, 139, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_48));
  // Apply to TextLayer
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_time_font);
  // Destroy GBitmap
  gbitmap_destroy(s_background_bitmap);

  // Destroy BitmapLayer
  bitmap_layer_destroy(s_background_layer);

  // Destroy output TextLayer
  // text_layer_destroy(s_output_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) {
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);
  
    // Send the message!
    app_message_outbox_send();
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");

  Tuple *t = dict_read_first(iterator);

  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    case KEY_MAKE_NEW_GESTURE:
      make_gesture = 1;
      break;
    case KEY_NEW_GESTURE_ID:
      break;
    case KEY_NEW_GESTURE_DATA:
      break;
    case KEY_GESTURE:
      break;
    case KEY_OLD_GESTURE_DATA:
      break;
    case KEY_OLD_GESTURE_ID:
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }

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

static void init() {
  // Create main Window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
      .load = main_window_load,
	.unload = main_window_unload
	});
  window_stack_push(s_main_window, true);
  update_time();

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Subscribe to the accelerometer data service
  // accel_data_service_subscribe(NUM_SAMPLES, data_handler); // **** this is batches
  accel_data_service_subscribe(0,NULL); // **** this is real time
  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
  head = 0; // begin head at beginning of buffer
  start_proc = 0;
  make_gesture = 0;

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());


  // Choose update rate
  // accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ); // 25Hz is default

  // APP_LOG(APP_LOG_LEVEL_INFO, "App opened!");
}

static void deinit() {
  // APP_LOG(APP_LOG_LEVEL_INFO, "App closed!");

  // Destroy main Window
  window_destroy(s_main_window);

  // accel_data_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
