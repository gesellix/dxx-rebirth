/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Windowing functions and controller.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "window.h"
#include "u_mem.h"
#include "fix.h"
#include "pstypes.h"
#include "gr.h"
#include "ui.h"
#include "key.h"
#include "mouse.h"
#include "timer.h"
#include "error.h"

#define W_BACKGROUND    (wnd->background )
#define W_X             (wnd->x)
#define W_Y             (wnd->y)
#define W_WIDTH         (wnd->width)
#define W_HEIGHT        (wnd->height)
#define W_OLDCANVAS     (wnd->oldcanvas)
#define W_CANVAS        (wnd->canvas)
#define W_GADGET        (wnd->gadget)
#define W_TEXT_X        (wnd->text_x)
#define W_TEXT_Y        (wnd->text_y)
#define W_NEXT          (wnd->next)
#define W_PREV          (wnd->prev)

#ifndef __MSDOS__
#define _disable()
#define _enable()
#endif

UI_WINDOW * CurWindow = NULL;
UI_WINDOW * FirstWindow = NULL;
UI_WINDOW * LastWindow = NULL;

int last_keypress = 0;

#define BORDER_WIDTH 8

static unsigned int FrameCount = 0;
unsigned int ui_event_counter = 0;
unsigned int ui_number_of_events = 0;
static UI_EVENT *   EventBuffer = NULL;
static int          Record = 0;
static int          RecordFlags = 0;

static short MouseDX=0, MouseDY=0, MouseButtons=0;

static unsigned char SavedState[256];

static int PlaybackSpeed = 1;

extern void ui_draw_frame( short x1, short y1, short x2, short y2 );

// 1=1x faster, 2=2x faster, etc
void ui_set_playback_speed( int speed )
{
	PlaybackSpeed = speed;
}

int ui_record_events( int NumberOfEvents, UI_EVENT * buffer, int Flags )
{
	if ( Record > 0 || buffer==NULL ) return 1;

	RecordFlags = Flags;
	EventBuffer = buffer;
	ui_event_counter = 0;
	FrameCount = 0;
	ui_number_of_events = NumberOfEvents;
	Record = 1;
	return 0;
}

int ui_play_events_realtime( int NumberOfEvents, UI_EVENT * buffer )
{	int i;
	if ( buffer == NULL ) return 1;

	EventBuffer = buffer;
	FrameCount = 0;
	ui_event_counter = 0;
	ui_number_of_events = NumberOfEvents;
	Record = 2;
	_disable();
	keyd_last_released= 0;
	keyd_last_pressed= 0;
	for (i=0; i<256; i++ )
		SavedState[i] = keyd_pressed[i];
	_enable();
	key_flush();
	return 0;
}

int ui_play_events_fast( int NumberOfEvents, UI_EVENT * buffer )
{
	int i;
	if ( buffer == NULL ) return 1;

	EventBuffer = buffer;
	FrameCount = 0;
	ui_event_counter = 0;
	ui_number_of_events = NumberOfEvents;
	Record = 3;
	_disable();
	keyd_last_released= 0;
	keyd_last_pressed= 0;

	for (i=0; i<256; i++ )
	{
		SavedState[i] = keyd_pressed[i];
	}
	_enable();
	key_flush();
	return 0;
}

// Returns:  0=Normal, 1=Recording, 2=Playback normal, 3=Playback fast
int ui_recorder_status()
{
	return Record;
}

void add_window_to_end( UI_WINDOW * wnd )
{
	if (LastWindow) {
		W_PREV = LastWindow;
		LastWindow->next = wnd;
	}
	LastWindow = wnd;
	if (!FirstWindow)
		FirstWindow = wnd;
}

void add_window_to_beg( UI_WINDOW * wnd )
{
	if (FirstWindow) {
		W_NEXT = FirstWindow;
		FirstWindow->prev = wnd;
	}
	FirstWindow = wnd;
	if (!LastWindow)
		LastWindow = wnd;
}

// Add w1 after w2
void add_window_after( UI_WINDOW * w1, UI_WINDOW * w2 )
{
	w1->prev = w2;
	w1->next = w2->next;
	w2->next = w1;
	if (w1->next == NULL )
		LastWindow = w1;
	else
		w1->next->prev = w1;
}

