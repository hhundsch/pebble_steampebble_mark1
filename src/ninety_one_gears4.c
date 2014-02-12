#include <pebble.h>

//Major props to Pebble Technology & orviwan's 91dub, for providing the base of the code used, thus allowing me to
//concentrate on the design elements needed for my cunning plans.

Window *window;
static Layer *window_layer;
Layer *timeFrame; //this is necessary to frame the digits so that they can be animated with the property animation tool yet be clipped when they move down
                    // outside of the watch frame.

GRect from_rect[4];  //to restore digits to their starting positions properly.

bool isDown[] = {true,true,true,true}; //should start in "down" state, so they can animate up during load.
bool minuteAnimating = false; //state of rapid animation of gears during the 3 second time animations.

int count_down_to = -1; // hmm, this is used in handle_second_tick, to determine how many digits need to be animated.
        // if I revise the code, I'll probably move it into that routine, I don't think it needs to be a global anymore.

int _gearCounter = 1; //this determines what frame of the gear animation we're on. It probably needs an upper bounds check somewhere, since in theory it increases indefinitely.

//AppTimerHandle timer_handle; //to hold the animation timer for the gears updating 10 times per second.
AppTimer *timer_handle;
static void handle_timer(void* data);

//BmpContainer *background_image;
GBitmap *background_image;
static BitmapLayer *background_imagelayer;

// BmpContainer meter_bar_image;    //from 91dub, currently not doing this.
// BmpContainer time_format_image;  //from 91dub, currently not doing this

const int DAY_NAME_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DAY_NAME_SUN,
  RESOURCE_ID_IMAGE_DAY_NAME_MON,
  RESOURCE_ID_IMAGE_DAY_NAME_TUE,
  RESOURCE_ID_IMAGE_DAY_NAME_WED,
  RESOURCE_ID_IMAGE_DAY_NAME_THU,
  RESOURCE_ID_IMAGE_DAY_NAME_FRI,
  RESOURCE_ID_IMAGE_DAY_NAME_SAT
};

//BmpContainer day_name_image;
GBitmap *day_name_image;
static BitmapLayer *day_name_imagelayer;

const int DATENUM_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DATENUM_0,
  RESOURCE_ID_IMAGE_DATENUM_1,
  RESOURCE_ID_IMAGE_DATENUM_2,
  RESOURCE_ID_IMAGE_DATENUM_3,
  RESOURCE_ID_IMAGE_DATENUM_4,
  RESOURCE_ID_IMAGE_DATENUM_5,
  RESOURCE_ID_IMAGE_DATENUM_6,
  RESOURCE_ID_IMAGE_DATENUM_7,
  RESOURCE_ID_IMAGE_DATENUM_8,
  RESOURCE_ID_IMAGE_DATENUM_9
};


#define TOTAL_DATE_DIGITS 2
//BmpContainer date_digits_images[TOTAL_DATE_DIGITS];
GBitmap *date_digits_images[TOTAL_DATE_DIGITS];
static BitmapLayer *date_digits_imageslayer[TOTAL_DATE_DIGITS];

const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] = {
    RESOURCE_ID_IMAGE_NUM_0,
    RESOURCE_ID_IMAGE_NUM_1,
    RESOURCE_ID_IMAGE_NUM_2,
    RESOURCE_ID_IMAGE_NUM_3,
    RESOURCE_ID_IMAGE_NUM_4,
    RESOURCE_ID_IMAGE_NUM_5,
    RESOURCE_ID_IMAGE_NUM_6,
    RESOURCE_ID_IMAGE_NUM_7,
    RESOURCE_ID_IMAGE_NUM_8,
    RESOURCE_ID_IMAGE_NUM_9
};

#define TOTAL_TIME_DIGITS 4
//BmpContainer time_digits_images[TOTAL_TIME_DIGITS];
GBitmap *time_digits_images[TOTAL_TIME_DIGITS];
static BitmapLayer *time_digits_imageslayer[TOTAL_TIME_DIGITS];

