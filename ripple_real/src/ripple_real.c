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
#define KEY_NEW_GESTURE_DATA_SIZE 3
#define KEY_GESTURE 4
#define KEY_OLD_GESTURE_ID 5
#define KEY_OLD_GESTURE_DATA 6
#define KEY_OLD_GESTURE_DATA_SIZE 7
#define KEY_ON_START 8

#define MAX_REF_SIZE 30 // this is the max number of samples that can be in a reference
#define MAX_GESTURES 9 // 4 default
#define MAX_BUFF_SIZE 50

#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) ((a>b)?(b):(a))
#define abs(a) (((a)>0)?(a):(-(a)))

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} DataVec;

// window and layers
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_output_layer;
static TextLayer *s_output_layer2;
static TextLayer *s_stay_still;
static TextLayer *number;

// resources
static GFont s_time_font;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

// buffer for accel data
static DataVec accel_buff[MAX_BUFF_SIZE];
static int accel_size;
static int head; // head of buffer
static int start_proc; // begin processing
static int make_gesture;
static int min_ges_i;
static const float alpha = 0.1;
static const float still_thresh = 1e5;
static const float sum_thresh = 1e6;
static const int count_thresh = 4;

// array of recorded gestures
static DataVec temp_ges[3][MAX_REF_SIZE];
static int temp_ges_size[3];
static int temp_count;
static DataVec gestures[MAX_GESTURES][MAX_REF_SIZE];
static int gesture_sizes[MAX_GESTURES];
static int gesture_count;
//static int gesture_ids[MAX_GESTURES];

static void make_a_gesture();

static int align(DataVec *ges1, int size1, DataVec *ges2, int size2) { // returns delay for the correlation with the greatest ratio between min/max
  // do x
  int i, j;
  int sumx = 0, sumy = 0, sumz = 0;
  int maxx = 0, maxy = 0, maxz = 0, minx = 0, miny = 0, minz = 0, delx = 0, dely = 0, delz = 0, del = 0, maxd, dy, dz;
  for (i = -size2+1; i < size1+size2; i++) {
    sumx = 0;
    sumy = 0;
    sumz = 0;
    for (j = max(0,i); j < min(size1,i+size2); j++) {
      sumx += ges1[j].x*ges2[j-i].x;
      sumy += ges1[j].y*ges2[j-i].y;
      sumz += ges1[j].z*ges2[j-i].z;
    }
    if (i == -size2+1) {
      maxx = sumx;
      maxy = sumy;
      maxz = sumz;
      minx = sumx;
      miny = sumy;
      minz = sumz;
      delx = i;
      dely = i;
      delz = i;
    } else {
      if (sumx > maxx) {
	maxx = sumx;
	delx = i;
      }
      if (sumx < minx) {
	minx = sumx;
      }
      if (sumy > maxy) {
	maxy = sumy;
	dely = i;
      }
      if (sumy < miny) {
	miny = sumy;
      }
      if (sumz > maxz) {
	maxz = sumz;
	delz = i;
      }
      if (sumz < minz) {
	minz = sumz;
      }
    }
  }
  del = delx;
  maxd = abs(maxx-minx);
  dy = abs(maxy-miny);
  dz = abs(maxz-minz);
  if (dy > maxd) {
    del = dely;
    maxd = dy;
  }
  if (dz > maxd) {
    del = delz;
    // maxr = rz; // unneeded
  }
  return del;
}

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

static void send_phone_message() { // (int id, DataVec *data, int size) {
  int id = gesture_count;
  DataVec *data = gestures[gesture_count];
  int size = gesture_sizes[gesture_count];
  const uint8_t key_count = 3;
  const uint32_t buffer_size = dict_calc_buffer_size(key_count, 4, sizeof(DataVec) * size, 4);
  uint8_t buffer[buffer_size];
  DictionaryIterator iter;
  DictionaryIterator *iter_p = &iter;
  dict_write_begin(iter_p, buffer, sizeof(buffer));
  app_message_outbox_begin(&iter_p);
  dict_write_int32(iter_p, (uint32_t)KEY_NEW_GESTURE_ID, (uint32_t)id);
  dict_write_int32(iter_p, (uint32_t)KEY_NEW_GESTURE_DATA_SIZE, (uint32_t)size);
  dict_write_data(iter_p, (uint32_t)KEY_NEW_GESTURE_DATA, (uint8_t *)data, (uint16_t)(sizeof(DataVec) * size));
  app_message_outbox_send();
  dict_write_end(iter_p);
}

static void send_gesture() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, KEY_GESTURE, &min_ges_i, sizeof(int), true);
  app_message_outbox_send();
}

