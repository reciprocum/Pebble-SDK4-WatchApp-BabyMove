#include "pebble.h"

#define SECONDS_INACTIVE_MAX 15
#define MOVS_TARGET          10
#define CUTOFF_HH            21

// This is a custom defined key for saving our count field
#define PKEY_MOVS         1
#define PKEY_TARGET_HH    2
#define PKEY_TARGET_MM    3
#define PKEY_CUTOFF_MOVS  4

// Defaults for values in persistent storage
#define MOVS_DEFAULT       0
#define TARGET_HH_DEFAULT  0
#define TARGET_MM_DEFAULT  0
#define CUTOFF_MOVS_INIT  -1

static Window *window ;

static GBitmap *action_icon_plus ;
static GBitmap *action_icon_zero ;
static GBitmap *action_icon_minus ;

static ActionBarLayer *action_bar ;

static TextLayer *header_text_layer ;
static TextLayer *movs_text_layer ;
static TextLayer *target_text_layer ;
static TextLayer *cutoff_text_layer ;

static uint8_t  secondsInactive = 0 ;

// Persisted vars.
static int movs        ;
static int cutoff_movs ;
static int target_HH   ;
static int target_MM   ;


void
update_text
( )
{ static char movs_text[50] ;
  snprintf( movs_text, sizeof(movs_text), "%u Movements", movs ) ;
  text_layer_set_text( movs_text_layer, movs_text ) ;

  if (movs < MOVS_TARGET)
    text_layer_set_text( target_text_layer, "" ) ;
  else
  { static char target_text[50] ;

    snprintf( target_text, sizeof(target_text), "#%u: %uh%u", MOVS_TARGET, target_HH, target_MM ) ;
    text_layer_set_text( target_text_layer, target_text ) ;
  }

  if (cutoff_movs == CUTOFF_MOVS_INIT)
    text_layer_set_text( cutoff_text_layer, "" ) ;
  else
  { static char cutoff_text[50] ;

    snprintf( cutoff_text, sizeof(cutoff_text), "@%uh: #%u", CUTOFF_HH, cutoff_movs ) ;
    text_layer_set_text( cutoff_text_layer, cutoff_text ) ;
  }
}


void
tick_timer_service_handler
( struct tm *tick_time
, TimeUnits  units_changed
)
{ if (++secondsInactive > SECONDS_INACTIVE_MAX)  window_stack_pop_all( false )	;

  if ((cutoff_movs == CUTOFF_MOVS_INIT) && (tick_time->tm_hour >= CUTOFF_HH))
  { cutoff_movs = movs ;
    update_text( ) ;
  }
}


void
increment_click_handler
( ClickRecognizerRef recognizer, void *context )
{ secondsInactive = 0 ;

  time_t now ;
  time( &now ) ;
  struct tm *local_time = localtime( &now ) ;

  if ( ++movs == MOVS_TARGET )
  { target_HH = local_time->tm_hour ;
    target_MM = local_time->tm_min ;
  }

  if ((cutoff_movs == CUTOFF_MOVS_INIT) && (local_time->tm_hour >= CUTOFF_HH)) cutoff_movs = movs ;

  update_text( ) ;
}


void
reset_click_handler
( ClickRecognizerRef recognizer, void *context )
{ secondsInactive = 0 ;
  movs        = MOVS_DEFAULT ;
  cutoff_movs = CUTOFF_MOVS_INIT ;
  target_HH   = TARGET_HH_DEFAULT ;
  target_MM   = TARGET_MM_DEFAULT ;
  update_text( ) ;
}


void
decrement_click_handler
( ClickRecognizerRef recognizer, void *context )
{ secondsInactive = 0 ;
  if (movs == 0) return ;                              // Keep the counter at zero

  --movs ;

  if (movs < MOVS_TARGET)                          // Invalidate previous target timestamp.
  { target_HH = TARGET_HH_DEFAULT ;
    target_MM = TARGET_MM_DEFAULT ;
  }

  if (movs < cutoff_movs) cutoff_movs = movs ;     // Invalidate previous cuttoff value.

  update_text( ) ;
}


// Tap events.
void
accel_tap_service_handler
( AccelAxisType  axis        // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
, int32_t        direction   // Direction is 1 or -1
)
{ secondsInactive = 0 ;

  switch ( axis )
  { case ACCEL_AXIS_X:    // Punch: 
    case ACCEL_AXIS_Y:    // Twist: ++counter.
      ++movs ;
      update_text( ) ;
      break ;

    case ACCEL_AXIS_Z:
      window_stack_pop_all(	false )	;
      break ;

   default:
      break ;
  }
}


void
click_config_provider
( void *context )
{ const uint16_t repeat_interval_ms = 250 ;
  window_single_repeating_click_subscribe( BUTTON_ID_UP    , repeat_interval_ms, (ClickHandler) increment_click_handler ) ;
  window_single_click_subscribe(           BUTTON_ID_SELECT,                     (ClickHandler) reset_click_handler     ) ;
  window_single_repeating_click_subscribe( BUTTON_ID_DOWN  , repeat_interval_ms, (ClickHandler) decrement_click_handler ) ;
}