PropertyAnimation *digit_animations[TOTAL_TIME_DIGITS];  //4 animations, 1 per digit, since they update at different rates.

const int GEAR_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_GEAR_0,
    RESOURCE_ID_IMAGE_GEAR_1,
    RESOURCE_ID_IMAGE_GEAR_2,
    RESOURCE_ID_IMAGE_GEAR_3,
    RESOURCE_ID_IMAGE_GEAR_4,
    RESOURCE_ID_IMAGE_GEAR_5,
    RESOURCE_ID_IMAGE_GEAR_6,
    RESOURCE_ID_IMAGE_GEAR_7,
    RESOURCE_ID_IMAGE_GEAR_8,
    RESOURCE_ID_IMAGE_GEAR_9,
    RESOURCE_ID_IMAGE_GEAR_10,
    RESOURCE_ID_IMAGE_GEAR_11,
    RESOURCE_ID_IMAGE_GEAR_12,
    RESOURCE_ID_IMAGE_GEAR_13,
    RESOURCE_ID_IMAGE_GEAR_14
};
//BmpContainer gear_image;
GBitmap *gear_image;
static BitmapLayer *gear_imagelayer;

//
// Main image setting routine.
// Used for every image swap in the app, the gears, the time digits, the day of week, the date
//

void on_animation_stopped(Animation *anim, bool finished, void *context) {
  //Free the memory used by the Animation
  property_animation_destroy((PropertyAnimation*) anim);
}

void set_container_image(BitmapLayer *bmp_container, const int resource_id, GPoint origin, Layer *targetLayer) {
  const GBitmap *me;

  layer_remove_from_parent((Layer *)bmp_container);                            //remove it from layer so it can be safely deinited

  //bmp_deinit_container(bmp_container);                              //deinit the old image.
  me = bitmap_layer_get_bitmap(bmp_container);
  gbitmap_destroy((GBitmap *)me);

  //bmp_init_container(resource_id, bmp_container);                   //init the container with the new image
  me = gbitmap_create_with_resource(resource_id);
	
  GRect frame = layer_get_frame((Layer *)bmp_container);       //posiiton the new image with the supplied coordinates.
  frame.origin.x = origin.x;
  frame.origin.y = origin.y;

  layer_destroy((Layer *)bmp_container);
  //layer_set_frame(&bmp_container->layer.layer, frame);
  bmp_container = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(bmp_container, me);

  layer_add_child(targetLayer, (Layer *)bmp_container);        //add the new image to the target layer.
}


//
// Get Display Hour.
// turn the display hour into the digits we actually want to display.
//
unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;
}

//
// Update Display
// This, called at the beginning, then updated once a minute, changes the display elements into their new images.
// Some weird provision needed to be made for the main clock digits, which have two different possible positions depending on
// whether or not they are currently up or down. (inline conditions on the isDown[] boolean array are used to determine this.
//

void update_display(struct tm *current_time) {
  //window_layer = window_get_root_layer(window);
  // TODO: Only update changed values?

  set_container_image(day_name_imagelayer, DAY_NAME_IMAGE_RESOURCE_IDS[current_time->tm_wday], GPoint(65, 64), window_layer);

  // TODO: Remove leading zero?
  set_container_image(date_digits_imageslayer[0], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday/10], GPoint(101, 64), window_layer);
  set_container_image(date_digits_imageslayer[1], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday%10], GPoint(109, 64), window_layer);
	
  unsigned short display_hour = get_display_hour(current_time->tm_hour);

  // TODO: Remove leading zero?
  if (display_hour/10)
  {
    set_container_image(time_digits_imageslayer[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour/10], isDown[0] ? GPoint(2, 50) : GPoint(2,0), timeFrame);
  }
  else
  {
	set_container_image(time_digits_imageslayer[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour/10], GPoint(2,50), timeFrame);
  }
  set_container_image(time_digits_imageslayer[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour%10], isDown[1] ? GPoint(30, 50) : GPoint(30,0), timeFrame);

  set_container_image(time_digits_imageslayer[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min/10], isDown[2] ? GPoint(67, 50) : GPoint(67,0), timeFrame);
  set_container_image(time_digits_imageslayer[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min%10], isDown[3] ? GPoint(95, 50) : GPoint(95,0), timeFrame); 
}

