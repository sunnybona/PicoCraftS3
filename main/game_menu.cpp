#include "game_menu.h"
#include "picosystem.hpp"
#include "engine/render_globals.h"
#include <string>

using namespace picosystem;

GameMenu game_menu;

static const char *TEST_NAMES[TEST_COUNT] = {
    "FRESH", "CRAFT", "MINE", "COMBAT", "TNT", "ALL ITEMS", "TOWER", "LAKE"
};

const char *GameMenu::test_name(int scenario) {
    if (scenario < 0 || scenario >= TEST_COUNT) return "?";
    return TEST_NAMES[scenario];
}

GameMenu::GameMenu() {
    mode = MODE_TITLE;
    title_sel = 0;
    pause_sel = 0;
}

// ---------------- Title screen ----------------
bool GameMenu::title_update(TitleEntryType &out_type, int &out_id) {
    if (pressed(UP)) {
        title_sel = (title_sel - 1 + TITLE_ENTRY_N) % TITLE_ENTRY_N;
    }
    if (pressed(DOWN)) {
        title_sel = (title_sel + 1) % TITLE_ENTRY_N;
    }
    if (pressed(A)) {
        if (title_sel < TITLE_SLOT_N) {
            out_type = TENTRY_SLOT;
            out_id = title_sel;
        } else {
            out_type = TENTRY_TEST;
            out_id = title_sel - TITLE_SLOT_N;
        }
        return true;
    }
    return false;
}

void GameMenu::title_draw(const bool slot_used[]) {
    // Background
    pen(2, 3, 7);
    frect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Title (centered)
    pen(15, 14, 6);
    text("PICOCRAFT", 53, 5);
    pen(10, 12, 15);
    text("SURVIVAL EDITION", 36, 14);

    for (int i = 0; i < TITLE_ENTRY_N; i++) {
        int y = 26 + i * 9;
        bool sel = (i == title_sel);

        std::string label;
        if (i < TITLE_SLOT_N) {
            int slot = i;
            label = "SLOT " + std::to_string(slot + 1);
            if (sel) pen(15, 15, 8);
            else     pen(12, 13, 15);
            text((sel ? "> " : "  ") + label, 8, y);
            // Slot state on the right
            if (slot_used[slot]) {
                pen(10, 15, 10);
                text("[SAVED]", SCREEN_WIDTH - 52, y);
            } else {
                pen(8, 9, 11);
                text("[EMPTY]", SCREEN_WIDTH - 52, y);
            }
        } else {
            int t = i - TITLE_SLOT_N;
            if (sel) pen(15, 15, 8);
            else     pen(11, 12, 14);
            std::string mark = sel ? "> TEST:" : "  TEST:";
            text(mark + TEST_NAMES[t], 8, y);
        }
    }

    pen(13, 14, 16);
    text("UP/DOWN SELECT  A:START", 10, SCREEN_HEIGHT - 9);
}

// ---------------- Pause menu ----------------
PauseAction GameMenu::pause_update() {
    if (pressed(UP))   pause_sel = (pause_sel - 1 + 3) % 3;
    if (pressed(DOWN)) pause_sel = (pause_sel + 1) % 3;
    if (pressed(B))    return PAUSE_RESUME;
    if (pressed(A)) {
        switch (pause_sel) {
            case 0:  return PAUSE_RESUME;
            case 1:  return PAUSE_SAVE;
            default: return PAUSE_QUIT;
        }
    }
    return PAUSE_NONE;
}

void GameMenu::pause_draw() {
    // Dim panel
    pen(1, 1, 3);
    frect(28, 32, 104, 58);
    pen(15, 15, 15);
    rect(28, 32, 104, 58);

    pen(15, 14, 6);
    text("PAUSED", 70, 38);

    static const char *ITEMS[3] = {"RESUME", "SAVE NOW", "QUIT TITLE"};
    for (int i = 0; i < 3; i++) {
        if (i == pause_sel) pen(15, 15, 8);
        else               pen(12, 13, 15);
        text((i == pause_sel ? "> " : "  ") + std::string(ITEMS[i]),
             44, 52 + i * 10);
    }
    pen(13, 14, 16);
    text("A:OK  B:BACK", 50, 84);
}