static void timer_callback(void *data) {
  // Long lived buffer
  static char s_buffer[128];
  static char s_buffer2[128];
    
  static float x_mavg;
  static float y_mavg;
  static float z_mavg;
  static float x_diff;
  static float y_diff;
  static float z_diff;
  static float still;
  static int count = 0;
  static int find_ref = 0;
  static int second = 0;
  static int was_listening = 0;
  static int show_still = 0;
  
  int delay1; // used for correlation
  int delay2;
  int delay; // correlation during regular listening
  int start, end, num, size;
  float sum, avg, min_ges;
  int i, j;
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);

  // Compose string of all data
  snprintf(s_buffer, sizeof(s_buffer), 
	   "X: %d Y: %d Z: %d",
	   accel.x, accel.y, accel.z);
  
  //Show the data
  text_layer_set_text(s_output_layer, s_buffer);

  if (!accel.did_vibrate) {
    // accel_buff[head].x = accel.x;
    // accel_buff[head].y = accel.y;
    // accel_buff[head].z = accel.z;
    head = (head+1)%(MAX_BUFF_SIZE);
    if (head >= MAX_REF_SIZE && !start_proc) {
      start_proc = 1;
      APP_LOG(APP_LOG_LEVEL_INFO, "Starting processing");
      x_mavg = accel.x; // accel_buff[head].x;
      y_mavg = accel.y; // accel_buff[head].y;
      z_mavg = accel.z; // accel_buff[head].z;
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
      if (show_still) {
	snprintf(s_buffer2, sizeof(s_buffer2), "STILL S: %d", (int)still);
      } else {
	snprintf(s_buffer2, sizeof(s_buffer2), "S: %d", (int)still);
      }
      text_layer_set_text(s_output_layer2, s_buffer2);
      if (make_gesture) { // we were told to create a gesture by the app
	if (was_listening) {
	  find_ref = 0;
	  count = 0;
	  second = 0;
	  was_listening = 0;
	}
	if (!find_ref) { // wait for stillness
	  if (still < still_thresh) { // it is still
	    count++;
	    if (count >= count_thresh) { // achieved stillness
	      text_layer_set_text(s_stay_still, "Go!");
	      count = 0;
	      if (second) { // second (end) stillness. we found one temporary reference
		// temp_ges[temp_count] now holds our reference
		temp_count++;
		second = 0;
		make_gesture = 0;
		text_layer_destroy(s_stay_still);
		make_a_gesture();
		if (temp_count >= 3) { // finished finding references
		  APP_LOG(APP_LOG_LEVEL_INFO, "Aligning and Averaging");
		  // we now align and average the references
		  delay1 = align(temp_ges[0],temp_ges_size[0],temp_ges[1],temp_ges_size[1]); // returns delay of second to match first
		  APP_LOG(APP_LOG_LEVEL_INFO, "delay1: %d", delay1);
		  delay2 = align(temp_ges[0],temp_ges_size[0],temp_ges[2],temp_ges_size[2]);
		  APP_LOG(APP_LOG_LEVEL_INFO, "delay2: %d", delay2);
		  start = min(0,delay1);
		  start = min(start,delay2);
		  end = max(temp_ges_size[0], temp_ges_size[1]+delay1);
		  end = max(end, temp_ges_size[2]+delay2);
		  num = 0;
		  size = end-start;
		  for (i = 0; i < size; i++) {
		    gestures[gesture_count][i].x = 0;
		    gestures[gesture_count][i].y = 0;
		    gestures[gesture_count][i].z = 0;
		    if (i+start >= 0 && i+start < temp_ges_size[0]) {
		      num++;
		      gestures[gesture_count][i].x += temp_ges[0][i+start].x;
		      gestures[gesture_count][i].y += temp_ges[0][i+start].y;
		      gestures[gesture_count][i].z += temp_ges[0][i+start].z;
		    }
		    if (i+start >= delay1 && i+start < temp_ges_size[1] + delay1) {
		      num++;
		      gestures[gesture_count][i].x += temp_ges[1][i+start-delay1].x;
		      gestures[gesture_count][i].y += temp_ges[1][i+start-delay1].y;
		      gestures[gesture_count][i].z += temp_ges[1][i+start-delay1].z;
		    }
		    if (i+start >= delay2 && i+start < temp_ges_size[2] + delay2) {
		      num++;
		      gestures[gesture_count][i].x += temp_ges[2][i+start-delay2].x;
		      gestures[gesture_count][i].y += temp_ges[2][i+start-delay2].y;
		      gestures[gesture_count][i].z += temp_ges[2][i+start-delay2].z;
		    }
		    gestures[gesture_count][i].x /= num;
		    gestures[gesture_count][i].y /= num;
		    gestures[gesture_count][i].z /= num;
		    //APP_LOG(APP_LOG_LEVEL_INFO, "num:%d", num);
		    num = 0;
		  }
		  gesture_sizes[gesture_count] = size;
		  APP_LOG(APP_LOG_LEVEL_INFO, "Made gesture of size %d for id %d ", size, gesture_count);
		  /*for (i = 0; i < size; i++) {
		    APP_LOG(APP_LOG_LEVEL_INFO, "%d", gestures[gesture_count][i].z);
		    }*/
		  light_enable(false); // success only
		  app_timer_register(750, send_phone_message, NULL);
		  gesture_count++;
		  temp_count = 0;
		}
	      } else { // this is the first stillness. now find reference
		find_ref = 1;
	      }
	    }
	  }
	} else { // finding reference
	  if (still >= still_thresh) { // moving
	    if (count < MAX_REF_SIZE) {
	      temp_ges[temp_count][count].x = accel.x;
	      temp_ges[temp_count][count].y = accel.y;
	      temp_ges[temp_count][count].z = accel.z;
	      count++;
	    } else { // hit max reference size. finished finding reference
	      APP_LOG(APP_LOG_LEVEL_INFO, "Hit max ref");
	      temp_ges_size[temp_count] = MAX_REF_SIZE;
	      find_ref = 0;
	      count = 0;
	      second = 1; // find second stillness
	    }
	  } else { // hit stillness
	    if (count >= count_thresh) { // finished finding reference
	      APP_LOG(APP_LOG_LEVEL_INFO, "Made a ref of size %d", count);
	      /*for (i = 0; i < count; i++) {
		APP_LOG(APP_LOG_LEVEL_INFO, "%d", temp_ges[temp_count][i].z);
		}*/
	      temp_ges_size[temp_count] = count;
	      find_ref = 0;
	      second = 1;
	    } // else we hit a false positive. restart counter but keep finding a reference
	    count = 0;
	  }
	}
      } else { // listening regularly
	was_listening = 1;
	if (!find_ref) {
	  if (still < still_thresh) { // is still
	    count++;
	    if (count >= count_thresh) { // stillness
	      APP_LOG(APP_LOG_LEVEL_INFO, "Still");
	      show_still = 1;
	      count = 0;
	      if (second) {
		second = 0;
		min_ges_i = 0;
		min_ges = 0;
		for (i = 0; i < gesture_count; i++) { // evaluate similarity of each gesture
		  APP_LOG(APP_LOG_LEVEL_INFO, "evaluating gesture num: %d", i);
		  delay = align(accel_buff, MAX_BUFF_SIZE, gestures[i], gesture_sizes[i]);
		  APP_LOG(APP_LOG_LEVEL_INFO, "delay is: %d", delay);
		  sum = 0;
		  for (j = max(0,delay); j < min(MAX_BUFF_SIZE,gesture_sizes[i]+delay); j++) {
		    sum += (((float)(gestures[i][j].x - accel_buff[j-delay].x))*((float)(gestures[i][j].x - accel_buff[j-delay].x))) + (((float)(gestures[i][j].y - accel_buff[j-delay].y))*((float)(gestures[i][j].y - accel_buff[j-delay].y))) + (((float)(gestures[i][j].z - accel_buff[j-delay].z))*((float)(gestures[i][j].z - accel_buff[j-delay].z)));
		  }
		  avg = sum/(min(MAX_BUFF_SIZE,gesture_sizes[i]+delay) - max(0,delay));
		  if (i == 0) {
		    min_ges = avg;
		  }
		  if (avg < min_ges) {
		    min_ges = avg;
		    min_ges_i = i;
		  }
		}
		APP_LOG(APP_LOG_LEVEL_INFO, "minimum square error: %de3", (int)(min_ges/1000));
		if (min_ges < sum_thresh && gesture_count) {
		  // found gesture!
		  // send gesture for min_ges_i
		  APP_LOG(APP_LOG_LEVEL_INFO, "found gesture %d", min_ges_i);
		  /*const uint32_t buffer_size = dict_calc_buffer_size(1, 4);
		  uint8_t buffer[buffer_size];
		  DictionaryIterator iter;
		  DictionaryIterator *iter_p = &iter;
		  dict_write_begin(iter_p, buffer, sizeof(buffer));
		  app_message_outbox_begin(&iter_p);
		  dict_write_int32(iter_p, (uint32_t)KEY_GESTURE, (uint32_t)(min_ges_i+1));
		  app_message_outbox_send();
		  dict_write_end(iter_p);*/
		  app_timer_register(750, send_gesture, NULL);
		}
	      } else { // first stillness, find gesture/reference
		find_ref = 1;
	      }
	    }
	  }
	} else { // find gesture/reference
	  if (still >= still_thresh) { // moving
	    if (count < MAX_BUFF_SIZE) {
	      accel_buff[count].x = accel.x;
	      accel_buff[count].y = accel.y;
	      accel_buff[count].z = accel.z;
	      count++;
	    } else { //hit max buffer size
	      APP_LOG(APP_LOG_LEVEL_INFO, "Hit max gesture buffer");
	      accel_size = MAX_BUFF_SIZE;
	      show_still = 0;
	      find_ref = 0;
	      count = 0;
	      second = 1;
	    }
	  } else { // still
	    if (count >= count_thresh) { // found gesture/reference
	      APP_LOG(APP_LOG_LEVEL_INFO, "Hit gesture");
	      accel_size = count;
	      show_still = 0;
	      find_ref = 0;
	      second = 1;
	    }
	    count = 0;	      
	  }
	}
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

  // Create output TextLayer
  s_output_layer2 = text_layer_create(GRect(5, 140, 139, 30));
  text_layer_set_font(s_output_layer2, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer2, "No data yet.");
  text_layer_set_overflow_mode(s_output_layer2, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer2));

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
  text_layer_destroy(s_output_layer);
  text_layer_destroy(s_output_layer2);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void gesture_callback3() {
  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect window_bounds = layer_get_bounds(window_layer);
  //s_stay_still = text_layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  s_stay_still = text_layer_create(GRect(0, 0, window_bounds.size.w, 40));
  text_layer_set_background_color(s_stay_still, GColorWhite);
  text_layer_set_text_color(s_stay_still, GColorBlack);
  text_layer_set_text(s_stay_still, "Stay Still");
  text_layer_set_font(s_stay_still, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(s_stay_still, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_stay_still, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_stay_still));
  text_layer_destroy(number);
  make_gesture = 1;
}

static void gesture_callback2() {
  text_layer_set_text(number, "1");
  app_timer_register(1*1000, gesture_callback3, NULL);
}


static void gesture_callback1() {
  text_layer_set_text(number, "2");
  app_timer_register(1*1000, gesture_callback2, NULL);
}

static void make_a_gesture() {
  if (temp_count >= 3) return;
  APP_LOG(APP_LOG_LEVEL_INFO, "making gesture");  
  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect window_bounds = layer_get_bounds(window_layer);

  vibes_short_pulse();
  light_enable(true);
  number = text_layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  text_layer_set_background_color(number, GColorWhite);
  text_layer_set_text_color(number, GColorBlack);
  text_layer_set_text(number, "3");
  text_layer_set_font(number, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(number, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(number));
  app_timer_register(1*1000, gesture_callback1, NULL);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");

  Tuple *t = dict_read_first(iterator);
  int id = 0;
  int valid = 0;
  
  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    case KEY_MAKE_NEW_GESTURE:
      make_a_gesture();
      break;
    case KEY_OLD_GESTURE_ID: // ***** ID MUST COME BEFORE SIZE & DATA
      //gesture_ids[gesture_count] = (int)t->value->int32;
      id = (int)t->value->int32;
      valid++;
      APP_LOG(APP_LOG_LEVEL_INFO, "Received gesture id: %d", id);
      break;
    case KEY_OLD_GESTURE_DATA_SIZE:
      if (valid == 1) {
	gesture_sizes[id] = (int)t->value->int32;
	valid++;
      } else {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Tried to create gesture without ID");
      }
      break;
    case KEY_OLD_GESTURE_DATA:
      if (valid == 2) {
	memcpy(gestures[gesture_count], (DataVec *)t->value->data, sizeof(DataVec)*gesture_sizes[id]);
      } else {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Tried to create gesture without size");
      }
      gesture_count++;
      break;
    case KEY_GESTURE:
    case KEY_NEW_GESTURE_ID:
    case KEY_NEW_GESTURE_DATA:
    case KEY_NEW_GESTURE_DATA_SIZE:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Incorrect usage of Key %d", (int)t->key);
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

static void on_ready() {
  /*const uint32_t buffer_size = dict_calc_buffer_size(1, 4);
  uint8_t buffer[buffer_size];
  DictionaryIterator iter;
  DictionaryIterator *iter_p = &iter;
  dict_write_begin(iter_p, buffer, sizeof(buffer));
  app_message_outbox_begin(&iter_p);
  dict_write_int32(iter_p, (uint32_t)KEY_ON_START, (uint32_t)1);
  app_message_outbox_send();
  dict_write_end(iter_p);*/
  int temp = 1;
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, KEY_ON_START, &temp, sizeof(int), true);
  app_message_outbox_send();
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
  gesture_count = 0;
  temp_count = 0;

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  app_timer_register(1000, on_ready, NULL);
  // app_timer_register(3000, make_a_gesture, NULL); // DEBUGGING PURPOSES *******************
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
