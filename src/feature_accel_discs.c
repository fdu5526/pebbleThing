#include "pebble.h"

#define MATH_PI 3.141592653589793238462
#define NUM_DISCS 1
#define DISC_DENSITY 0.25
#define ACCEL_RATIO 0.05
#define ACCEL_STEP_MS 50
#define VIBRATE_FACTOR 2.2
  
// vector in 2D
typedef struct Vec2d {
  double x;
  double y;
} Vec2d;

// circular disc
typedef struct Disc {
  Vec2d pos;
  Vec2d vel;
  double mass;
  double radius;
} Disc;

static Disc discs[NUM_DISCS];

static Window *window;
static GRect window_frame;
static Layer *disc_layer;
static AppTimer *timer;

static GBitmap *creature_bitmap;
static BitmapLayer *s_bitmap_layer;

// get mass based on size of circle
static double disc_calc_mass(Disc *disc) {
  return MATH_PI * disc->radius * disc->radius * DISC_DENSITY;
}

// initialize a single disc
static void disc_init(Disc *disc) {
  GRect frame = window_frame;
  disc->pos.x = frame.size.w/2;
  disc->pos.y = frame.size.h/2;
  disc->vel.x = 0;
  disc->vel.y = 0;
  disc->radius = 22;
  disc->mass = MATH_PI * 8 * 8 * DISC_DENSITY;
}

// euler integration on force, update velocity
static void disc_apply_force(Disc *disc, Vec2d force) {
  disc->vel.x += force.x / disc->mass;
  disc->vel.y += force.y / disc->mass;
}

// euler integration on acceleration, has dampening
static void disc_apply_accel(Disc *disc, AccelData accel) {
  Vec2d force;
  force.x = accel.x * ACCEL_RATIO;
  force.y = -accel.y * ACCEL_RATIO;
  disc_apply_force(disc, force);
}

// euler integration on acceleration, has dampening
static void disc_update(Disc *disc) {
  const GRect frame = window_frame;
  double e = 0.5;
  
  // bounce x
  if ((disc->pos.x - disc->radius < 0 && disc->vel.x < 0)
    || (disc->pos.x + disc->radius > frame.size.w && disc->vel.x > 0)) {
    disc->vel.x = -disc->vel.x * e;
    
    // vibrate
    if(disc->vel.x > VIBRATE_FACTOR || disc->vel.x < -VIBRATE_FACTOR)
      vibes_short_pulse();
  }
  // bounce y
  if ((disc->pos.y - disc->radius < 0 && disc->vel.y < 0)
    || (disc->pos.y + disc->radius > frame.size.h && disc->vel.y > 0)) {
    disc->vel.y = -disc->vel.y * e;
    
    // vibrate
    if(disc->vel.y > VIBRATE_FACTOR || disc->vel.y < -VIBRATE_FACTOR)
      vibes_short_pulse();
  }
  
  // euler integration
  disc->pos.x += disc->vel.x;
  disc->pos.y += disc->vel.y;
}

// draw the circle
static void disc_draw(GContext *ctx, Disc *disc) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_draw_bitmap_in_rect(ctx, creature_bitmap, 
                               GRect(disc->pos.x, disc->pos.y, 19, 21));
  //graphics_fill_circle(ctx, GPoint(disc->pos.x, disc->pos.y), disc->radius);
}

// draw each circle
static void disc_layer_update_callback(Layer *me, GContext *ctx) {
  for (int i = 0; i < NUM_DISCS; i++) {
    disc_draw(ctx, &discs[i]);
  }
}

// draw each circle
static void timer_callback(void *data) {
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };

  accel_service_peek(&accel);

  for (int i = 0; i < NUM_DISCS; i++) {
    Disc *disc = &discs[i];
    disc_apply_accel(disc, accel);
    disc_update(disc);
  }

  layer_mark_dirty(disc_layer);

  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

// initialize discs and layers
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = window_frame = layer_get_frame(window_layer);

  disc_layer = layer_create(frame);
  layer_set_update_proc(disc_layer, disc_layer_update_callback);
  layer_add_child(window_layer, disc_layer);

  for (int i = 0; i < NUM_DISCS; i++) {
    disc_init(&discs[i]);
  }
  
  creature_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CREATURE_WAKE);
  //s_bitmap_layer = bitmap_layer_create(GRect(5, 5, 48, 48));
  //bitmap_layer_set_bitmap(s_bitmap_layer, creature_bitmap);
    
  //layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));
}

// deinitialize window
static void window_unload(Window *window) {
  layer_destroy(disc_layer);
}


static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  accel_data_service_subscribe(0, NULL);

  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void deinit(void) {
  accel_data_service_unsubscribe();

  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}