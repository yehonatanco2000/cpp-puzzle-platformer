#pragma once

#include "point.h"
#include "Screen.h"
#include <iostream>

void point::draw(char ch) {
	gotoxy(x, y);                  // Move console cursor to the point's position
	std::cout << ch << std::flush; // Print given character and flush output
}

void point::move() {

	if (x + dir.dirx < 0 || x + dir.dirx > Screen::MAX_X) {
		dir.dirx = -dir.dirx;      // Reverse horizontal direction when hitting screen edge
	}
	if (y + dir.diry < 0 || y + dir.diry > Screen::MAX_Y) {
		dir.diry = -dir.diry;      // Reverse vertical direction when hitting screen edge
	}
	if (dir.dx() == 0 && dir.dy() == 0)
		return;                    // Do nothing if direction is STAY

	x += dir.dirx;                 // Move point in x by current direction
	y += dir.diry;                 // Move point in y by current direction
}