void
window_load
( Window *window )
{ secondsInactive = 0 ;

  action_bar = action_bar_layer_create( ) ;
  action_bar_layer_add_to_window( action_bar, window ) ;
  action_bar_layer_set_click_config_provider( action_bar, click_config_provider ) ;

  action_bar_layer_set_icon( action_bar, BUTTON_ID_UP    , action_icon_plus  ) ;
  action_bar_layer_set_icon( action_bar, BUTTON_ID_SELECT, action_icon_zero  ) ;
  action_bar_layer_set_icon( action_bar, BUTTON_ID_DOWN  , action_icon_minus ) ;

  Layer *window_root_layer = window_get_root_layer( window ) ;
  const int16_t width = layer_get_frame(window_root_layer).size.w - ACTION_BAR_WIDTH - 3 ;

  header_text_layer = text_layer_create( GRect(4, 0, width, 20) ) ;
  text_layer_set_font(header_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(header_text_layer, GColorClear);
  text_layer_set_text(header_text_layer, "Move counter");
  layer_add_child(window_root_layer, text_layer_get_layer(header_text_layer));

  movs_text_layer = text_layer_create(GRect(4, 30, width, 60));
  text_layer_set_font(movs_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(movs_text_layer, GColorClear);
  layer_add_child(window_root_layer, text_layer_get_layer(movs_text_layer));

  target_text_layer = text_layer_create(GRect(4, 95, width, 25));
  text_layer_set_font( target_text_layer, fonts_get_system_font( FONT_KEY_ROBOTO_CONDENSED_21 ) );
  text_layer_set_background_color( target_text_layer, GColorClear );
  layer_add_child(window_root_layer, text_layer_get_layer(target_text_layer));

  cutoff_text_layer = text_layer_create(GRect(4, 125, width, 25));
  text_layer_set_font( cutoff_text_layer, fonts_get_system_font( FONT_KEY_ROBOTO_CONDENSED_21 ) );
  text_layer_set_background_color( cutoff_text_layer, GColorClear );
  layer_add_child(window_root_layer, text_layer_get_layer(cutoff_text_layer));

  // Get the count from persistent storage for use if it exists, otherwise use the default
  movs        = persist_exists(PKEY_MOVS)        ? persist_read_int(PKEY_MOVS)        : MOVS_DEFAULT      ;
  target_HH   = persist_exists(PKEY_TARGET_HH)   ? persist_read_int(PKEY_TARGET_HH)   : TARGET_HH_DEFAULT ;
  target_MM   = persist_exists(PKEY_TARGET_MM)   ? persist_read_int(PKEY_TARGET_MM)   : TARGET_MM_DEFAULT ;
  cutoff_movs = persist_exists(PKEY_CUTOFF_MOVS) ? persist_read_int(PKEY_CUTOFF_MOVS) : CUTOFF_MOVS_INIT  ;

  update_text( ) ;
  tick_timer_service_subscribe( SECOND_UNIT, tick_timer_service_handler ) ;
  accel_tap_service_subscribe( accel_tap_service_handler ) ;
}


void
window_unload
( Window *window )
{ // Save the count into persistent storage on app exit.
  persist_write_int( PKEY_MOVS       , movs        ) ;
  persist_write_int( PKEY_TARGET_HH  , target_HH   ) ;
  persist_write_int( PKEY_TARGET_MM  , target_MM   ) ;
  persist_write_int( PKEY_CUTOFF_MOVS, cutoff_movs ) ;

  accel_tap_service_unsubscribe( ) ;
  tick_timer_service_unsubscribe( ) ;

  text_layer_destroy( header_text_layer ) ;
  text_layer_destroy( movs_text_layer ) ;
  text_layer_destroy( target_text_layer ) ;
  text_layer_destroy( cutoff_text_layer ) ;

  action_bar_layer_destroy( action_bar ) ;
}


void
app_init
( void )
{ action_icon_plus  = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_ACTION_ICON_PLUS ) ;
  action_icon_zero  = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_ACTION_ICON_ZERO ) ;
  action_icon_minus = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_ACTION_ICON_MINUS ) ;

  window = window_create( ) ;
 
  window_set_window_handlers( window
                            , (WindowHandlers)
                              { .load   = window_load
                              , .unload = window_unload
                              }
                            ) ;

  window_stack_push( window, true /* Animated */) ;
}


void
app_deinit
( void )
{ window_stack_remove( window, false ) ;
  window_destroy( window ) ;
  gbitmap_destroy( action_icon_plus ) ;
  gbitmap_destroy( action_icon_minus ) ;
}


int
main
( void )
{ app_init( ) ;
  app_event_loop( ) ;
  app_deinit( ) ;
}
