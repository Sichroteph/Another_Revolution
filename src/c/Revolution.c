// Copyright (c) 2013 Douwe Maan <http://www.douwemaan.com/>
// Also features additions by Ben10do.
// The above copyright notice shall be included in all copies or substantial portions of the program.

// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include <pebble.h>


// Settings
#define USE_AMERICAN_DATE_FORMAT      false
#define VIBE_ON_HOUR                  false
#define TIME_SLOT_ANIMATION_DURATION  800  
  
// Magic numbers  
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168

#define TIME_IMAGE_WIDTH    70
#define TIME_IMAGE_HEIGHT   70

#define DATE_IMAGE_WIDTH    20
#define DATE_IMAGE_HEIGHT   20

#define BAT_IMAGE_WIDTH  10
#define BAT_IMAGE_HEIGHT 10

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    10

#define MARGIN              1
#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     4

bool is_connected=false;
static char pebble_Lang[20]=" ";


void bt_handler(bool connected) {
  if(!is_connected&&connected){
    is_connected=true;
    vibes_double_pulse();
  }
  if(is_connected&&!connected) {
    is_connected=false;
    vibes_double_pulse();
  }
  // APP_LOG(APP_LOG_LEVEL_DEBUG,"dirty handler bt");

}

// Images
#define NUMBER_OF_TIME_IMAGES 10
const int TIME_IMAGE_RESOURCE_IDS[NUMBER_OF_TIME_IMAGES] = {
  RESOURCE_ID_IMAGE_TIME_0, 
  RESOURCE_ID_IMAGE_TIME_1, RESOURCE_ID_IMAGE_TIME_2, RESOURCE_ID_IMAGE_TIME_3, 
  RESOURCE_ID_IMAGE_TIME_4, RESOURCE_ID_IMAGE_TIME_5, RESOURCE_ID_IMAGE_TIME_6, 
  RESOURCE_ID_IMAGE_TIME_7, RESOURCE_ID_IMAGE_TIME_8, RESOURCE_ID_IMAGE_TIME_9
};

#define NUMBER_OF_DATE_IMAGES 10
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_IMAGE_DATE_0, 
  RESOURCE_ID_IMAGE_DATE_1, RESOURCE_ID_IMAGE_DATE_2, RESOURCE_ID_IMAGE_DATE_3, 
  RESOURCE_ID_IMAGE_DATE_4, RESOURCE_ID_IMAGE_DATE_5, RESOURCE_ID_IMAGE_DATE_6, 
  RESOURCE_ID_IMAGE_DATE_7, RESOURCE_ID_IMAGE_DATE_8, RESOURCE_ID_IMAGE_DATE_9
};

#define NUMBER_OF_BAT_IMAGES 10
const int BAT_IMAGE_RESOURCE_IDS[NUMBER_OF_BAT_IMAGES] = {
  RESOURCE_ID_IMAGE_BAT_0, 
  RESOURCE_ID_IMAGE_BAT_1, RESOURCE_ID_IMAGE_BAT_2, RESOURCE_ID_IMAGE_BAT_3, 
  RESOURCE_ID_IMAGE_BAT_4, RESOURCE_ID_IMAGE_BAT_5, RESOURCE_ID_IMAGE_BAT_6, 
  RESOURCE_ID_IMAGE_BAT_7, RESOURCE_ID_IMAGE_BAT_8, RESOURCE_ID_IMAGE_BAT_9
};

#define NUMBER_OF_DAY_IMAGES 7
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0, RESOURCE_ID_IMAGE_DAY_1, RESOURCE_ID_IMAGE_DAY_2, 
  RESOURCE_ID_IMAGE_DAY_3, RESOURCE_ID_IMAGE_DAY_4, RESOURCE_ID_IMAGE_DAY_5, 
  RESOURCE_ID_IMAGE_DAY_6
};


const int DAY_EN_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0_EN, RESOURCE_ID_IMAGE_DAY_1_EN, RESOURCE_ID_IMAGE_DAY_2_EN, 
  RESOURCE_ID_IMAGE_DAY_3_EN, RESOURCE_ID_IMAGE_DAY_4_EN, RESOURCE_ID_IMAGE_DAY_5_EN, 
  RESOURCE_ID_IMAGE_DAY_6_EN
};


// General
static Window *window;


#define EMPTY_SLOT -1
typedef struct Slot {
  int         number;
  GBitmap     *image;
  BitmapLayer *image_layer;
  int         state;
} Slot;  

// Time
typedef struct TimeSlot {
  Slot              slot;
  int               new_state;
  PropertyAnimation *slide_out_animation;
  PropertyAnimation *slide_in_animation;
  bool              updating;
} TimeSlot;

#define NUMBER_OF_TIME_SLOTS 4
static Layer *time_layer;
static TimeSlot time_slots[NUMBER_OF_TIME_SLOTS];