//
// Handle timer
// Animates the rapid motion of the gears. (when it's just one gear movement per second, that's handled in handle_second_tick)
//

#define COOKIE_MY_TIMER 1

void handle_timer(void *data) {
	//Layer *window_layer = window_get_root_layer(window);
    uint32_t cookie = (uint32_t) data;
    if (cookie == COOKIE_MY_TIMER) {
        //update the gear to the next frame of animation.
        _gearCounter++;
        set_container_image(gear_imagelayer, GEAR_IMAGE_RESOURCE_IDS[_gearCounter%15], GPoint(9, 9), window_layer);
    }
    
    if(_gearCounter < 300 || minuteAnimating) {
        //if we're in the first 30 seconds, keep the animation moving fast OR
        //if it's during the animation of the minute digits, keep the animation moving fast.
        timer_handle = app_timer_register(100 /* milliseconds */, handle_timer, (void *) COOKIE_MY_TIMER);
    }
}

//
// Handle Second Tick
// The main update happens here. Called once a second.
// 

void handle_second_tick(struct tm *t, TimeUnits tu) {
    //Layer *window_layer = window_get_root_layer(window);
    unsigned short display_second = t->tm_sec;
    
    //  bmp_init_container(RESOURCE_ID_IMAGE_METER_BAR, &meter_bar_image);
    
    // meter_bar_image.layer.layer.frame.origin.x = 9+(display_second*2);  // move the meter bar as a second hand. 
    // layer_set_hidden(&(meter_bar_image.layer.layer),false);
    // layer_mark_dirty(&(meter_bar_image.layer.layer));
    
    //set_container_image(&meter_bar_image,RESOURCE_ID_IMAGE_METER_BAR,GPoint(77-display_second,43));
    
    if(_gearCounter > 299 || !minuteAnimating) {
        //if we're not doing a rapid gear animation, we should still update once per second.
        //removing this would save some battery life, I imagine.
        _gearCounter++;
        set_container_image(gear_imagelayer, GEAR_IMAGE_RESOURCE_IDS[_gearCounter%15], GPoint(9, 9), window_layer);
    }
	
    if(display_second==58)
    {
        unsigned short display_hour = get_display_hour(t->tm_hour);
        
        //figure out how many digits will be updating in 2 seconds.
        count_down_to = 3;
        if (t->tm_min%10 == 9)
        {
            count_down_to = 2;
            if (t->tm_min/10 == 5)
            {
                count_down_to = 1;
                if (display_hour==9 || display_hour==19 || display_hour==23)
                {
                    count_down_to = 0;
                }
                if (display_hour==12 && !clock_is_24h_style())
                {
                    count_down_to = 0;
                }
            }
        }

        //in 2 seconds, at least one digit will be changing. We animate all the digits that will be updating off of the bottom thier layer's frame.
        animation_unschedule_all(); //just in case. This isn't likely to occur, but could happen if the app is launched near minute transition
        minuteAnimating = true; //spin the wheels while moving the digitis.

        if (_gearCounter>299)
        {
            //we're not doing the initial quick animation anymore, so we have to start our own.
            timer_handle = app_timer_register(100 /* milliseconds */, handle_timer, (void *)COOKIE_MY_TIMER);
        }

        for(int i=3;i>=count_down_to;i--)
        {
            //for each digit that's going to be changing.
            isDown[i] = true;  //mark it as down so that updateImage knows to redraw new digit outside the layer frame.
            
            GRect to_rect = GRect(0, 0, 0, 0);
            to_rect = from_rect[i]; //what's its base position?
            to_rect.origin.y += 50; //we want to move it down 50 pixels, to get it outside the frame.
            
            //set up and start the animation.
            //property_animation_init_layer_frame(&digit_animations[i], &time_digits_images[i].layer.layer, NULL, &to_rect);
			//property_animation_destroy(digit_animations[i]);
			digit_animations[i] = property_animation_create_layer_frame((Layer *)time_digits_imageslayer[i], NULL, &to_rect);
            animation_set_duration((Animation *) digit_animations[i], 1750-(250*i));
            animation_set_curve((Animation *) digit_animations[i],AnimationCurveEaseIn);
			
			AnimationHandlers handlers = {
            	.stopped = (AnimationStoppedHandler) on_animation_stopped
            };
            animation_set_handlers((Animation*) digit_animations[i], handlers, NULL);

            animation_schedule((Animation *) digit_animations[i]);
        }
    }

	if(display_second==0) {
      update_display(t); //we call this rather than having the OS do so, so we can control exactly when it's going to happen.
      animation_unschedule_all(); //just in case. This isn't likely to occur, but could happen if the app is launched near minute transition.
		
	  unsigned short display_hour = get_display_hour(t->tm_hour);
      int enddigit = 1;
      if (display_hour/10) {
        enddigit = 0;
      }			
      //animate the digits back to starting positions!
      for(int i=3;i>=enddigit;i--)
      {
        if(isDown[i]) {
          //if we put it down, so set up and start the animation to get it back up.
	      digit_animations[i] = property_animation_create_layer_frame((Layer *)time_digits_imageslayer[i], NULL, &from_rect[i]);
          animation_set_duration((Animation *)digit_animations[i], 1250-(125*i));
          animation_set_curve((Animation *)digit_animations[i],AnimationCurveEaseIn);
		
	      AnimationHandlers handlers = {
            	.stopped = (AnimationStoppedHandler) on_animation_stopped
          };
          animation_set_handlers((Animation*) digit_animations[i], handlers, NULL);

          animation_schedule((Animation *)digit_animations[i]);
          isDown[i] = false;
        }
      }
    }
	
    if(display_second==1) {
        //stop the gears spinning fast, and go back to 1 update per second.
        minuteAnimating = false;
    }
} //end handle_second_tick

