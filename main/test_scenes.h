// Programmatic test scenes in eight different locations/environments.
// Each scene loads the chunks around its own centre, then builds a distinct
// pre-made structure (camp, house, cave, fort, quarry, hall, tower, lakeside)
// and places the player safely inside it.
#ifndef TEST_SCENES_H
#define TEST_SCENES_H

#include <cstdint>

// Enter test scene t (0..TEST_COUNT-1). Resets the world, builds the scene and
// switches the game to MODE_PLAY with no associated save slot (current_slot=-1).
void enter_test_scene(int t);

#endif