// Footer
static Layer *footer_layer;

// Day
typedef struct DayItem {
  GBitmap     *image;
  BitmapLayer *image_layer;
  Layer       *layer;
  bool       loaded;
} DayItem;
static DayItem day_item;

// Date
#define NUMBER_OF_DATE_SLOTS 4
static Layer *date_layer;
static Slot date_slots[NUMBER_OF_DATE_SLOTS];

// Seconds
#define NUMBER_OF_BAT_SLOTS 2
static Layer *bat_layer;
static Slot bat_slots[NUMBER_OF_BAT_SLOTS];


// General
void destroy_property_animation(PropertyAnimation **prop_animation);
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);

// Time
void display_time(struct tm *tick_time);
void display_time_value(int value, int row_number);
void update_time_slot(TimeSlot *time_slot, int digit_value);
GRect frame_for_time_slot(TimeSlot *time_slot);
void slide_in_digit_image_into_time_slot(TimeSlot *time_slot, int digit_value);
void time_slot_slide_in_animation_stopped(Animation *slide_in_animation, bool finished, void *context);
void slide_out_digit_image_from_time_slot(TimeSlot *time_slot);
void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context);

// Day
void display_day(struct tm *tick_time);
void unload_day_item();

// Date
void display_date(struct tm *tick_time);
void display_date_value(int value, int part_number);
void update_date_slot(Slot *date_slot, int digit_value);

// Seconds
void display_bat(void);
void update_bat_slot(Slot *bat_slot, int digit_value);

// Handlers
int main(void);
void init();
void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);
void deinit();


// General
void destroy_property_animation(PropertyAnimation **animation) {
  if (*animation == NULL)
    return;

  if (animation_is_scheduled((Animation *)*animation)) {
    animation_unschedule((Animation *)*animation);
  }

  property_animation_destroy(*animation);
  *animation = NULL;
}

BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 9)
    return NULL;

  if (slot->state != EMPTY_SLOT)
    return NULL;

  slot->state = digit_value;

  slot->image = gbitmap_create_with_resource(digit_resource_ids[digit_value]);

  slot->image_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(slot->image_layer, slot->image);
  layer_add_child(parent_layer, bitmap_layer_get_layer(slot->image_layer));

  return slot->image_layer;
}

void unload_digit_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT)
    return;

  layer_remove_from_parent(bitmap_layer_get_layer(slot->image_layer));
  bitmap_layer_destroy(slot->image_layer);

  gbitmap_destroy(slot->image);

  slot->state = EMPTY_SLOT;
}

// Time
void display_time(struct tm *tick_time) {
  int hour = tick_time->tm_hour;

  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  display_time_value(hour,              0);
  display_time_value(tick_time->tm_min, 1);
}

void display_time_value(int value, int row_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int time_slot_number = (row_number * 2) + column_number;

    TimeSlot *time_slot = &time_slots[time_slot_number];

    update_time_slot(time_slot, value % 10);

    value = value / 10;
  }
}

void update_time_slot(TimeSlot *time_slot, int digit_value) {
  if (time_slot->slot.state == digit_value)
    return;

  if (time_slot->updating) {
    // Otherwise we'll crash when the animation is replaced by a new animation before we're finished.
    return;
  }

  time_slot->updating = true;

  if (time_slot->slot.state == EMPTY_SLOT) {
    slide_in_digit_image_into_time_slot(time_slot, digit_value);
  }
  else {
    time_slot->new_state = digit_value;
    slide_out_digit_image_from_time_slot(time_slot);
  }
}

GRect frame_for_time_slot(TimeSlot *time_slot) {
  int x = MARGIN + (time_slot->slot.number % 2) * (TIME_IMAGE_WIDTH + TIME_SLOT_SPACE);
  int y = MARGIN + (time_slot->slot.number / 2) * (TIME_IMAGE_HEIGHT + TIME_SLOT_SPACE);

  return GRect(x, y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);
}

void slide_in_digit_image_into_time_slot(TimeSlot *time_slot, int digit_value) {
  destroy_property_animation(&time_slot->slide_in_animation);

  GRect to_frame = frame_for_time_slot(time_slot);

  int from_x = to_frame.origin.x;
  int from_y = to_frame.origin.y;
  switch (time_slot->slot.number) {
    case 0:
    from_x -= TIME_IMAGE_WIDTH + MARGIN;
    break;
    case 1:
    from_y -= TIME_IMAGE_HEIGHT + MARGIN;
    break;
    case 2:
    from_y += TIME_IMAGE_HEIGHT + MARGIN;
    break;
    case 3:
    from_x += TIME_IMAGE_WIDTH + MARGIN;
    break;
  }
  GRect from_frame = GRect(from_x, from_y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);

  BitmapLayer *image_layer = load_digit_image_into_slot(&time_slot->slot, digit_value, time_layer, from_frame, TIME_IMAGE_RESOURCE_IDS);

  time_slot->slide_in_animation = property_animation_create_layer_frame(bitmap_layer_get_layer(image_layer), &from_frame, &to_frame);

  Animation *animation = (Animation *)time_slot->slide_in_animation;
  animation_set_duration( animation,  TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(    animation,  AnimationCurveEaseOut);
  animation_set_handlers( animation,  (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)time_slot_slide_in_animation_stopped
  }, (void *)time_slot);

  animation_schedule(animation);
}

