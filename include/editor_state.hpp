#pragma once

#include <list>
#include <vector>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>

#include "abstract_state.hpp"

#include "view.hpp"
#include "map.hpp"
#include "edit_commands.hpp"

class EditorState : public AbstractState
{
public:
	EditorState(StateMachine& state_machine, InputHandler& input);

	~EditorState() override;

	void pause() override;
	void resume() override;

	void handleEvents(const ALLEGRO_EVENT &ev) override;
	void update(double delta_time) override;
	void draw() override;

	//Keybound Functions
	void save();
	void load();
	void undo();
	void redo();
	
	void onMouseWheelUp();
	void onMouseWheelDown();
	void onMiddleMouseUp();
	void onMiddleMouseDown();
	void onLeftMouseUp();
	void onLeftMouseDown();
	void onRightMouseDown();
	void onRightMouseUp();
	void addTileToEditVector(vec2i position, bool show);

	bool isMouseInView();
	void zoomToCursor(bool zoom_out);
private:
	ALLEGRO_FONT* fn;

	View view;
	Map map;
	std::string save_file;

	vec2i last_pos;
	vec2i mouse_pos; // Needs better name

	vec2i last_tile_hovered;

	vec2i fill_start_pos;
	bool dragging;
	bool filling;
	bool show_hidden;
	bool draw_grid;
	bool draw_debug;

	std::list<std::unique_ptr<Command>> redo_stack;
	std::list<std::unique_ptr<Command>> undo_stack;

	void pushCommand(std::unique_ptr<Command> c);
	std::vector<vec2i> tiles_to_edit;
};