void window_load(Window *window){
  window_layer = window_get_root_layer(window);
	
  //bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image);
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  BitmapLayer *background_image_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(background_image_layer, background_image);
  layer_add_child(window_layer, (Layer *) background_image_layer);
	
  day_name_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DAY_NAME_SUN);
  day_name_imagelayer = bitmap_layer_create(GRect(65, 64, 34, 15));
  bitmap_layer_set_bitmap(day_name_imagelayer, day_name_image);
  layer_add_child(window_layer, (Layer *) day_name_imagelayer);
	
  for (int i = 0; i < TOTAL_DATE_DIGITS; i++) {
    //bmp_deinit_container(&date_digits_images[i]);
    date_digits_images[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DATENUM_0);
    date_digits_imageslayer[i] = bitmap_layer_create(GRect(101, 64, 10, 15));
  }
  layer_add_child(window_layer, (Layer *) date_digits_imageslayer[0]);
	
  //bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image);
  gear_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GEAR_0);
  gear_imagelayer = bitmap_layer_create(GRect(9, 9, 125, 50));
  bitmap_layer_set_bitmap(gear_imagelayer, gear_image);
  layer_add_child(window_layer, (Layer *) gear_imagelayer);

  //bmp_deinit_container(&time_digits_images[i]);
  for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
    time_digits_images[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NUM_0); 
  }
  
  time_digits_imageslayer[0] = bitmap_layer_create(GRect(2, 50, 27, 50));
  time_digits_imageslayer[1] = bitmap_layer_create(GRect(30, 50, 27, 50));
  time_digits_imageslayer[2] = bitmap_layer_create(GRect(67, 50, 27, 50));
  time_digits_imageslayer[3] = bitmap_layer_create(GRect(95, 50, 27, 50));

  //layer_init(&timeFrame, GRect(9, 82, 125, 50)); //clipping region for big digits.
  //layer_add_child(window_layer, timeFrame); //clipping region for time numbers
  timeFrame = layer_create(GRect(9, 82, 125, 50)); //clipping region for big digits.
  for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
	bitmap_layer_set_bitmap(time_digits_imageslayer[i], time_digits_images[i]);
    layer_add_child(timeFrame, (Layer *) time_digits_imageslayer[i]);
  }
  layer_set_clips(timeFrame, true);
  layer_add_child(window_layer, timeFrame); //clipping region for time numbers

  // Avoids a blank screen on watch start.
  struct tm *tick_time;
  time_t temp = time(NULL);
  tick_time = localtime(&temp);
	
  update_display(tick_time);
	
  unsigned short display_hour = get_display_hour(tick_time->tm_hour);
  int enddigit = 1;
  if (display_hour/10) {
    enddigit = 0;
    isDown[0] = true;
  }	
	//start by animating 3 or 4 digits up from the bottom of the display, slower than we do later on for dramatic effect.
    for(int i=3;i>=enddigit;i--)
    {
        from_rect[i] = layer_get_frame((Layer *)time_digits_imageslayer[i]);
	    from_rect[i].origin.y-=50;
		
        //property_animation_init_layer_frame(&digit_animations[i], &time_digits_images[i].layer.layer, NULL, &from_rect[i]);
		//digit_animations[i] = animation_create();
		digit_animations[i] = property_animation_create_layer_frame((Layer *)time_digits_imageslayer[i], NULL, &from_rect[i]);
		
        animation_set_duration((Animation *) digit_animations[i], 2500-(400*i));
        animation_set_curve((Animation *) digit_animations[i],AnimationCurveEaseIn);
		
		AnimationHandlers handlers = {
           .stopped = (AnimationStoppedHandler) on_animation_stopped
        };
        animation_set_handlers((Animation*) digit_animations[i], handlers, NULL);
		
        animation_schedule((Animation *) digit_animations[i]);
        isDown[i] = false;
    }
}

