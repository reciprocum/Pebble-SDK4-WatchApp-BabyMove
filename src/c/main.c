/*
   WatchApp: Baby move counter
   File    : main.c
   Author  : Afonso Santos, Portugal

   Last revision: 25 October 2016
*/

#include "pebble.h"

#define SECONDS_INACTIVE_MAX 15
#define MOVS_TARGET          10
#define CUTOFF_HH            21

#define HEADER_YOFFSET_PCT      3
#define MOVS_YOFFSET_PCT        19
#define TARGET_YOFFSET_PCT      50
#define CUTOFF_YOFFSET_PCT      75

// Defaults for values in persistent storage
#define MOVS_DEFAULT       0
#define TARGET_HH_DEFAULT  0
#define TARGET_MM_DEFAULT  0
#define CUTOFF_MOVS_INIT  -1

// This is a custom defined key for saving our count field
#define PKEY_MOVS         1
#define PKEY_TARGET_HH    2
#define PKEY_TARGET_MM    3
#define PKEY_CUTOFF_MOVS  4

static Window  *s_window ;
static Layer   *s_window_layer ;

static GBitmap *s_action_icon_plus ;
static GBitmap *s_action_icon_zero ;
static GBitmap *s_action_icon_minus ;

static ActionBarLayer *s_action_bar_layer ;

static TextLayer *s_header_text_layer ;
static Layer     *s_header_layer ;

static TextLayer *s_movs_text_layer ;
static Layer     *s_movs_layer ;

static TextLayer *s_target_text_layer ;
static Layer     *s_target_layer ;

static TextLayer *s_cutoff_text_layer ;
static Layer     *s_cutoff_layer ;

static uint8_t  s_secondsInactive = 0 ;

// Persisted vars.
static int s_movs        ;
static int s_cutoff_movs ;
static int s_target_HH   ;
static int s_target_MM   ;


GSize unobstructed_screen ;

int16_t
percentOf( const int16_t pct, const int16_t max )
{
  return (max * pct) / 100 ;
}


void
unobstructed_area_change_handler
( AnimationProgress progress
, void             *context
)
{
  unobstructed_screen = layer_get_unobstructed_bounds( s_window_layer ).size ;

  GRect header_frame = layer_get_frame( s_header_layer ) ;
  header_frame.origin.y = percentOf( HEADER_YOFFSET_PCT, unobstructed_screen.h ) ;
  layer_set_frame( s_header_layer, header_frame ) ;

  GRect movs_frame = layer_get_frame( s_movs_layer ) ;
  movs_frame.origin.y = percentOf( MOVS_YOFFSET_PCT, unobstructed_screen.h ) ;
  layer_set_frame( s_movs_layer, movs_frame ) ;

  GRect target_frame = layer_get_frame( s_target_layer ) ;
  target_frame.origin.y = percentOf( TARGET_YOFFSET_PCT, unobstructed_screen.h ) ;
  layer_set_frame( s_target_layer, target_frame ) ;

  GRect cutoff_frame = layer_get_frame( s_cutoff_layer ) ;
  cutoff_frame.origin.y = percentOf( CUTOFF_YOFFSET_PCT, unobstructed_screen.h ) ;
  layer_set_frame( s_cutoff_layer, cutoff_frame ) ;
}
  

void
update_text
( )
{
  static char movs_text[50] ;
  snprintf( movs_text, sizeof(movs_text), "%u", s_movs ) ;
  text_layer_set_text( s_movs_text_layer, movs_text ) ;

  if (s_movs < MOVS_TARGET)
    text_layer_set_text( s_target_text_layer, "" ) ;
  else
  {
    static char target_text[50] ;

    snprintf( target_text, sizeof(target_text), "%u: @%uh%02u", MOVS_TARGET, s_target_HH, s_target_MM ) ;
    text_layer_set_text( s_target_text_layer, target_text ) ;
  }

  if (s_cutoff_movs == CUTOFF_MOVS_INIT)
    text_layer_set_text( s_cutoff_text_layer, "" ) ;
  else
  {
    static char cutoff_text[50] ;

    snprintf( cutoff_text, sizeof(cutoff_text), "@%uh: %u", CUTOFF_HH, s_cutoff_movs ) ;
    text_layer_set_text( s_cutoff_text_layer, cutoff_text ) ;
  }
}


void
tick_timer_service_handler
( struct tm *tick_time
, TimeUnits  units_changed
)
{
  if (++s_secondsInactive > SECONDS_INACTIVE_MAX)
  {
    window_stack_pop_all( false )	;
    exit_reason_set( APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY ) ;
  }

  if ((s_cutoff_movs == CUTOFF_MOVS_INIT) && (tick_time->tm_hour >= CUTOFF_HH))
  {
    s_cutoff_movs = s_movs ;
    update_text( ) ;
  }
}


