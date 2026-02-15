
#include "Menu.h"
#include "KeyboardInputSource.h"
#include "game.h"
#include "utils.h"
#include <conio.h>
#include <iostream>
#include <memory>


// Access global save mode flag from main.cpp
extern bool g_saveMode;

const enum options {
  start = 1,
  back = 4,
  instruction = 8,
  Exit = 9,
  toggleColor = 7
};

void Menu::show_menu() {
  Screen &s = getScreen();
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789//
  std::cout << "           ========================    MENU    "
               "=======================                    "
            << std::endl;
  std::cout << "           |                                                   "
               "      |                    "
            << std::endl;
  std::cout << "           |                                                   "
               "      |                    "
            << std::endl;
  std::cout << "           |                    1 - Start a new game           "
               "      |                    "
            << std::endl;
  std::cout << "           |                                                   "
               "      |                    "
            << std::endl;
  std::cout << "           |                    7 - Colors (ON/OFF):"
            << (s.enableColor ? "ON " : "OFF") << "              |"
            << std::endl;
  std::cout << "           |                                                   "
               "      |                    "
            << std::endl;
  std::cout << "           |                    8 - Present instruction and "
               "keys     |                    "
            << std::endl;
  std::cout << "           |                                                   "
               "      |                    "
            << std::endl;
  std::cout << "           |                    9 - EXIT                       "
               "      |                    "
            << std::endl;
  std::cout << "           |                                                   "
               "      |                    "
            << std::endl;
  std::cout << "           "
               "===========================================================    "
               "                "
            << std::endl;
  int choice = get_num();
  Menu::res_choice(choice);
}

int Menu::get_num() {
  while (true) {
    int ch = _getch(); // Read a single key from the user (no Enter needed)

    switch (ch) {
    case '1':
      return 1; // Start a new game
    case '7':
      return 7; // Toggle colors ON/OFF
    case '8':
      return 8; // Show instructions
    case '9':
      return 9; // Exit game
    case 'b':
      return 4; // Back option (mapped to enum value 'back')
    default:
      break; // Ignore any other key and wait again
    }
  }
}

void Menu::res_choice(int num) {
  cls(); // Clear screen before handling the choice
  switch (num) {
  case options::start: {
    game play; // Create a new game instance
    play.setInputSource(std::make_unique<KeyboardInputSource>());
    if (g_saveMode) {
      play.setSaveMode(true); // Apply save mode if -save was passed
    }
    play.start_game(); // Run the game loop
    break;
  }
  case options::instruction:
    Menu::show_instructions(); // Display instructions screen
    return;                    // Do not immediately re-read choice here
  case options::toggleColor: {
    Menu::toggleColor(); // Flip color mode and redraw menu
    break;
  }
  case options::Exit:
    return; // Leave menu and end program
  case options::back:
    Menu::show_menu(); // Show main menu again
  }

  int choice = get_num(); // Wait for next menu choice from user
  res_choice(choice);     // Recursively handle the new choice
}

void Menu::show_instructions() {
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789//
  std::cout << "=============================== Game Instructions "
               "=============================="
            << std::endl;
  std::cout << "|                                                              "
               "                |"
            << std::endl;
  std::cout << "|1.There are two players participating in the game world.      "
               "                |"
            << std::endl;
  std::cout << "|2.The goal of each player is to reach the end of the level "
               "and solve all      |"
            << std::endl;
  std::cout << "|  challenges.                                                 "
               "                |"
            << std::endl;
  std::cout << "|3.Players can move using the following keys:                  "
               "                |"
            << std::endl;
  std::cout << "|     Player 1: W (Up), A (Left), S (Stay), D (Right), X "
               "(Down), E (Dispose)   |"
            << std::endl;
  std::cout << "|     Player 2: I (Up), J (Left), K (Stay), L (Right), M "
               "(Down), O (Dispose)   |"
            << std::endl;
  std::cout << "|4.You will encounter obstacles, doors, keys, "
               "springs,bombs,and other game     |"
            << std::endl;
  std::cout << "|  elements.                                                   "
               "                |"
            << std::endl;
  std::cout << "|5.Collaboration is required to overcome obstacles and open "
               "new paths.         |"
            << std::endl;
  std::cout << "|6.Good luck!                                                  "
               "                |"
            << std::endl;
  std::cout << "|                                                              "
               "                |"
            << std::endl;
  std::cout << "| *for back press b*                                           "
               "                |"
            << std::endl;
  std::cout << "==============================================================="
               "================="
            << std::endl;

  while (true) {
    int ch = _getch();
    if (ch == 'b') {
      cls();
      show_menu();
      return;
    }
  }
}

void Menu::toggleColor() {
  Screen &s = getScreen(); // Get reference to the shared Screen object
  s.enableColor =
      !s.enableColor; // Flip color mode: true -> false, false -> true

  // Redraw the menu so the ON/OFF text matches the new state
  cls();       // Clear the console
  show_menu(); // Show updated main menu
}