void time_slot_slide_in_animation_stopped(Animation *slide_in_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  destroy_property_animation(&time_slot->slide_in_animation);

  time_slot->updating = false;
}

void slide_out_digit_image_from_time_slot(TimeSlot *time_slot) {
  destroy_property_animation(&time_slot->slide_out_animation);

  GRect from_frame = frame_for_time_slot(time_slot);

  int to_x = from_frame.origin.x;
  int to_y = from_frame.origin.y;
  switch (time_slot->slot.number) {
    case 0:
    to_y -= TIME_IMAGE_HEIGHT + MARGIN;
    break;
    case 1:
    to_x += TIME_IMAGE_WIDTH + MARGIN;
    break;
    case 2:
    to_x -= TIME_IMAGE_WIDTH + MARGIN;
    break;
    case 3:
    to_y += TIME_IMAGE_HEIGHT + MARGIN;
    break;
  }
  GRect to_frame = GRect(to_x, to_y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);

  BitmapLayer *image_layer = time_slot->slot.image_layer;

  time_slot->slide_out_animation = property_animation_create_layer_frame(bitmap_layer_get_layer(image_layer), &from_frame, &to_frame);

  Animation *animation = (Animation *)time_slot->slide_out_animation;
  animation_set_duration( animation,  TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(    animation,  AnimationCurveEaseIn);
  animation_set_handlers(animation, (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)time_slot_slide_out_animation_stopped
  }, (void *)time_slot);

  animation_schedule(animation);
}

void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  destroy_property_animation(&time_slot->slide_out_animation);

  if (time_slot->new_state == EMPTY_SLOT) {
    time_slot->updating = false;
  }
  else {
    unload_digit_image_from_slot(&time_slot->slot);

    slide_in_digit_image_into_time_slot(time_slot, time_slot->new_state);

    time_slot->new_state = EMPTY_SLOT;
  }
}

// Day
void display_day(struct tm *tick_time) {
  unload_day_item();

  if (strcmp(pebble_Lang, "fr_FR") == 0)
    day_item.image = gbitmap_create_with_resource(DAY_IMAGE_RESOURCE_IDS[tick_time->tm_wday]);
  else
    day_item.image = gbitmap_create_with_resource(DAY_EN_IMAGE_RESOURCE_IDS[tick_time->tm_wday]);



  day_item.image_layer = bitmap_layer_create(gbitmap_get_bounds(day_item.image));  
  bitmap_layer_set_bitmap(day_item.image_layer, day_item.image);
  layer_add_child(day_item.layer, bitmap_layer_get_layer(day_item.image_layer));

  day_item.loaded = true;
}

void unload_day_item() {
  if (!day_item.loaded) 
    return;

  layer_remove_from_parent(bitmap_layer_get_layer(day_item.image_layer));
  bitmap_layer_destroy(day_item.image_layer);

  gbitmap_destroy(day_item.image);
}

// Date
void display_date(struct tm *tick_time) {
  int day   = tick_time->tm_mday;
  int month = tick_time->tm_mon + 1;

  #if USE_AMERICAN_DATE_FORMAT
  display_date_value(month, 0);
  display_date_value(day,   1);
  #else
  display_date_value(day,   0);
  display_date_value(month, 1);
  #endif
}

void display_date_value(int value, int part_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int date_slot_number = (part_number * 2) + column_number;

    Slot *date_slot = &date_slots[date_slot_number];

    update_date_slot(date_slot, value % 10);

    value = value / 10;
  }
}

void update_date_slot(Slot *date_slot, int digit_value) {
  if (date_slot->state == digit_value)
    return;

  int x = date_slot->number * (DATE_IMAGE_WIDTH + MARGIN);
  if (date_slot->number >= 2) {
    x += 3; // 3 extra pixels of space between the day and month
  }
  GRect frame =  GRect(x, 0, DATE_IMAGE_WIDTH, DATE_IMAGE_HEIGHT);

  unload_digit_image_from_slot(date_slot);
  load_digit_image_into_slot(date_slot, digit_value, date_layer, frame, DATE_IMAGE_RESOURCE_IDS);
}

