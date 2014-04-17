#include "pebble.h"

#define CIRCLE_RADIUS 69

static int show_second_hand    = 1;
static int show_bluetooth_icon = 1;
static int show_battery_icon   = 1;

static Window *window;
static Layer *path_layer;

static GBitmap *battery_images[11];
static BitmapLayer *battery_layer;

static GBitmap *bluetooth_images[2];
static BitmapLayer *bluetooth_layer;

static int seconds_angle = 0;
static int minutes_angle = 0;
static int hours_angle   = 0;

// various values that will change
static int showing_info = 0;
static int is_charging = 0;

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

void on_animation_stopped(Animation *anim, bool finished, void *context) {
	property_animation_destroy((PropertyAnimation*) anim);
}

void animate_layer(Layer *layer, GRect *start, GRect *finish, int duration, int delay) {
	// declare animation
	PropertyAnimation *anim = property_animation_create_layer_frame(layer, start, finish);
 
	// set characteristics
	animation_set_duration((Animation*) anim, duration);
	animation_set_delay((Animation*) anim, delay);

	// set stopped handler to free memory
	AnimationHandlers handlers = {
		// the reference to the stopped handler is the only one in the array
		.stopped = (AnimationStoppedHandler) on_animation_stopped
	};
	animation_set_handlers((Animation*) anim, handlers, NULL);

	// start animation
	animation_schedule((Animation*) anim);
}

static void path_layer_update_callback(Layer *me, GContext *ctx) {
	(void) me;

	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);

	// build out the hour marker dots
	graphics_context_set_fill_color(ctx, GColorWhite);
	for (int32_t angle = 0; angle < TRIG_MAX_ANGLE; angle += TRIG_MAX_ANGLE / 12) {
		GPoint pos = GPoint(
			(bounds.size.w / 2) + CIRCLE_RADIUS * cos_lookup(angle) / TRIG_MAX_RATIO,
			(bounds.size.h / 2) + CIRCLE_RADIUS * sin_lookup(angle) / TRIG_MAX_RATIO
		);
		graphics_fill_circle(ctx, pos, 2);
	}
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	// draw the hour hand
	gpath_rotate_to(hour_hand_path, hours_angle);
	gpath_draw_filled(ctx, hour_hand_path);
	gpath_draw_outline(ctx, hour_hand_path);

	// draw the minute hand
	gpath_rotate_to(minute_hand_path, minutes_angle);
	gpath_draw_filled(ctx, minute_hand_path);
	gpath_draw_outline(ctx, minute_hand_path);

  	// draw the second hand
	if (show_second_hand == 1) {
		graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
		gpath_rotate_to(second_hand_path, seconds_angle);
		graphics_context_set_stroke_color(ctx, GColorWhite);
		gpath_draw_outline(ctx, second_hand_path);
	}

	// outer circle
	graphics_fill_circle(ctx, center, 5);

	// inner circle
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, center, 2);
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	seconds_angle = (TRIG_MAX_ANGLE / 60) * (tick_time->tm_sec + 90);
	minutes_angle = (TRIG_MAX_ANGLE / 60) * (tick_time->tm_min + 90);
	hours_angle   = (TRIG_MAX_ANGLE / 60) * ((tick_time->tm_hour * 5 + (tick_time->tm_min / 12)) + 90);
	layer_mark_dirty(path_layer);
	
	if (show_battery_icon == 1 && is_charging == 1) {
		if (tick_time->tm_sec % 2 == 0) {
			layer_set_hidden(bitmap_layer_get_layer(battery_layer), 0);
		}
		else {
			layer_set_hidden(bitmap_layer_get_layer(battery_layer), 1);
		}
	}
}

void hide_info_layers(void *data) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);

	// hide the battery layer
	GRect start = GRect(bounds.size.w - 15 - 5,   7, 15, 11);
	GRect end   = GRect(bounds.size.w - 15 - 5, -50, 15, 11);
	animate_layer(bitmap_layer_get_layer(battery_layer), &start, &end, 500, 0);

	// hide the bluetooth layer
	GRect start2 = GRect(5,   5, 15, 15);
	GRect end2   = GRect(5, -50, 15, 15);
	animate_layer(bitmap_layer_get_layer(bluetooth_layer), &start2, &end2, 500, 0);

	showing_info = 0;
}

