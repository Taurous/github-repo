/*	LICENSE
	DnD Battle Map
    Copyright (C) 2021  Aksel Huff

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include <iostream> // For std::cerr
#include <chrono>	// For FPS counting and providing game with tick time
#include <fstream>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_native_dialog.h>

#include "input.hpp"
#include "util.hpp"

#include "map_editor.hpp"

#include "editor_events.hpp"

constexpr int 	DEFAULT_WIND_WIDTH	= 1400;
constexpr int 	DEFAULT_WIND_HEIGHT	= 900;
constexpr int 	CONSOLE_HEIGHT		= 256;
constexpr char 	DISPLAY_TITLE[]		= "Axe DnD Map";

using std_clk = std::chrono::steady_clock;

struct ThreadArgs
{
	std::string bmp_path;
	ALLEGRO_MUTEX *mutex;
	ALLEGRO_COND *cond;
	ALLEGRO_EVENT_SOURCE *event_source;
};

ALLEGRO_DISPLAY *createDisplay(const char* title, int width, int height, int flags)
{
	ALLEGRO_DISPLAY *d = nullptr;

	al_set_new_display_flags(flags);

	std::string full_title = DISPLAY_TITLE;
	full_title += " - ";
	full_title += title;
	al_set_new_window_title(full_title.c_str());

	d = al_create_display(width, height);

	if (!d)
	{
		std::cerr << "Failed to create display!" << std::endl;
		exit(EXIT_FAILURE);
	}

	return d;
}

static void *thread_func(ALLEGRO_THREAD* thr, void* arg)
{
	ALLEGRO_DISPLAY* display = nullptr;
	ALLEGRO_EVENT_QUEUE* evq = nullptr;
	ALLEGRO_TIMER* timer = nullptr;
	ALLEGRO_EVENT ev;
	std_clk::time_point current_time;
	double delta_time;
	bool running = true;
	bool redraw = true;

	ThreadArgs *thargs = (ThreadArgs*)arg;
	View view;
	bool lerping = false;
	constexpr double lerp_time = 1.5;
	double elapsed = 0.0;
	vec2d view_start;
	vec2d view_target;
	ALLEGRO_BITMAP* bmp;

	display = createDisplay("Viewer", DEFAULT_WIND_WIDTH, DEFAULT_WIND_HEIGHT - CONSOLE_HEIGHT, ALLEGRO_RESIZABLE | ALLEGRO_WINDOWED);
	timer = al_create_timer(1.0 / 60.0);
	evq = al_create_event_queue();

	al_register_event_source(evq, al_get_timer_event_source(timer));
	al_register_event_source(evq, al_get_display_event_source(display));
	al_register_event_source(evq, thargs->event_source);

	bmp = al_load_bitmap(thargs->bmp_path.c_str());
	if (!bmp)
	{
		std::cerr << "Failed to load bitmap in thread" << std::endl;
		if (display) al_destroy_display(display);
		if (timer) al_destroy_timer(timer);
		if (evq) al_destroy_event_queue(evq);
		return NULL;
	}

	al_start_timer(timer);
	auto last_time = std_clk::now();
	while (running)
	{
		if (al_get_thread_should_stop(thr))
		{
			break;
		}

		al_wait_for_event(evq, &ev);
	
		vec2d diff;
		switch (ev.type)
		{
			case AXE_EDITOR_EVENT_MOVE_VIEW:
				view_target.x = static_cast<double>(ev.user.data1);
				view_target.y = static_cast<double>(ev.user.data2);
				view_start = view.world_pos;
				lerping = true;
				elapsed = 0.0;
			break;

			case AXE_EDITOR_EVENT_SCALE_VIEW:
				std::cout << "AXE_EDITOR_EVENT_SCALE_VIEW\n" << "\tev.user.data1 = " << ev.user.data1 << "\n";
				if (ev.user.data1 > 0)
				{
					view.scale += { 0.1, 0.1 };
				}
				else if (ev.user.data1 < 0)
				{
					view.scale -= { 0.1, 0.1 };
				}
			break;

			case ALLEGRO_EVENT_TIMER:
				current_time = std_clk::now();
				delta_time = std::chrono::duration<double>(current_time - last_time).count();
				last_time = current_time;
				elapsed += delta_time;
				redraw = true;

				if (lerping && elapsed <= lerp_time)
				{
					view.world_pos = vec_lerp(view_start, view_target, easeInAndOutQuart(elapsed / lerp_time));
				}
				else lerping = false;
			break;

			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				running = false;
			break;

			case ALLEGRO_EVENT_DISPLAY_RESIZE:
				al_acknowledge_resize(display);
			break;

			default:
			break;
		}

		if (al_event_queue_is_empty(evq) && redraw)
		{
			al_clear_to_color(al_map_rgb(45, 45, 45));

			int w = al_get_display_width(display);
			int h = al_get_display_height(display);

			al_draw_bitmap_region(bmp, view.world_pos.x - (w/2),view.world_pos.y - (h/2), w, h, 0, 0, 0);

			al_flip_display();
			redraw = false;
		}
	}

	al_destroy_bitmap(bmp);
	al_destroy_timer(timer);
	al_destroy_event_queue(evq);
	al_destroy_display(display);

	return NULL;
}

int main(int argc, char ** argv)
{
	ALLEGRO_DISPLAY*		main_display	= nullptr;
	ALLEGRO_EVENT_QUEUE*	ev_queue		= nullptr;
	ALLEGRO_TIMER*			timer			= nullptr;
	ALLEGRO_THREAD*			view_thread		= nullptr;
	ALLEGRO_EVENT ev;
	std_clk::time_point current_time;
	double delta_time;
	
	bool redraw = true;
	bool quit = false;

	ThreadArgs thargs;
	thargs.bmp_path = "/home/aksel/Downloads/Maps of the Mad Mage-20220114T044344Z-001/Maps of the Mad Mage/L1_grid.jpg";
	thargs.mutex = al_create_mutex();
	thargs.cond = al_create_cond();

	if (!al_init())
	{
		std::cerr << "Failed to load Allegro!" << std::endl;
		exit(EXIT_FAILURE);
	}

	main_display = createDisplay("Editor", DEFAULT_WIND_WIDTH, DEFAULT_WIND_HEIGHT, ALLEGRO_WINDOWED | ALLEGRO_RESIZABLE);

	al_init_image_addon();
	al_init_font_addon();
	al_init_ttf_addon();
	al_init_primitives_addon();
	al_init_native_dialog_addon();

	timer = al_create_timer(1.0 / 60.0);
	ev_queue = al_create_event_queue();

	InputHandler m_input; // Installs keyboard and mouse

	al_register_event_source(ev_queue, al_get_keyboard_event_source());
	al_register_event_source(ev_queue, al_get_mouse_event_source());
	al_register_event_source(ev_queue, al_get_timer_event_source(timer));
	al_register_event_source(ev_queue, al_get_display_event_source(main_display));

	// User Events

	ALLEGRO_EVENT_SOURCE editor_event_source;
	al_init_user_event_source(&editor_event_source);

	thargs.event_source = &editor_event_source;

	MapEditor map_editor(m_input, editor_event_source, {0, 0}, { getScreenSize().x, getScreenSize().y - CONSOLE_HEIGHT });

	// Set program lifetime keybinds
	m_input.setKeybind(ALLEGRO_KEY_ESCAPE, 	[&quit](){ quit = true; });
	m_input.setKeybind(ALLEGRO_KEY_F1,		[&](){
		al_destroy_thread(view_thread);

		view_thread = nullptr;

		// Pass pointer to map data into thread

		view_thread = al_create_thread(thread_func, &thargs);
		al_start_thread(view_thread);
	});

	m_input.callKeybind(ALLEGRO_KEY_F1);

	al_start_timer(timer);
	auto last_time = std_clk::now();
	while (!quit)
	{
		al_wait_for_event(ev_queue, &ev);

		// Skip any events where the focus is the view display
		if (ev.any.source == al_get_mouse_event_source())
		{
			if (ev.mouse.display != main_display)
			{
				continue;
			}
		}
		else if (ev.any.source == al_get_keyboard_event_source())
		{
			if (ev.keyboard.display != main_display)
			{
				continue;
			}
		}

		m_input.getInput(ev);
		map_editor.handleEvents(ev);
	
		switch (ev.type)
		{
			case ALLEGRO_EVENT_DISPLAY_RESIZE:
				al_acknowledge_resize(ev.display.source);
				map_editor.resizeView({0, 0}, { getScreenSize().x, getScreenSize().y - CONSOLE_HEIGHT });
			break;

			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				quit = true;
			break;

			case ALLEGRO_EVENT_TIMER:
				current_time = std_clk::now();
				delta_time = std::chrono::duration<double>(current_time - last_time).count();
				last_time = current_time;
				map_editor.update(delta_time);
				redraw = true;
			break;

			default:
			break;
		}
		
		//Drawing

		if (al_event_queue_is_empty(ev_queue) && redraw)
		{
			al_clear_to_color(al_map_rgb(0, 0, 0));

			map_editor.draw();

			al_flip_display();

			redraw = false;
		}
	}

	if (view_thread) al_set_thread_should_stop(view_thread);

	al_destroy_timer(timer);
	al_destroy_event_queue(ev_queue);
	al_destroy_display(main_display);

	if (thargs.mutex) al_destroy_mutex(thargs.mutex);
	if (thargs.cond) al_destroy_cond(thargs.cond);
	al_destroy_thread(view_thread);

	return 0;
}