void close_all()
{
	UI_WINDOW *sav, *wnd = LastWindow;

	while(wnd)
	{
		sav = W_PREV;
		ui_close_window(wnd);
		wnd = sav;
	}
}

void remove_window( UI_WINDOW * wnd )
{
	if (W_NEXT)
		W_NEXT->prev = W_PREV;
	if (W_PREV)
		W_PREV->next = W_NEXT;
	if (FirstWindow == wnd )
		FirstWindow = W_NEXT;
	if (LastWindow == wnd )
		LastWindow = W_PREV;
	W_NEXT = W_PREV = NULL;
}



UI_WINDOW * ui_open_window( short x, short y, short w, short h, int flags )
{
	UI_WINDOW * wnd;
	int sw, sh, req_w, req_h;

	wnd = (UI_WINDOW *) d_malloc(sizeof(UI_WINDOW));
	if (wnd==NULL) Error("Could not create window: Out of memory");

	W_NEXT = NULL;
	W_PREV = NULL;

	add_window_to_end( wnd );

	sw = grd_curscreen->sc_w;
	sh = grd_curscreen->sc_h;

	//mouse_set_limits(0, 0, sw - 1, sh - 1);

	req_w = w;
	req_h = h;

	if (flags & WIN_BORDER)
	{
		x -= BORDER_WIDTH;
		y -= BORDER_WIDTH;
		w += 2*BORDER_WIDTH;
		h += 2*BORDER_WIDTH;
	}

	if ( x < 0 ) x = 0;
	if ( (x+w-1) >= sw ) x = sw - w;
	if ( y < 0 ) y = 0;
	if ( (y+h-1) >= sh ) y = sh - h;

	W_X = x;
	W_Y = y;
	W_WIDTH = w;
	W_HEIGHT = h;
	W_OLDCANVAS = grd_curcanv;
	W_GADGET = NULL;
	wnd->keyboard_focus_gadget = NULL;

	ui_mouse_hide();

	if (flags & WIN_SAVE_BG)
	{
		W_BACKGROUND = gr_create_bitmap( w, h );
		gr_bm_ubitblt(w, h, 0, 0, x, y, &(grd_curscreen->sc_canvas.cv_bitmap), W_BACKGROUND );
	}
	else
		W_BACKGROUND = NULL;

	if (flags & WIN_BORDER)
	{
		W_CANVAS = gr_create_sub_canvas( &(grd_curscreen->sc_canvas), x+BORDER_WIDTH, y+BORDER_WIDTH, req_w, req_h );
		gr_set_current_canvas( NULL );
		ui_draw_frame( x, y, x+w-1, y+h-1 );
	}
	else
		W_CANVAS = gr_create_sub_canvas( &(grd_curscreen->sc_canvas), x, y, req_w, req_h );

	gr_set_current_canvas( W_CANVAS );

	if (flags & WIN_FILLED)
		ui_draw_box_out( 0, 0, req_w-1, req_h-1 );

	gr_set_fontcolor( CBLACK, CWHITE );

	selected_gadget = NULL;

	W_TEXT_X = 0;
	W_TEXT_Y = 0;

	return wnd;

}

void ui_close_window( UI_WINDOW * wnd )
{

	ui_mouse_hide();

	ui_gadget_delete_all( wnd );

	if (W_BACKGROUND)
	{
		gr_bm_ubitblt(W_WIDTH, W_HEIGHT, W_X, W_Y, 0, 0, W_BACKGROUND, &(grd_curscreen->sc_canvas.cv_bitmap));
		gr_free_bitmap( W_BACKGROUND );
	} else
	{
		gr_set_current_canvas( NULL );
		gr_setcolor( CBLACK );
		gr_rect( W_X, W_Y, W_X+W_WIDTH-1, W_Y+W_HEIGHT-1 );
	}

	gr_free_sub_canvas( W_CANVAS );

	gr_set_current_canvas( W_OLDCANVAS );

	selected_gadget = NULL;

	remove_window( wnd );

	if (CurWindow==wnd)
		CurWindow = NULL;

	d_free( wnd );

	ui_mouse_show();
}

