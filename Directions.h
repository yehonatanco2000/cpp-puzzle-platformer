#pragma once



class Direction
{
	int dirx, diry;                           // Direction vector components (x and y)
	friend class point;                       // Allow class point to access private members
public:
	enum { UP, RIGHT, DOWN, LEFT, STAY, NUM_DIRECTIONS }; // Indices for possible directions
	static const Direction directions[NUM_DIRECTIONS];     // Predefined array of all directions
	Direction(int dir_x, int dir_y)                        // Constructor: set direction vector
		: dirx(dir_x), diry(dir_y) {
	}
	int dx() const { return dirx; }           // Get x component of direction
	int dy() const { return diry; }           // Get y component of direction
};