void
increment_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  s_secondsInactive = 0 ;

  time_t now ;
  time( &now ) ;
  struct tm *local_time = localtime( &now ) ;

  if (++s_movs == MOVS_TARGET)
  {
    s_target_HH = local_time->tm_hour ;
    s_target_MM = local_time->tm_min ;
  }

  if ((s_cutoff_movs == CUTOFF_MOVS_INIT) && (local_time->tm_hour >= CUTOFF_HH))
    s_cutoff_movs = s_movs ;

  update_text( ) ;
}


void
reset_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  s_secondsInactive = 0 ;
  s_movs        = MOVS_DEFAULT ;
  s_cutoff_movs = CUTOFF_MOVS_INIT ;
  s_target_HH   = TARGET_HH_DEFAULT ;
  s_target_MM   = TARGET_MM_DEFAULT ;
  update_text( ) ;
}


void
decrement_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  s_secondsInactive = 0 ;

  if (s_movs == 0)
    return ;                            // Keep the counter at zero

  --s_movs ;

  if (s_movs < MOVS_TARGET)             // Invalidate previous target timestamp.
  {
    s_target_HH = TARGET_HH_DEFAULT ;
    s_target_MM = TARGET_MM_DEFAULT ;
  }

  if (s_movs < s_cutoff_movs)
    s_cutoff_movs = s_movs ;            // Invalidate previous cuttoff value.

  update_text( ) ;
}


