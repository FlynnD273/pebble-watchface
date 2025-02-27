#include "gcolor_definitions.h"
#include "src/resource_ids.auto.h"
#include <pebble.h>
#include <stdint.h>

static Window *s_main_window;

static BitmapLayer *s_minute_10_layer;
static BitmapLayer *s_minute_1_layer;
static BitmapLayer *s_hour_10_layer;
static BitmapLayer *s_hour_1_layer;

static GBitmap *s_minute_10_bitmap = NULL;
static GBitmap *s_minute_1_bitmap = NULL;
static GBitmap *s_hour_10_bitmap = NULL;
static GBitmap *s_hour_1_bitmap = NULL;

static Layer *s_battery_layer;
static Layer *window_layer;
static uint8_t batt_percent = 0;

const int DIGITS[10] = {
    RESOURCE_ID_IMAGE_B_0, RESOURCE_ID_IMAGE_B_1, RESOURCE_ID_IMAGE_B_2,
    RESOURCE_ID_IMAGE_B_3, RESOURCE_ID_IMAGE_B_4, RESOURCE_ID_IMAGE_B_5,
    RESOURCE_ID_IMAGE_B_6, RESOURCE_ID_IMAGE_B_7, RESOURCE_ID_IMAGE_B_8,
    RESOURCE_ID_IMAGE_B_9,
};

static int palette_size(GBitmap *bitmap) {
  GBitmapFormat format = gbitmap_get_format(bitmap);
  switch (format) {
  case GBitmapFormat1BitPalette:
    return 2;
  case GBitmapFormat2BitPalette:
    return 4;
  case GBitmapFormat4BitPalette:
    return 8;
  default:
    return 0;
  }
  return 0;
}

static void swap_color(GBitmap *bitmap, GColor original, GColor new) {
  uint8_t no_alpha = original.argb & 0b00111111;
  int len = palette_size(bitmap);
  GColor *palette = gbitmap_get_palette(bitmap);
  for (int i = 0; i < len; i++) {
    if ((palette[i].argb & 0b00111111) == no_alpha) {
      palette[i].argb = (palette[i].a << 6) | (new.argb & 0b00111111);
      return;
    }
  }
}

static void handle_battery(BatteryChargeState charge_state) {
  batt_percent = charge_state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect batt_bar = GRect(bounds.origin.x, bounds.origin.y,
                         bounds.size.w * batt_percent / 100, bounds.size.h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, batt_bar, 0, 0);
  for (uint8_t i = 0; i <= 100; i += 20) {
    if (i < batt_percent) {
      graphics_context_set_stroke_color(ctx, GColorBlack);
    } else {
      graphics_context_set_stroke_color(ctx, GColorWhite);
    }
    uint8_t x = bounds.origin.x + bounds.size.w * i / 100;
    graphics_draw_line(ctx, GPoint(x, bounds.origin.y),
                       GPoint(x, bounds.origin.y + bounds.size.h));
  }
}

static void unload_bitmap(BitmapLayer **layer, GBitmap **bitmap) {
  if (*layer) {
    layer_remove_from_parent(bitmap_layer_get_layer(*layer));
    bitmap_layer_destroy(*layer);
    layer = NULL;
  }
  if (*bitmap) {
    gbitmap_destroy(*bitmap);
    bitmap = NULL;
  }
}

static void load_bitmap(BitmapLayer **layer, GBitmap **bitmap, int number,
                        GColor color) {
  GRect bounds = layer_get_frame(bitmap_layer_get_layer(*layer));
  unload_bitmap(layer, bitmap);
  *bitmap = gbitmap_create_with_resource(DIGITS[number]);
  swap_color(*bitmap, GColorBlack, color);
  *layer = bitmap_layer_create(
      GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h));
  bitmap_layer_set_bitmap(*layer, *bitmap);
  bitmap_layer_set_compositing_mode(*layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(*layer));
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Needs to be static because it's used by the system later.
  if (units_changed & MINUTE_UNIT) {
    int min = tick_time->tm_min;
    if (min < 10) {
      load_bitmap(&s_minute_10_layer, &s_minute_10_bitmap, 0, GColorPurpureus);
    } else {
      load_bitmap(&s_minute_10_layer, &s_minute_10_bitmap, min / 10,
                  GColorPurpureus);
    }
    load_bitmap(&s_minute_1_layer, &s_minute_1_bitmap, min % 10,
                GColorPurpureus);
  }
  if (units_changed & HOUR_UNIT) {
    int hour = tick_time->tm_hour;
    hour %= 12;
    if (hour < 10) {
      load_bitmap(&s_hour_10_layer, &s_hour_10_bitmap, 0, GColorWhite);
    } else {
      load_bitmap(&s_hour_10_layer, &s_hour_10_bitmap, hour / 10, GColorWhite);
    }
    load_bitmap(&s_hour_1_layer, &s_hour_1_bitmap, hour % 10, GColorWhite);
  }
}

static void main_window_load(Window *window) {
  window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  int battery_thickness = 5;
  int vert_offset = 1;
  int w = 60;
  int h = 80;

  s_hour_10_layer =
      bitmap_layer_create(GRect(bounds.size.w / 2 - w, vert_offset, w, h));
  bitmap_layer_set_background_color(s_hour_10_layer, GColorClear);

  s_hour_1_layer =
      bitmap_layer_create(GRect(bounds.size.w / 2, vert_offset, w, h));
  bitmap_layer_set_background_color(s_hour_1_layer, GColorClear);

  s_minute_10_layer = bitmap_layer_create(
      GRect(bounds.size.w / 2 - w, h + battery_thickness + vert_offset, w, h));
  bitmap_layer_set_background_color(s_minute_10_layer, GColorClear);

  s_minute_1_layer = bitmap_layer_create(
      GRect(bounds.size.w / 2, h + battery_thickness + vert_offset, w, h));
  bitmap_layer_set_background_color(s_minute_1_layer, GColorClear);

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

  tick_timer_service_subscribe(MINUTE_UNIT | HOUR_UNIT, handle_minute_tick);
  battery_state_service_subscribe(handle_battery);

  layer_add_child(window_layer, bitmap_layer_get_layer(s_hour_10_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_hour_1_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_minute_10_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_minute_1_layer));

  layer_add_child(window_layer, s_battery_layer);

  handle_battery(battery_state_service_peek());
}

static void main_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bitmap_layer_destroy(s_hour_10_layer);
  bitmap_layer_destroy(s_hour_1_layer);
  bitmap_layer_destroy(s_minute_10_layer);
  bitmap_layer_destroy(s_minute_1_layer);
  layer_destroy(s_battery_layer);
}

static void init() {
  /* s_font = */
  /*     fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MAIN_75));
   */
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