void restore_state()
{
	int i;
	_disable();
	for (i=0; i<256; i++ )
	{
		keyd_pressed[i] = SavedState[i];
	}
	_enable();
}


fix64 last_event = 0;

void ui_reset_idle_seconds()
{
	last_event = timer_query();
}

int ui_get_idle_seconds()
{
	return (timer_query() - last_event)/F1_0;
}

void ui_mega_process()
{
	int mx, my, mz;
	unsigned char k;
	
	event_process();

	switch( Record )
	{
	case 0:
		mouse_get_delta( &mx, &my, &mz );
		Mouse.new_dx = mx;
		Mouse.new_dy = my;
		Mouse.new_buttons = mouse_get_btns();
		last_keypress = key_inkey();

		if ( Mouse.new_buttons || last_keypress || Mouse.new_dx || Mouse.new_dy )	{
			last_event = timer_query();
		}

		break;
	case 1:

		if (ui_event_counter==0 )
		{
			EventBuffer[ui_event_counter].frame = 0;
			EventBuffer[ui_event_counter].type = 7;
			EventBuffer[ui_event_counter].data = ui_number_of_events;
			ui_event_counter++;
		}


		if (ui_event_counter==1 && (RecordFlags & UI_RECORD_MOUSE) )
		{
			Mouse.new_buttons = 0;
			EventBuffer[ui_event_counter].frame = FrameCount;
			EventBuffer[ui_event_counter].type = 6;
			EventBuffer[ui_event_counter].data = ((Mouse.y & 0xFFFF) << 16) | (Mouse.x & 0xFFFF);
			ui_event_counter++;
		}

		mouse_get_delta( &mx, &my, &mz );
		MouseDX = mx;
		MouseDY = my;
		MouseButtons = mouse_get_btns();

		Mouse.new_dx = MouseDX;
		Mouse.new_dy = MouseDY;

		if ((MouseDX != 0 || MouseDY != 0)  && (RecordFlags & UI_RECORD_MOUSE)  )
		{
			if (ui_event_counter < ui_number_of_events-1 )
			{
				EventBuffer[ui_event_counter].frame = FrameCount;
				EventBuffer[ui_event_counter].type = 1;
				EventBuffer[ui_event_counter].data = ((MouseDY & 0xFFFF) << 16) | (MouseDX & 0xFFFF);
				ui_event_counter++;
			} else {
				Record = 0;
			}

		}

		if ( (MouseButtons != Mouse.new_buttons) && (RecordFlags & UI_RECORD_MOUSE) )
		{
			Mouse.new_buttons = MouseButtons;

			if (ui_event_counter < ui_number_of_events-1 )
			{
				EventBuffer[ui_event_counter].frame = FrameCount;
				EventBuffer[ui_event_counter].type = 2;
				EventBuffer[ui_event_counter].data = MouseButtons;
				ui_event_counter++;
			} else {
				Record = 0;
			}

		}


		if ( keyd_last_pressed && (RecordFlags & UI_RECORD_KEYS) )
		{
			_disable();
			k = keyd_last_pressed;
			keyd_last_pressed= 0;
			_enable();

			if (ui_event_counter < ui_number_of_events-1 )
			{
				EventBuffer[ui_event_counter].frame = FrameCount;
				EventBuffer[ui_event_counter].type = 3;
				EventBuffer[ui_event_counter].data = k;
				ui_event_counter++;
			} else {
				Record = 0;
			}

		}

		if ( keyd_last_released && (RecordFlags & UI_RECORD_KEYS) )
		{
			_disable();
			k = keyd_last_released;
			keyd_last_released= 0;
			_enable();

			if (ui_event_counter < ui_number_of_events-1 )
			{
				EventBuffer[ui_event_counter].frame = FrameCount;
				EventBuffer[ui_event_counter].type = 4;
				EventBuffer[ui_event_counter].data = k;
				ui_event_counter++;
			} else {
				Record = 0;
			}
		}

		last_keypress = key_inkey();

		if (last_keypress == KEY_F12 )
		{
			ui_number_of_events = ui_event_counter;
			last_keypress = 0;
			Record = 0;
			break;
		}

		if ((last_keypress != 0) && (RecordFlags & UI_RECORD_KEYS) )
		{
			if (ui_event_counter < ui_number_of_events-1 )
			{
				EventBuffer[ui_event_counter].frame = FrameCount;
				EventBuffer[ui_event_counter].type = 5;
				EventBuffer[ui_event_counter].data = last_keypress;
				ui_event_counter++;
			} else {
				Record = 0;
			}
		}

		FrameCount++;

		break;
	case 2:
	case 3:
		Mouse.new_dx = 0;
		Mouse.new_dy = 0;
		Mouse.new_buttons = 0;
		last_keypress = 0;

		if ( keyd_last_pressed ) {
			_disable();			
			k = keyd_last_pressed;
			keyd_last_pressed = 0;
			_disable();
			SavedState[k] = 1;
		}

		if ( keyd_last_released ) 
		{
			_disable();			
			k = keyd_last_released;
			keyd_last_released = 0;
			_disable();
			SavedState[k] = 0;
		}
		
		if (key_inkey() == KEY_F12 )
		{
			restore_state();
			Record = 0;
			break;
		}

		if (EventBuffer==NULL) {
			restore_state();
			Record = 0;
			break;
		}

		while( (ui_event_counter < ui_number_of_events) && (EventBuffer[ui_event_counter].frame <= FrameCount) )
		{
			switch ( EventBuffer[ui_event_counter].type )
			{
			case 1:     // Mouse moved
				Mouse.new_dx = EventBuffer[ui_event_counter].data & 0xFFFF;
				Mouse.new_dy = (EventBuffer[ui_event_counter].data >> 16) & 0xFFFF;
				break;
			case 2:     // Mouse buttons changed
				Mouse.new_buttons = EventBuffer[ui_event_counter].data;
				break;
			case 3:     // Key moved down
				keyd_pressed[ EventBuffer[ui_event_counter].data ] = 1;
				break;
			case 4:     // Key moved up
				keyd_pressed[ EventBuffer[ui_event_counter].data ] = 0;
				break;
			case 5:     // Key pressed
				last_keypress = EventBuffer[ui_event_counter].data;
				break;
			case 6:     // Initial Mouse X position
				Mouse.x = EventBuffer[ui_event_counter].data & 0xFFFF;
				Mouse.y = (EventBuffer[ui_event_counter].data >> 16) & 0xFFFF;
				break;
			case 7:
				break;
			}
			ui_event_counter++;
			if (ui_event_counter >= ui_number_of_events )
			{
				Record = 0;
				restore_state();
				//( 0, "Done playing %d events.\n", ui_number_of_events );
			}
		}

		switch (Record)
		{
		case 2:
			{
				int next_frame;
			
				if ( ui_event_counter < ui_number_of_events )
				{
					next_frame = EventBuffer[ui_event_counter].frame;
					
					if ( (FrameCount+PlaybackSpeed) < next_frame )
						FrameCount = next_frame - PlaybackSpeed;
					else
						FrameCount++;
				} else {
				 	FrameCount++;
				}	
			}
			break;

		case 3:			
 			if ( ui_event_counter < ui_number_of_events ) 
				FrameCount = EventBuffer[ui_event_counter].frame;
			else 		
				FrameCount++;
			break;
		default:		
			FrameCount++;
		}
	}

	ui_mouse_process();

}

void ui_wprintf( UI_WINDOW * wnd, char * format, ... )
{
	char buffer[1000];
	va_list args;

	va_start(args, format );
	vsprintf(buffer,format,args);

	gr_set_current_canvas( W_CANVAS );

	ui_mouse_hide();
	W_TEXT_X = gr_string( W_TEXT_X, W_TEXT_Y, buffer );
	ui_mouse_show();
}

void ui_wprintf_at( UI_WINDOW * wnd, short x, short y, char * format, ... )
{
	char buffer[1000];
	va_list args;

	va_start(args, format );
	vsprintf(buffer,format,args);

	gr_set_current_canvas( W_CANVAS );

	ui_mouse_hide();
	gr_string( x, y, buffer );
	ui_mouse_show();

}