void window_unload(Window *window){
	//Destroy resources
	//Like a good developer would
  
  layer_destroy(timeFrame);
	
  //bmp_deinit_container(&background_image);	
  gbitmap_destroy(background_image);
  bitmap_layer_destroy(background_imagelayer);
	
  //bmp_deinit_container(&meter_bar_image);
  //bmp_deinit_container(&time_format_image);
  //bmp_deinit_container(&day_name_image);
  gbitmap_destroy(day_name_image);
  bitmap_layer_destroy(day_name_imagelayer);
  //bmp_deinit_container(&gear_image);
  gbitmap_destroy(gear_image);
  bitmap_layer_destroy(gear_imagelayer);
    
  for (int i = 0; i < TOTAL_DATE_DIGITS; i++) {
    //bmp_deinit_container(&date_digits_images[i]);
    gbitmap_destroy(date_digits_images[i]);
    bitmap_layer_destroy(date_digits_imageslayer[i]);
  }

  for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
    //bmp_deinit_container(&time_digits_images[i]);
    gbitmap_destroy(time_digits_images[i]);
    bitmap_layer_destroy(time_digits_imageslayer[i]);
  }
}

void handle_init(void) {
  window = window_create();
	
  window_set_window_handlers(window, (WindowHandlers) {
  		.load = window_load,
  		.unload = window_unload,
      });

  tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) handle_second_tick);
	
  //window_init(&window, "91 Gears");
  //window_stack_push(&window, true /* Animated */);
  window_stack_push(window, true);

  //timer_handle = app_timer_send_event(ctx, 100 /* milliseconds */, COOKIE_MY_TIMER);
  timer_handle = app_timer_register(100 /* milliseconds */, handle_timer, (void *)COOKIE_MY_TIMER);
} //end handle_init


void handle_deinit(void) {
  tick_timer_service_unsubscribe();
  animation_unschedule_all();
  window_destroy(window);
}


int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}