void show_info_layers(void) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);

	// show the battery layer
	GRect start = GRect(bounds.size.w - 15 - 5, -50, 15, 11);
	GRect end   = GRect(bounds.size.w - 15 - 5,   7, 15, 11);
	animate_layer(bitmap_layer_get_layer(battery_layer), &start, &end, 500, 0);

	// show the bluetooth layer
	GRect start2 = GRect(5, -50, 15, 15);
	GRect end2   = GRect(5,   5, 15, 15);
	animate_layer(bitmap_layer_get_layer(bluetooth_layer), &start2, &end2, 500, 0);

	// in 3 seconds, make the layers slide away
	app_timer_register(3000, hide_info_layers, NULL);
}

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
	if (showing_info == 1) {
		return;
	}
	showing_info = 1;

	show_info_layers();
}

void handle_bluetooth(bool connected) {
	bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_images[connected ? 1 : 0]);
	layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), show_bluetooth_icon == 0);
}

void handle_battery(BatteryChargeState charge_state) {
	int offset = charge_state.charge_percent / 10;
	bitmap_layer_set_bitmap(battery_layer, battery_images[offset]);
	layer_set_hidden(bitmap_layer_get_layer(battery_layer), show_battery_icon == 0);
	
	is_charging = charge_state.is_charging;
}

static void init() {
	window = window_create();
	window_stack_push(window, true);
	window_set_background_color(window, GColorBlack);

	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);

	path_layer = layer_create(bounds);
	layer_set_update_proc(path_layer, path_layer_update_callback);
	layer_add_child(window_layer, path_layer);

	// initialize the gpaths
	second_hand_path = gpath_create(&SECOND_HAND_PATH_POINTS);
	minute_hand_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
	hour_hand_path   = gpath_create(&HOUR_HAND_PATH_POINTS);

	GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
	gpath_move_to(second_hand_path, center);
	gpath_move_to(minute_hand_path, center);
	gpath_move_to(hour_hand_path,   center);

	// set up bluetooth icons
	bluetooth_images[0] = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_IMAGE_OFF);
	bluetooth_images[1] = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_IMAGE_ON);
	bluetooth_layer = bitmap_layer_create(GRect(5, 5, 15, 15));
	layer_add_child(window_layer, bitmap_layer_get_layer(bluetooth_layer));

	battery_images[0]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_0);
	battery_images[1]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_10); 
	battery_images[2]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_20); 
	battery_images[3]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_30); 
	battery_images[4]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_40); 
	battery_images[5]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_50); 
	battery_images[6]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_60); 
	battery_images[7]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_70); 
	battery_images[8]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_80); 
	battery_images[9]  = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_90); 
	battery_images[10] = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMAGE_100); 

	battery_layer = bitmap_layer_create(GRect(bounds.size.w - 15 - 5, 7, 15, 11));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_layer));

	tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) tick_handler);

	battery_state_service_subscribe(&handle_battery);
	handle_battery(battery_state_service_peek());

	bluetooth_connection_service_subscribe(&handle_bluetooth);
	handle_bluetooth(bluetooth_connection_service_peek());

	accel_tap_service_subscribe(&accel_tap_handler);
	hide_info_layers(NULL);

	// load up a value before the first tick
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);
	tick_handler(t, SECOND_UNIT);
}

static void deinit() {
	gpath_destroy(second_hand_path);
	gpath_destroy(minute_hand_path);
	gpath_destroy(hour_hand_path);

	bitmap_layer_destroy(battery_layer);
	for (int i = 0; i < 11; i++) {
		gbitmap_destroy(battery_images[i]);
	}

	bitmap_layer_destroy(bluetooth_layer);
	for (int i = 0; i < 2; i++) {
		gbitmap_destroy(bluetooth_images[i]);
	}

	layer_destroy(path_layer);
	window_destroy(window);

	accel_tap_service_unsubscribe();
	battery_state_service_unsubscribe();
	tick_timer_service_unsubscribe();
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}