#include "pebble.h"

#define CENTER_X    71
#define CENTER_Y    71
	
static Window *window;

static Layer *path_layer;

static int seconds_angle = 0;
static int minutes_angle = 0;
static int hours_angle   = 0;

static const GPathInfo SECOND_HAND_PATH_POINTS = {
	2,
	(GPoint []) {
		{0, -14},
		{0, 62}
	}
};

static const GPathInfo MINUTE_HAND_PATH_POINTS = {
  4,
  (GPoint []) {
	{-3, -12},
	{-3, 62},
	{3,  62},
	{3,  -12}
  }
};

static const GPathInfo HOUR_HAND_PATH_POINTS = {
  4,
  (GPoint []) {
	{-3, -10},
	{-3, 45},
	{3,  45},
	{3,  -10}
  }
};

static GPath *second_hand_path;
static GPath *minute_hand_path;
static GPath *hour_hand_path;

// This is the layer update callback which is called on render updates
static void path_layer_update_callback(Layer *me, GContext *ctx) {
	(void) me;

	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);

  // build out the hour marker dots
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int32_t angle = 0; angle < TRIG_MAX_ANGLE; angle += TRIG_MAX_ANGLE / 12) {
    GPoint pos = GPoint(
      (bounds.size.w / 2) + 69 * cos_lookup(angle) / TRIG_MAX_RATIO,
      (bounds.size.h / 2) + 69 * sin_lookup(angle) / TRIG_MAX_RATIO
	);
    graphics_fill_circle(ctx, pos, 2);
  }
	
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  gpath_rotate_to(hour_hand_path, hours_angle);
  gpath_draw_filled(ctx, hour_hand_path);
  gpath_draw_outline(ctx, hour_hand_path);
	
  // draw the minute hand
  gpath_rotate_to(minute_hand_path, minutes_angle);
  gpath_draw_filled(ctx, minute_hand_path);
  gpath_draw_outline(ctx, minute_hand_path);
	
  // draw the minute hand
  gpath_draw_filled(ctx, minute_hand_path);
  gpath_draw_outline(ctx, minute_hand_path);
	
  // draw the second hand
  graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
  gpath_rotate_to(second_hand_path, seconds_angle);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  gpath_draw_outline(ctx, second_hand_path);

  // outer circle
  graphics_fill_circle(ctx, center, 5);

  // inner circle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 1);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	
	seconds_angle = (TRIG_MAX_ANGLE / 60) * (tick_time->tm_sec + 90);
	minutes_angle = (TRIG_MAX_ANGLE / 60) * (tick_time->tm_min + 90);
	hours_angle   = (TRIG_MAX_ANGLE / 60) * ((tick_time->tm_hour * 5 + (tick_time->tm_min / 12)) + 90);
	
	layer_mark_dirty(path_layer);
}

static void init() {
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);

  // window_set_click_config_provider(window, config_provider);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  path_layer = layer_create(bounds);
  layer_set_update_proc(path_layer, path_layer_update_callback);
  layer_add_child(window_layer, path_layer);

  // Pass the corresponding GPathInfo to initialize a GPath
  second_hand_path = gpath_create(&SECOND_HAND_PATH_POINTS);
  minute_hand_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
  hour_hand_path   = gpath_create(&HOUR_HAND_PATH_POINTS);

  gpath_move_to(second_hand_path, GPoint(bounds.size.w / 2, bounds.size.h / 2));
  gpath_move_to(minute_hand_path, GPoint(bounds.size.w / 2, bounds.size.h / 2));
  gpath_move_to(hour_hand_path,   GPoint(bounds.size.w / 2, bounds.size.h / 2));

  tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) tick_handler);
	
  // load up a value before the first tick
  time_t temp = time(NULL);
  struct tm *t = localtime(&temp);
  tick_handler(t, SECOND_UNIT);

}

static void deinit() {
  gpath_destroy(second_hand_path);
  gpath_destroy(minute_hand_path);
  gpath_destroy(hour_hand_path);

  layer_destroy(path_layer);
  window_destroy(window);
	
  tick_timer_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
