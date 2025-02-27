#include "gcolor_definitions.h"
#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_minute_layer;
static TextLayer *s_hour_layer;
static Layer *s_battery_layer;
static GFont s_font;
static uint8_t batt_percent = 0;

static void handle_battery(BatteryChargeState charge_state) {
  batt_percent = charge_state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect text_bounds = GRect(0, bounds.origin.y,
                            bounds.size.w * batt_percent / 100, bounds.size.h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, text_bounds, 0, 0);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Needs to be static because it's used by the system later.
  if (units_changed | SECOND_UNIT) {
    static char s_minute_text[] = "00";

    strftime(s_minute_text, sizeof(s_minute_text), "%02M", tick_time);
    text_layer_set_text(s_minute_layer, s_minute_text);
  }
  if (units_changed | SECOND_UNIT) {
    static char s_hour_text[] = "00";

    strftime(s_hour_text, sizeof(s_hour_text), "%02l", tick_time);
    text_layer_set_text(s_hour_layer, s_hour_text);
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  int battery_thickness = 5;

  s_hour_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y - 8,
                                         bounds.size.w, bounds.size.h / 2));
  text_layer_set_text_color(s_hour_layer, GColorWhite);
  text_layer_set_background_color(s_hour_layer, GColorClear);
  text_layer_set_font(s_hour_layer, s_font);
  text_layer_set_text_alignment(s_hour_layer, GTextAlignmentCenter);

  s_minute_layer = text_layer_create(
      GRect(bounds.origin.x, bounds.origin.y + bounds.size.h / 2 - 8,
            bounds.size.w, bounds.size.h / 2));
  text_layer_set_text_color(s_minute_layer, GColorFromHEX(0xba59ea));
  text_layer_set_background_color(s_minute_layer, GColorClear);
  text_layer_set_font(s_minute_layer, s_font);
  text_layer_set_text_alignment(s_minute_layer, GTextAlignmentCenter);

  s_battery_layer =
      layer_create(GRect(0, (bounds.size.h - battery_thickness) / 2,
                         bounds.size.w, battery_thickness));
  layer_set_update_proc(s_battery_layer, battery_update_proc);

  // Ensures time is displayed immediately (will break if NULL tick event
  // accessed). (This is why it's a good idea to have a separate routine to do
  // the update itself.)
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  handle_minute_tick(current_time, MINUTE_UNIT | HOUR_UNIT);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  battery_state_service_subscribe(handle_battery);

  layer_add_child(window_layer, text_layer_get_layer(s_minute_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_hour_layer));
  layer_add_child(window_layer, s_battery_layer);

  handle_battery(battery_state_service_peek());
}

static void main_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  text_layer_destroy(s_minute_layer);
  text_layer_destroy(s_hour_layer);
  layer_destroy(s_battery_layer);
}

static void init() {
  s_font =
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MAIN_75));
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers){
                                                .load = main_window_load,
                                                .unload = main_window_unload,
                                            });
  window_stack_push(s_main_window, true);
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