// Seconds
void display_bat() {
  BatteryChargeState battery_state = battery_state_service_peek();
  int bat = battery_state.charge_percent;

  bat = bat % 100; // Maximum of two digits per row.

  for (int bat_slot_number = 1; bat_slot_number >= 0; bat_slot_number--) {
    Slot *bat_slot = &bat_slots[bat_slot_number];

    update_bat_slot(bat_slot, bat % 10);

    bat = bat / 10;
  }
}

void update_bat_slot(Slot *bat_slot, int digit_value) {
  if (bat_slot->state == digit_value)
    return;

  GRect frame = GRect(
    bat_slot->number * (BAT_IMAGE_WIDTH + MARGIN), 
    0, 
    BAT_IMAGE_WIDTH, 
    BAT_IMAGE_HEIGHT
  );

  unload_digit_image_from_slot(bat_slot);
  load_digit_image_into_slot(bat_slot, digit_value, bat_layer, frame, BAT_IMAGE_RESOURCE_IDS);
}

// Handlers




int main(void) {
  init();
  app_event_loop();
  deinit();
}

void init() {

  snprintf(pebble_Lang, sizeof(pebble_Lang), "%s", i18n_get_system_locale());

  bluetooth_connection_service_subscribe(bt_handler); 

  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  Layer *root_layer = window_get_root_layer(window);

  // Time
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    TimeSlot *time_slot = &time_slots[i];
    time_slot->slot.number  = i;
    time_slot->slot.state   = EMPTY_SLOT;
    time_slot->new_state    = EMPTY_SLOT;
    time_slot->updating     = false;
  }

  time_layer = layer_create(GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(time_layer, true);
  layer_add_child(root_layer, time_layer);

  // Footer
  int footer_height = SCREEN_HEIGHT - SCREEN_WIDTH;

  footer_layer = layer_create(GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, footer_height));
  layer_add_child(root_layer, footer_layer);

  // Day
  day_item.loaded = false;

  GRect day_layer_frame = GRect(
    MARGIN, 
    footer_height - DAY_IMAGE_HEIGHT - MARGIN, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );
  day_item.layer = layer_create(day_layer_frame);
  layer_add_child(footer_layer, day_item.layer);

  // Date
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    Slot *date_slot = &date_slots[i];
    date_slot->number = i;
    date_slot->state  = EMPTY_SLOT;
  }

  GRect date_layer_frame = GRectZero;
  date_layer_frame.size.w   = DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH + DATE_PART_SPACE + DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH;
  date_layer_frame.size.h   = DATE_IMAGE_HEIGHT;
  date_layer_frame.origin.x = (SCREEN_WIDTH - date_layer_frame.size.w) / 2;
  date_layer_frame.origin.y = footer_height - DATE_IMAGE_HEIGHT - MARGIN;

  date_layer = layer_create(date_layer_frame);
  layer_add_child(footer_layer, date_layer);

  // Seconds
  for (int i = 0; i < NUMBER_OF_BAT_SLOTS; i++) {
    Slot *bat_slot = &bat_slots[i];
    bat_slot->number = i;
    bat_slot->state  = EMPTY_SLOT;
  }

  GRect bat_layer_frame = GRect(
    SCREEN_WIDTH - BAT_IMAGE_WIDTH - MARGIN - BAT_IMAGE_WIDTH - MARGIN, 
    footer_height - BAT_IMAGE_HEIGHT - MARGIN, 
    BAT_IMAGE_WIDTH + MARGIN + BAT_IMAGE_WIDTH, 
    BAT_IMAGE_HEIGHT
  );
  bat_layer = layer_create(bat_layer_frame);
  layer_add_child(footer_layer, bat_layer);


  // Display
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  display_time(tick_time);
  display_day(tick_time);
  display_date(tick_time);
  display_bat();

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  display_bat();

  // if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
  display_time(tick_time);
  //  }

  #if VIBE_ON_HOUR
  if ((units_changed & HOUR_UNIT) == HOUR_UNIT) {
    vibes_enqueue_custom_pattern((VibePattern) {
      .durations = (uint32_t []) {50},
        .num_segments = 1
    });
  }
  #endif

  if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    display_day(tick_time);
    display_date(tick_time);
  }
}

void deinit() {
  // Time
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    unload_digit_image_from_slot(&time_slots[i].slot);

    destroy_property_animation(&time_slots[i].slide_in_animation);
    destroy_property_animation(&time_slots[i].slide_out_animation);
  }
  layer_destroy(time_layer);

  // Day
  unload_day_item();
  layer_destroy(day_item.layer);

  // Date
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_digit_image_from_slot(&date_slots[i]);
  }
  layer_destroy(date_layer);

  // Seconds
  for (int i = 0; i < NUMBER_OF_BAT_SLOTS; i++) {
    unload_digit_image_from_slot(&bat_slots[i]);
  }
  layer_destroy(bat_layer);


  layer_destroy(footer_layer);

  window_destroy(window);
}