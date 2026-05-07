#ifndef GAME_3_H
#define GAME_3_H

#include "Menu.h"

/*
 * Game 3: Submarine Rescue
 *
 * Joystick-controlled rescue game. The player controls a submarine,
 * rescues divers, avoids mines, manages oxygen, and returns to base
 * to score points.
 */

void Game3_Init(void);
void Game3_Update(void);
void Game3_Render(void);
MenuState Game3_Run(void);

#endif
