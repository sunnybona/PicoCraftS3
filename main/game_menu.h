// Title screen, in-game pause menu and save-slot / test-scenario selection.
// This module owns only the menu state and drawing; it never touches the game
// world objects directly. The main loop reads the selection and performs the
// actual load / scenario setup (see main.cpp).
#ifndef GAME_MENU_H
#define GAME_MENU_H

#include <cstdint>
#include "voxel/save_store.h"

enum GameMode : uint8_t {
    MODE_TITLE = 0,
    MODE_PLAY,
    MODE_PAUSE,
};

// What kind of entry is selected on the title screen.
enum TitleEntryType : uint8_t {
    TENTRY_SLOT = 0,   // real user save slot
    TENTRY_TEST,       // programmatic test scenario
};

// Programmatic test scenarios, each in a different place/environment with its
// own pre-built structure (camp, wooden house, cave, fort, quarry, hall,
// tower, lakeside).
enum TestScenario : uint8_t {
    TEST_FRESH = 0,    // spawn plains camp, starter kit
    TEST_CRAFT,        // forest wooden house with a workshop
    TEST_MINE,         // hand-built cave with ore veins and torches
    TEST_COMBAT,       // night wilderness camp with a low wall
    TEST_TNT,          // open quarry, TNT array and a cover wall
    TEST_ALL,          // great hall displaying every block
    TEST_TOWER,        // tall lookout tower to climb
    TEST_LAKE,         // lakeside cottage on the sand shore
    TEST_COUNT,
};

#define TITLE_SLOT_N   SAVE_SLOT_COUNT
#define TITLE_TEST_N   TEST_COUNT
#define TITLE_ENTRY_N  (TITLE_SLOT_N + TITLE_TEST_N)

// Pause menu actions.
enum PauseAction : uint8_t {
    PAUSE_NONE = 0,
    PAUSE_RESUME,
    PAUSE_SAVE,
    PAUSE_QUIT,
};

class GameMenu {
public:
    GameMode mode;
    int title_sel;
    int pause_sel;

    GameMenu();

    // Drive the title screen. Returns true once on the frame A confirms a
    // selection; out_type/out_id then describe the chosen slot or scenario.
    bool title_update(TitleEntryType &out_type, int &out_id);

    // Draw the title screen. slot_used[i] tells whether user slot i has data.
    void title_draw(const bool slot_used[]);

    // Drive the pause menu; returns the chosen action (PAUSE_NONE while idle).
    PauseAction pause_update();

    // Draw the pause overlay.
    void pause_draw();

    // Static labels for a test scenario (used by the help document too).
    static const char *test_name(int scenario);
};

extern GameMenu game_menu;

#endif
