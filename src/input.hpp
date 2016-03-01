#ifndef INPUT_HPP
#define INPUT_HPP

#include "thing.hpp"
#include "action.hpp"

enum InputMode {
    InputMode_MAIN,
    InputMode_REPLAY,
    InputMode_INVENTORY_CHOOSE_ITEM,
    InputMode_CHOOSE_ABILITY,
    InputMode_INVENTORY_CHOOSE_ACTION,
    InputMode_THROW_CHOOSE_DIRECTION,
    InputMode_ABILITY_CHOOSE_DIRECTION,
    InputMode_ZAP_CHOOSE_DIRECTION,
    InputMode_READ_BOOK_CHOOSE_DIRECTION,
    InputMode_FLOOR_CHOOSE_ACTION,
    InputMode_CHEATCODE_POLYMORPH_CHOOSE_SPECIES,
    InputMode_CHEATCODE_WISH_CHOOSE_THING_TYPE,
    InputMode_CHEATCODE_WISH_CHOOSE_WAND_ID,
    InputMode_CHEATCODE_WISH_CHOOSE_POTION_ID,
    InputMode_CHEATCODE_WISH_CHOOSE_BOOK_ID,
    InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_SPECIES,
    InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_DECISION_MAKER,
    InputMode_CHEATCODE_GENERATE_MONSTER_CHOOSE_LOCATION,
};

extern Action current_player_decision;

extern bool request_shutdown;

extern InputMode input_mode;
extern int inventory_cursor;
extern List<Action> inventory_menu_items;
extern int inventory_menu_cursor;
extern int ability_cursor;
extern int floor_menu_cursor;
extern int cheatcode_polymorph_choose_species_menu_cursor;
extern int cheatcode_wish_choose_thing_type_menu_cursor;
extern int cheatcode_wish_choose_wand_id_menu_cursor;
extern int cheatcode_wish_choose_potion_id_menu_cursor;
extern int cheatcode_wish_choose_book_id_menu_cursor;
extern int cheatcode_generate_monster_choose_species_menu_cursor;
extern int cheatcode_generate_monster_choose_decision_maker_menu_cursor;
extern Coord cheatcode_generate_monster_choose_location_cursor;

Coord get_mouse_pixels();
void on_mouse_motion();
void read_input();
void get_floor_actions(List<Action> * actions);

#endif