// Tap events.
void
accel_tap_service_handler
( AccelAxisType  axis        // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
, int32_t        direction   // Direction is 1 or -1
)
{
  s_secondsInactive = 0 ;

  switch ( axis )
  {
    case ACCEL_AXIS_X:    // Punch: 
    case ACCEL_AXIS_Y:    // Twist: ++counter.
      ++s_movs ;
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
{
  const uint16_t repeat_interval_ms = 250 ;
  window_single_repeating_click_subscribe( BUTTON_ID_UP    , repeat_interval_ms, (ClickHandler) increment_click_handler ) ;
  window_single_click_subscribe(           BUTTON_ID_SELECT,                     (ClickHandler) reset_click_handler     ) ;
  window_single_repeating_click_subscribe( BUTTON_ID_DOWN  , repeat_interval_ms, (ClickHandler) decrement_click_handler ) ;
}


void
window_load
( Window *s_window )
{
  s_secondsInactive = 0 ;

  s_action_bar_layer = action_bar_layer_create( ) ;
  action_bar_layer_add_to_window( s_action_bar_layer, s_window ) ;
  action_bar_layer_set_click_config_provider( s_action_bar_layer, click_config_provider ) ;

  action_bar_layer_set_icon( s_action_bar_layer, BUTTON_ID_UP    , s_action_icon_plus  ) ;
  action_bar_layer_set_icon( s_action_bar_layer, BUTTON_ID_SELECT, s_action_icon_zero  ) ;
  action_bar_layer_set_icon( s_action_bar_layer, BUTTON_ID_DOWN  , s_action_icon_minus ) ;

  s_window_layer      = window_get_root_layer( s_window ) ;
  unobstructed_screen = layer_get_unobstructed_bounds( s_window_layer ).size ;

  const int16_t width = layer_get_frame(s_window_layer).size.w - ACTION_BAR_WIDTH - 3 ;

  s_header_text_layer = text_layer_create( GRect( PBL_IF_RECT_ELSE(0, 20)
                                                , percentOf( HEADER_YOFFSET_PCT, unobstructed_screen.h )
                                                , width
                                                , 20
                                                )
                                         ) ;
  text_layer_set_text_alignment( s_header_text_layer, GTextAlignmentCenter ) ;
  text_layer_set_font( s_header_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18)) ;
  text_layer_set_background_color( s_header_text_layer, GColorClear) ;
  text_layer_set_text( s_header_text_layer, PBL_IF_RECT_ELSE("Baby movements","Baby moves") ) ;

  s_movs_text_layer = text_layer_create( GRect( PBL_IF_RECT_ELSE(0, 10)
                                              , percentOf( MOVS_YOFFSET_PCT, unobstructed_screen.h )
                                              , width
                                              , 45
                                              )
                                       ) ;
  text_layer_set_text_alignment( s_movs_text_layer, GTextAlignmentCenter ) ;
  text_layer_set_font( s_movs_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
  text_layer_set_background_color( s_movs_text_layer, GColorClear);

  s_target_text_layer = text_layer_create( GRect( 0
                                                , percentOf( TARGET_YOFFSET_PCT, unobstructed_screen.h )
                                                , width
                                                , 25
                                                )
                                         ) ;
  text_layer_set_text_alignment( s_target_text_layer, GTextAlignmentCenter ) ;
  text_layer_set_font( s_target_text_layer, fonts_get_system_font( FONT_KEY_ROBOTO_CONDENSED_21 ) );
  text_layer_set_background_color( s_target_text_layer, GColorClear );

  s_cutoff_text_layer = text_layer_create( GRect( PBL_IF_RECT_ELSE(0, 20)
                                                , percentOf( CUTOFF_YOFFSET_PCT, unobstructed_screen.h )
                                                , width
                                                , 25
                                                )
                                         ) ;
  text_layer_set_text_alignment( s_cutoff_text_layer, GTextAlignmentCenter ) ;
  text_layer_set_font( s_cutoff_text_layer, fonts_get_system_font( FONT_KEY_ROBOTO_CONDENSED_21 ) );
  text_layer_set_background_color( s_cutoff_text_layer, GColorClear );

  // Add the layers to the main window layer.
  layer_add_child( s_window_layer, s_header_layer = text_layer_get_layer( s_header_text_layer ) ) ;
  layer_add_child( s_window_layer, s_movs_layer   = text_layer_get_layer( s_movs_text_layer   ) ) ;
  layer_add_child( s_window_layer, s_target_layer = text_layer_get_layer( s_target_text_layer ) ) ;
  layer_add_child( s_window_layer, s_cutoff_layer = text_layer_get_layer( s_cutoff_text_layer ) ) ;

  // Get the count from persistent storage for use if it exists, otherwise use the default
  s_movs        = persist_exists(PKEY_MOVS)        ? persist_read_int(PKEY_MOVS)        : MOVS_DEFAULT      ;
  s_target_HH   = persist_exists(PKEY_TARGET_HH)   ? persist_read_int(PKEY_TARGET_HH)   : TARGET_HH_DEFAULT ;
  s_target_MM   = persist_exists(PKEY_TARGET_MM)   ? persist_read_int(PKEY_TARGET_MM)   : TARGET_MM_DEFAULT ;
  s_cutoff_movs = persist_exists(PKEY_CUTOFF_MOVS) ? persist_read_int(PKEY_CUTOFF_MOVS) : CUTOFF_MOVS_INIT  ;

  update_text( ) ;

  // Obstrution handling.
  UnobstructedAreaHandlers unobstructed_area_handlers = { .change = unobstructed_area_change_handler } ;
  unobstructed_area_service_subscribe( unobstructed_area_handlers, NULL ) ;

  tick_timer_service_subscribe( SECOND_UNIT, tick_timer_service_handler ) ;
  accel_tap_service_subscribe( accel_tap_service_handler ) ;
}


void
window_unload
( Window *s_window )
{ // Save the count into persistent storage on app exit.
  persist_write_int( PKEY_MOVS       , s_movs        ) ;
  persist_write_int( PKEY_TARGET_HH  , s_target_HH   ) ;
  persist_write_int( PKEY_TARGET_MM  , s_target_MM   ) ;
  persist_write_int( PKEY_CUTOFF_MOVS, s_cutoff_movs ) ;

  // Unsubscribe services.
  accel_tap_service_unsubscribe( ) ;
  tick_timer_service_unsubscribe( ) ;
  unobstructed_area_service_unsubscribe( ) ;

  // Destroy layers.
  text_layer_destroy( s_header_text_layer ) ;
  text_layer_destroy( s_movs_text_layer ) ;
  text_layer_destroy( s_target_text_layer ) ;
  text_layer_destroy( s_cutoff_text_layer ) ;

  action_bar_layer_destroy( s_action_bar_layer ) ;
}


void
app_init
( void )
{
  s_action_icon_plus  = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_ACTION_ICON_PLUS ) ;
  s_action_icon_zero  = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_ACTION_ICON_ZERO ) ;
  s_action_icon_minus = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_ACTION_ICON_MINUS ) ;

  s_window = window_create( ) ;
 
  window_set_window_handlers( s_window
                            , (WindowHandlers)
                              { .load   = window_load
                              , .unload = window_unload
                              }
                            ) ;

  window_stack_push( s_window, true /* Animated */) ;
}


void
app_deinit
( void )
{
  window_stack_remove( s_window, false ) ;
  window_destroy( s_window ) ;
  gbitmap_destroy( s_action_icon_plus ) ;
  gbitmap_destroy( s_action_icon_zero ) ;
  gbitmap_destroy( s_action_icon_minus ) ;
}


int
main
( void )
{
  app_init( ) ;
  app_event_loop( ) ;
  app_deinit( ) ;
}