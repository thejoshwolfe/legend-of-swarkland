#ifndef INPUT_HPP
#define INPUT_HPP

#include "thing.hpp"
#include "action.hpp"

enum InputMode {
    InputMode_MAIN,
    InputMode_INVENTORY_CHOOSE_ITEM,
    InputMode_INVENTORY_CHOOSE_ACTION,
    InputMode_THROW_CHOOSE_DIRECTION,
    InputMode_ZAP_CHOOSE_DIRECTION,
    InputMode_FLOOR_CHOOSE_ACTION,
    InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES,
};

extern bool request_shutdown;

extern InputMode input_mode;
extern int inventory_cursor;
extern Thing chosen_item;
extern List<Action::Id> inventory_menu_items;
extern int inventory_menu_cursor;
extern int floor_menu_cursor;
extern int cheatcode_generate_monster_choose_species_menu_cursor;

Coord get_mouse_pixels();
void on_mouse_motion();
void read_input();
void get_floor_actions(Thing actor, List<Action> * actions);

#endif
