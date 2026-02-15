#pragma once


class Menu {
public:
	void show_menu();          // Display the main menu on the screen
	int get_num();             // Read and return the user's menu choice
	void res_choice(int num);  // Handle the selected menu option
	void show_instructions();  // Show game instructions screen
	void toggleColor();        // Turn colors ON/OFF and update menu display
};
