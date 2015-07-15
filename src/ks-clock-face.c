#include <pebble.h>

#define COLORS       false
#define ANTIALIASING true
#define CLOCKFACE    false

#define HAND_MARGIN  20
#define FINAL_RADIUS 75

#define ANIMATION_DURATION 400
#define ANIMATION_DELAY    500

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer, *s_date_layer, *background_layer;

static GPoint s_center, s_top;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0, s_color_channels[3];
static bool s_animating = false;

const char *TH = "th";
const char *ST = "st";
const char *RD = "rd";
const char *ND = "nd";

static TextLayer *s_num_label, *s_suffix_label;
static char s_num_buffer[4],s_suffix_buffer[2];

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  for(int i = 0; i < 3; i++) {
    s_color_channels[i] = rand() % 256;
  }

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_background(Layer *layer, GContext *ctx) {
  // Color background?
  if(COLORS) {
    graphics_context_set_fill_color(ctx, GColorFromRGB(s_color_channels[0], s_color_channels[1], s_color_channels[2]));
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorBlack);
  } else {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
  }
}

static void update_clock(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, ANTIALIASING);

  if(CLOCKFACE) {
    // White clockface
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, s_center, s_radius);

    // Draw outline
    graphics_draw_circle(ctx, s_center, s_radius);
  }

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  // test values for the ui:
  // mode_time.hours = 1;
  // mode_time.minutes = 0;
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, s_top, s_top);

  // Draw hands with positive length only
  if(s_radius > 2 * HAND_MARGIN) {
    graphics_context_set_stroke_width(ctx, 17);
    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_draw_line(ctx, s_center, hour_hand);
    graphics_context_set_stroke_width(ctx, 10);
    graphics_context_set_stroke_color(ctx, GColorFolly);
    graphics_draw_line(ctx, s_center, hour_hand);
  }
  if(s_radius > HAND_MARGIN) {
    graphics_context_set_stroke_width(ctx, 10);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, s_center, minute_hand);
  }
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);

  int day = atoi(s_num_buffer);
  if ((4 <= day && day <= 20) || (24 <= day && day <= 30)) {
    strcpy(s_suffix_buffer,TH);
  } else {
    switch (day % 10 -1) {
      case 0:
        strcpy(s_suffix_buffer,ST);
        break;
      case 1:
        strcpy(s_suffix_buffer,ND);
        break;
      case 2:
        strcpy(s_suffix_buffer,RD);
        break;
    }
  }
  text_layer_set_text(s_suffix_label, s_suffix_buffer);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
  s_top = grect_center_point(&window_bounds);
  s_top.y = 10;

  background_layer = layer_create(window_bounds);
  layer_set_update_proc(background_layer, update_background);
  layer_add_child(window_layer, background_layer);

  s_date_layer = layer_create(window_bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);

  s_num_label = text_layer_create(GRect(73 - 42, 168 - 44, 44, 44));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorBlack);
  text_layer_set_text_color(s_num_label, GColorDarkGray);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_num_label, GTextAlignmentRight);
  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));

  s_suffix_label = text_layer_create(GRect(73 + 2, 168 - 42, 28, 30));
  text_layer_set_text(s_suffix_label, s_suffix_buffer);
  text_layer_set_background_color(s_suffix_label, GColorBlack);
  text_layer_set_text_color(s_suffix_label, GColorDarkGray);
  text_layer_set_font(s_suffix_label, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(s_date_layer, text_layer_get_layer(s_suffix_label));

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_clock);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(background_layer);
  layer_destroy(s_canvas_layer);
  layer_destroy(s_date_layer);

  text_layer_destroy(s_num_label);
  text_layer_destroy(s_suffix_label);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}

static void init() {
  srand(time(NULL));

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
