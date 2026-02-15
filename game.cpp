#include "game.h"
#include "Directions.h"
#include "KeyboardInputSource.h"
#include "Menu.h"
#include "Riddle.h"
#include "Screen.h"
#include "player.h"
#include <Windows.h>
#include <algorithm>
#include <conio.h>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>

void game::start_game() {
  if (!loadSuccess) {
    cls();
    std::cout << "==========================================================="
              << std::endl;
    std::cout << fatalError << std::endl;
    std::cout << "==========================================================="
              << std::endl;
    std::cout << "\nPress any key to return to menu..." << std::endl;
    _getch();
    cls();
    Menu return_menu;
    return_menu.show_menu();
    return;
  }

  hideCursor();            // Hide console cursor during the game
  constexpr char ESC = 27; // ESC key code

  // Setup default input source if not set
  if (!inputSource) {
    inputSource = std::make_unique<KeyboardInputSource>();
  }

  // In load mode, apply saved color setting BEFORE initial draw
  if (loadMode && inputSource) {
    bool savedColor = inputSource->getSavedColorMode();
    theScreen.enableColor = savedColor;

    // Initialize result verifier
    resultVerifier.load("adv-world.result");
  }

  // Start recording if in save mode
  // Note: We do this early to capture initial state if needed,
  // but originally it was later. Moving it here is safe.
  if (saveMode) {
    stepRecorder.start("adv-world.steps", screenFileNames,
                       theScreen.enableColor);
    resultRecorder.start("adv-world.result");
  }

  if (!silentMode) {
    theScreen.draw(); // Initial draw of the whole screen
    drawHUD();
    p1.updateHUD();
    p2.updateHUD();
  }

  // Store previous positions to detect movement
  int lastP1X = p1.getX(), lastP1Y = p1.getY();
  int lastP2X = p2.getX(), lastP2Y = p2.getY();

  // Track if players held a torch in the previous frame
  bool lastP1HadTorch = (p1.getCarryItem() == '!');
  bool lastP2HadTorch = (p2.getCarryItem() == '!');
  gameCycle = 0; // Reset cycle counter
  TimeGame = 0;  // Reset game timer
  int timer = 0; // Reset clock tick counter
  score = 0;     // Reset score for new game
  int width = 62;

  while (true) {

    theScreen.setTorchInfo(p1.getCarryItem() == '!', // player 1 holds torch?
                           p2.getCarryItem() == '!', // player 2 holds torch?
                           p1.getX(), p1.getY(), p2.getX(), p2.getY());

    // Optimization: Only redraw full screen if lighting conditions change
    bool needRedraw = false;

    // 1. Check if players moved (while holding torch)
    bool p1Moved = (p1.getX() != lastP1X || p1.getY() != lastP1Y);
    bool p2Moved = (p2.getX() != lastP2X || p2.getY() != lastP2Y);
    bool p1HasTorch = (p1.getCarryItem() == '!');
    bool p2HasTorch = (p2.getCarryItem() == '!');

    if ((p1Moved && p1HasTorch) || (p2Moved && p2HasTorch)) {
      needRedraw = true;
    }

    // 2. Check if torch status CHANGED (Pickup or Drop)
    if (p1HasTorch != lastP1HadTorch || p2HasTorch != lastP2HadTorch) {
      needRedraw = true;
    }

    // Update trackers for next frame
    lastP1X = p1.getX();
    lastP1Y = p1.getY();
    lastP2X = p2.getX();
    lastP2Y = p2.getY();
    lastP1HadTorch = p1HasTorch;
    lastP2HadTorch = p2HasTorch;

    if (needRedraw && !silentMode) {
      theScreen.draw();
      drawHUD();
    }

    timer++;
    if (timer % GAME_CLOCK_TICKS_PER_SECOND == 0) {
      if (getTimeGame() < TIME_PER_GAME) {
        setTimeGame(getTimeGame() + 1); // game time++
      }
      if (getScore() >= 100)
        width = 63;
      int tx = getHudX() + width;
      int ty = getHudY() + 2;
      if (currentRoom != 3 && (!p1HasTorch && !p2HasTorch) && !silentMode) {
        gotoxy(tx, ty);
        std::cout << (TIME_PER_GAME - getTimeGame()) << " " << std::flush;
      }
    }

    // HUD should be updated if needed, but its functions handle cursor
    // positioning well drawHUD(); // Static parts of HUD rarely change
    if (p1.getIsActive())
      p1.updateHUD();
    if (p2.getIsActive())
      p2.updateHUD();

    for (auto &b : bombs) { // Update all active bombs
      if (!b.alive)
        continue; // Skip bombs that already exploded
      if (!silentMode) {
        gotoxy(b.x, b.y);                   // Move cursor to bomb position
        std::cout << b.timer << std::flush; // Show remaining timer
        Sleep(50);                          // Small delay for visual effect
      }
      if (--b.timer == 0) {  // Decrease timer and check explosion
        explodeAt(b.x, b.y); // Trigger explosion effect
        b.alive = false;     // Mark bomb as dead
        if (!silentMode) {
          theScreen.draw();
          drawHUD();
        }
      }
    }

    if (p1.getIsActive()) {
      p1.move(); // Move player 1 if still active
    }

    if (p2.getIsActive()) {
      p2.move(); // Move player 2 if still active
    }

    // In silent mode, skip sleep for faster execution
    if (!silentMode) {
      Sleep(loadMode ? 50 : 100); // Faster in load mode
    }

    // Notify input source of current cycle
    inputSource->onCycle(gameCycle);

    // Process one logical input per cycle (matches saveMode behavior)
    if (inputSource->hasInput()) {
      char key = inputSource->getKey();

      // Record step if in save mode
      if (stepRecorder.isActive()) {
        stepRecorder.recordStep(gameCycle, key);
      }

      if (key == ESC) {
        int action = pause_menu();
        if (action == 1) { // Return to main menu
          if (saveMode) {
            resultRecorder.recordGameEnd(gameCycle, getScore());
          }
          if (loadMode && resultVerifier.isActive()) {
            resultVerifier.verifyGameEnd(gameCycle, getScore());
            if (silentMode) {
              cls();
              if (resultVerifier.isPassed() &&
                  resultVerifier.allEventsConsumed()) {
                std::cout << "Test passed" << std::endl;
              } else {
                std::cout << "Test failed" << std::endl;
              }
            }
          }
          stepRecorder.stop();
          resultRecorder.stop();
          if (!silentMode)
            cls();
          if (!loadMode) {
            Menu show_main_menu;
            show_main_menu.show_menu();
          }
          return; // Exit start_game
        } else {  // Continue game
          if (!silentMode) {
            theScreen.draw();
            drawHUD();
            p1.updateHUD();
            p2.updateHUD();
          }
          lastP1X = p1.getX();
          lastP1Y = p1.getY();
          lastP2X = p2.getX();
          lastP2Y = p2.getY();
        }
      } else if ((unsigned char)key == 224 || (unsigned char)key == 0) {
        // Special key - consume extra byte immediately
        p1.keyPressed(key);
        p2.keyPressed(key);
        if (inputSource->hasInput()) {
          char secondByte = inputSource->getKey();
          if (stepRecorder.isActive()) {
            stepRecorder.recordStep(gameCycle, secondByte);
          }
          p1.keyPressed(secondByte);
          p2.keyPressed(secondByte);
        }
      } else {
        p1.keyPressed(key);
        p2.keyPressed(key);
      }
    }

    // Check for Game Over conditions
    bool bothDead = !p1.getIsActive() && !p2.getIsActive();
    bool timeOut = (getTimeGame() >= TIME_PER_GAME);
    bool stepsOut = (loadMode && !inputSource->isValid());

    // If game should naturally end OR replay has no more steps
    if (bothDead || stepsOut) {
      if (saveMode && (bothDead)) {
        resultRecorder.recordGameEnd(gameCycle, getScore());
      }

      bool resultsPending =
          resultVerifier.isActive() && !resultVerifier.allEventsConsumed();
      bool cycleExceeded = false;

      if (resultsPending) {
        int nextCycle = resultVerifier.getNextExpectedCycle();
        if (nextCycle != -1) {
          // If game ended prematurely (death/timeout) but more results expected
          // -> fail immediately
          if (bothDead || timeOut || gameCycle > nextCycle + 50)
            cycleExceeded = true;
        } else {
          resultsPending = false; // Only END remains
        }
      }

      if (!resultsPending || cycleExceeded) {
        if (!silentMode)
          theScreen.draw();

        if (resultVerifier.isActive()) {
          resultVerifier.verifyGameEnd(gameCycle, getScore());
        }

        if (loadMode && !silentMode) {
          cls();
          std::cout << "\n========================================"
                    << std::endl;
          std::cout << "          REPLAY COMPLETED" << std::endl;
          std::cout << "========================================" << std::endl;
          std::cout << "Press any key to exit..." << std::endl;
          _getch();
        } else {
          // Replay results for automated testing (Silent Mode)
          if (resultVerifier.isActive()) {
            cls();
            if (resultVerifier.isPassed() &&
                resultVerifier.allEventsConsumed()) {
              std::cout << "Test passed" << std::endl;
            } else {
              std::cout << "Test failed" << std::endl;
            }
          }
        }
        return;
      }
    }

    ++gameCycle; // Increment game cycle
  }

  return;
}
void game::initObjectsFromBoard() {
  riddles.clear(); // Remove old riddles from previous room

  for (int y = 0; y <= Screen::MAX_Y; ++y) {
    for (int x = 0; x <= Screen::MAX_X; ++x) {
      char ch = theScreen.board[y][x]; // Character at current board cell

      if (ch == 'k') {
        keys.emplace_back(x, y); // Add key object at this position
      }
      if (ch == '?') {
        if (!all_riddles.empty()) {
          int idx = nextRiddleIndex % all_riddles.size();
          riddles.emplace_back(x, y, all_riddles[idx]);
          ++nextRiddleIndex;
        }
      } else if (ch == '/' || ch == '\\') {
        switches.push_back(std::make_unique<DoorSwitch>(x, y, ch == '/'));
      } else if (ch == '+' || ch == '-') {
        switches.push_back(std::make_unique<WallSwitch>(x, y, ch == '+'));

      } else if (ch == '@') {
        bombItem.emplace_back(x, y); // Bomb item on floor
      } else if (ch >= '1' && ch <= '9') {
        Doors newDoor(x, y, ch);
        // Look for overrides for this room
        int roomIdx = useFileRooms ? currentRoom : 0;
        if (roomIdx < doorKeyOverrides.size()) {
          for (auto &p : doorKeyOverrides[roomIdx]) {
            if (p.first == ch)
              newDoor.setRequiredKeys(p.second);
          }
        }
        doors.push_back(newDoor);

      } else if (ch == '!') {
        torches.emplace_back(x, y); // Add torch at this position
      } else if (ch == '#') {
        springs.emplace_back(x, y); // add spring cell at this position
      } else if (ch == '*') {       // Obstacle
        // Check if an obstacle part is already here to group them
        bool alreadyExists = false;
        for (const auto &obs : obstacles) {
          if (obs.contains(x, y)) {
            alreadyExists = true;
            break;
          }
        }
        if (!alreadyExists) {
          obstacles.emplace_back(x, y, theScreen, this);
        }
      }
    }
  }
}

void game::handleStepOnObjects(player &pl) {
  int x = pl.getX(); // Player current x
  int y = pl.getY(); // Player current y

  // --- Handle switches ---
  for (size_t i = 0; i < switches.size(); ++i) {
    if (switches[i]->getX() == x && switches[i]->getY() == y) {

      // Toggle switch state and update display
      switches[i]->toggle(theScreen);
      switchOn = switches[i]->isOn();
      drawHUD();

      if (!switches[i]->isDoorSwitch()) {
        update_Internal_Wall_By_Switch(*switches[i]);
      }
      break; // Only one switch per tile
    }
  }

  // --- Handle riddles ---
  for (size_t i = 0; i < riddles.size(); ++i) {
    if (riddles[i].getX() == x && riddles[i].getY() == y) {

      bool solved;
      char riddleAnswer = '0';

      // In load mode, get answer from input source
      if (loadMode) {
        if (inputSource) {
          riddleAnswer = inputSource->getRiddleAnswer();
          if (riddleAnswer != 0) {
            solved = riddles[i].askWithAnswer(riddleAnswer, silentMode);
          } else {
            solved = false;
          }
        } else {
          solved = false;
        }
      } else {
        // Normal mode - ask player
        solved = riddles[i].ask(silentMode);
        riddleAnswer =
            char('1' + riddles[i].getCorrectIndex()); // Default if correct
      }

      // Record riddle answer in save mode
      if (stepRecorder.isActive() && !loadMode) {
        // Get the actual answer that was given by the user
        char answerChar = riddles[i].getLastChoice();
        stepRecorder.recordStep(gameCycle, answerChar);
      }

      // Determine player number for recording
      int playerNum = (&pl == &p1) ? 1 : 2;

      if (solved) {
        setScore(score + 50); // Increase score for correct answer

        // Record riddle result
        if (resultRecorder.isActive()) {
          resultRecorder.recordRiddle(gameCycle, playerNum,
                                      riddles[i].getQuestion(),
                                      riddles[i].getAnswer(), true);
        }
        if (loadMode && resultVerifier.isActive()) {
          resultVerifier.verifyRiddle(gameCycle, playerNum,
                                      riddles[i].getQuestion(),
                                      riddles[i].getAnswer(), true);
        }

        if (!silentMode) {
          std::cout << "Correct answer!" << std::endl;
          std::cout << "you get 50 points! Your current score is: "
                    << getScore() << std::endl;
          Sleep(2500);
        }
        // Correct answer: remove riddle mark from screen and vector
        theScreen.setChar(y, x, ' ');
        riddles.erase(riddles.begin() + i);
      } else {
        setScore(score - 30); // Decrease score for wrong answer

        // Record riddle result
        if (resultRecorder.isActive()) {
          resultRecorder.recordRiddle(gameCycle, playerNum,
                                      riddles[i].getQuestion(), "", false);
        }
        if (loadMode && resultVerifier.isActive()) {
          resultVerifier.verifyRiddle(gameCycle, playerNum,
                                      riddles[i].getQuestion(), "", false);
        }

        if (!silentMode) {
          std::cout << "Wrong answer! Returning to start position."
                    << std::endl;
          std::cout << "you lose 30 points! Your current score is: "
                    << getScore() << std::endl;
          Sleep(3000);
        }
        // Wrong answer: reset player to starting position in this room
        pl.setPosition(pl.get_startpointX(), pl.get_startpointY());
      }
      if (!silentMode) {
        cls();            // Clear any artifacts (like riddle text)
        theScreen.draw(); // Redraw room after change
        drawHUD();
      }
    }
  }

  // --- Handle doors ---
  for (size_t i = 0; i < doors.size(); ++i) {
    if (doors[i].getX() == x && doors[i].getY() == y) {

      // If player holds a key (represented as 'k'), give it to the door
      if (pl.iscarrying_item() && pl.getCarryItem() == 'k') {
        pl.useKey();       // Remove key from player
        doors[i].addKey(); // Increase door key counter
      }
      bool canopen =
          doors[i].isOpen(); // Door open according to its internal logic
      bool hasSwitchPermission = false;

      bool needSwitch = false;
      for (const auto &sw : switches) {
        if (sw->isDoorSwitch()) {
          needSwitch = true;
          if (sw->isOn())
            hasSwitchPermission = true;
        }
      }
      if (needSwitch)
        canopen = canopen && hasSwitchPermission;

      if (canopen) {
        openedDoor = true;
        pl.deactivate();      // Player finished this room
        playerPassedDoor(pl); // Handle room exit logic
        if (!silentMode) {
          theScreen.draw(); // Redraw room after player exit
          drawHUD();
        }
      }

      // Door remains on map and vector; just stop further checks
      return;
    }
  }

  // --- Handle picking up items (keys, bombs, torches) ---
  // --- Handle picking up items (keys, bombs, torches) ---
  char before = theScreen.board[y][x]; // What was on this tile
  if (pl.pickupIfPossible()) { // Try to pick up (or interact if hand full)
    if (before == 'k') {       // Remove key from keys vector
      for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].getX() == x && keys[i].getY() == y) {
          keys.erase(keys.begin() + i);
          break;
        }
      }
    } else if (before == '@') { // Remove bomb item from bombItem vector
      for (size_t i = 0; i < bombItem.size(); ++i) {
        if (bombItem[i].getX() == x && bombItem[i].getY() == y) {
          bombItem.erase(bombItem.begin() + i);
          break;
        }
      }
    } else if (before == '!') { // Remove torch item from torch vector
      for (size_t i = 0; i < torches.size(); ++i) {
        if (torches[i].getX() == x && torches[i].getY() == y) {
          torches.erase(torches.begin() + i);
          break;
        }
      }
    }
  }
  activateSpringIfNeeded(pl);
}
void game::goToNextRoom() {
  if (getTimeGame() < TIME_PER_GAME) {
    setScore(score + 200); // Bonus for finishing room quickly
    if (!silentMode) {
      cls();
      std::cout << "Room completed quickly! Bonus 200 points! Current score: "
                << getScore() << std::endl;
      Sleep(2500);
    }
  } else {
    setScore(score + 50); // Standard room completion score
    if (!silentMode) {
      cls();
      std::cout << "Room completed! You get 50 points! Current score: "
                << getScore() << std::endl;
      Sleep(2500);
    }
  }
  if (p1.getLife() == 5 && p2.getLife() == 5) {
    bool Full_Life;
    setScore(score + 500);
    if (!silentMode) {
      cls();
      std::cout << "you finish the room with full life! Bonus 500 points! "
                   "Current score: "
                << getScore() << std::endl;
      Sleep(2500);
    }
  }
  ++currentRoom; // Move to next room index

  // Check if we reached the end room or beyond
  if (currentRoom >= (int)allRooms.size() - 1) {
    theScreen.setHudInfo(0, 21, false);
    // Load the last room (end screen) from files
    if (currentRoom < (int)allRooms.size()) {
      theScreen.copyFrom(allRooms[currentRoom]);
    }
    if (!silentMode) {
      theScreen.draw();
      gotoxy(38, 22);
      std::cout << getScore() << std::endl;
    }
    // Record game end
    if (resultRecorder.isActive()) {
      resultRecorder.recordGameEnd(gameCycle, getScore());
      resultRecorder.stop();
      stepRecorder.stop();
    }
    if (!silentMode && !loadMode) {
      Sleep(100000);
    } else if (!silentMode) {
      Sleep(2000);
    }
    p1.deactivate(); // Deactivate player 1
    p2.deactivate(); // Deactivate player 2
    return;
  }

  // Load the next room from the vector (read from external files)
  theScreen.copyFrom(allRooms[currentRoom]);
  p1.getIsActiveback(); // Reactivate player 1
  p1.setCarryItem(' ');
  p2.getIsActiveback(); // Reactivate player 2
  p2.setCarryItem(' ');
  keys.clear(); // Clear all old objects
  doors.clear();
  bombItem.clear();
  switches.clear();
  obstacles.clear();      // clear all old objects
  initObjectsFromBoard(); // Scan new board for objects

  setTimeGame(0);     // Reset game time for new room
  openedDoor = false; // Reset door opened flag
  switchOn = false;   // Reset switch state
  // Reset player positions for new room
  int p1x, p1y, p2x, p2y;
  findPlayerStartPositions(p1x, p1y, p2x, p2y);
  setposition_player1(p1x, p1y);
  p1.setStartPosition(p1x, p1y);
  setposition_player2(p2x, p2y);
  p2.setStartPosition(p2x, p2y);
  if (!silentMode) {
    theScreen.draw(); // Draw whole new room
    p1.draw('$');     // Draw player 1 character
    p2.draw('&');     // Draw player 2 character
    drawHUD();
    p1.updateHUD();
    p2.updateHUD();
  }
}

int game::pause_menu() {
  if (!silentMode) {
    cls(); // Clear screen and show pause menu
    std::cout << "============ PAUSED ============" << std::endl;
    std::cout << "      ESC - continue game       " << std::endl;
    std::cout << "       H  - return to main menu " << std::endl;
    std::cout << "       I - view instructions    " << std::endl;
    std::cout << "================================" << std::endl;
  }

  while (true) {
    if (loadMode) {
      // In load mode, wait for input is simulated by file source
      if (!inputSource->hasInput()) {
        Sleep(10);
        continue; // Wait for next step to become available (if any)
      }
      if (!silentMode)
        Sleep(1000); // Visual delay to see menu choice
    } else {
      // In keyboard mode, we busy wait or use _kbhit via inputSource
      if (!inputSource->hasInput()) {
        continue;
      }
    }

    // Get key from source (keyboard or file)
    char ch = inputSource->getKey();

    // Record key if saving
    if (stepRecorder.isActive() && !loadMode) {
      stepRecorder.recordStep(gameCycle, ch);
    }

    if (ch == 27) { // ESC -> continue game
      return 0;
    } else if (ch == 'h' || ch == 'H') { // h/H -> go back to main menu
      return 1;
    } else if (ch == 'i' || ch == 'I') { // i/I -> show instructions
      if (silentMode)
        continue;
      cls();
      std::cout << "=============================== Game Instructions "
                   "=============================="
                << std::endl;
      std::cout << "|                                                          "
                   "                    |"
                << std::endl;
      std::cout << "|1.There are two players participating in the game world.  "
                   "                    |"
                << std::endl;
      std::cout << "|2.The goal of each player is to reach the end of the "
                   "level and solve all      |"
                << std::endl;
      std::cout << "|  challenges.                                             "
                   "                    |"
                << std::endl;
      std::cout << "|3.Players can move using the following keys:              "
                   "                    |"
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
      std::cout << "|  elements.                                               "
                   "                    |"
                << std::endl;
      std::cout << "|                                                          "
                   "                    |"
                << std::endl;
      std::cout << "==========================================================="
                   "===================="
                << std::endl;
      std::cout << "\nPress 'b' to return to pause menu...";

      // Wait for 'b' key to return to pause menu
      // We must record ALL keys pressed here to sync the loop
      while (true) {
        if (loadMode && !inputSource->hasInput()) {
          Sleep(10);
          continue;
        }
        if (!loadMode && !inputSource->hasInput()) {
          continue;
        }

        if (loadMode && !silentMode)
          Sleep(100); // Small delay for visual effect

        char key = inputSource->getKey();

        if (stepRecorder.isActive() && !loadMode) {
          stepRecorder.recordStep(gameCycle, key);
        }

        if (key == 'b' || key == 'B') {
          break;
        }
      }
      // Re-draw pause menu
      cls();
      std::cout << "============ PAUSED ============" << std::endl;
      std::cout << "      ESC - continue game       " << std::endl;
      std::cout << "       H  - return to main menu " << std::endl;
      std::cout << "       I - view instructions    " << std::endl;
      std::cout << "================================" << std::endl;
    }
  }
}

// perplexity AI
bool game::isPlayerAt(int x, int y, const player *ignore) const {
  const player *other = nullptr; // Pointer to the other player

  if (ignore == &p1)
    other = &p2;
  else if (ignore == &p2)
    other = &p1;
  else
    return false; // ignore is not p1 or p2

  return other->getIsActive() && // Other player must be active
         other->getX() == x &&   // Same x
         other->getY() == y;     // Same y
}

bool game::isLastActivePlayer(const player &pl) const {
  int activeCount = 0; // Count how many players are active
  if (p1.getIsActive())
    ++activeCount;
  if (p2.getIsActive())
    ++activeCount;

  return (activeCount == 1) &&
         pl.getIsActive(); // True if only this player is active
}

void game::playerPassedDoor(player &pl) {
  // Hide player from current position on screen
  pl.draw(' ');

  // Record room change
  int playerNum = (&pl == &p1) ? 1 : 2;
  if (resultRecorder.isActive()) {
    resultRecorder.recordRoomChange(gameCycle, playerNum, currentRoom + 1);
  }
  if (loadMode && resultVerifier.isActive()) {
    resultVerifier.verifyRoomChange(gameCycle, playerNum, currentRoom + 1);
  }

  ++playersExitedThisRoom; // Increment counter of players who left

  // If first player exited, keep room; play continues with remaining player
  if (playersExitedThisRoom == 1) {
    return;
  }

  // If second player also exited, load next room
  if (playersExitedThisRoom == 2) {
    goToNextRoom();
    playersExitedThisRoom = 0; // Reset counter for the new room
  }
}

void game::explodeAt(int x, int y) {
  // First handle visual and board effects
  theScreen.explodeAt(x, y, theScreen.getHudY());

  // Then remove riddles within distance 3 from explosion center
  for (int dy = -3; dy <= 3; ++dy) {
    for (int dx = -3; dx <= 3; ++dx) {
      int nx = x + dx;
      int ny = y + dy;

      if (nx < 0 || nx > Screen::MAX_X || ny < 0 || ny > Screen::MAX_Y)
        continue; // Skip out-of-bounds cells

      for (size_t i = 0; i < riddles.size(); ++i) {
        if (riddles[i].getX() == nx && riddles[i].getY() == ny) {
          riddles.erase(riddles.begin() + i); // Remove riddle in blast radius
          break;
        }
      }
      for (size_t i = 0; i < doors.size(); ++i) {
        if (doors[i].getX() == nx && doors[i].getY() == ny) {
          doors.erase(doors.begin() + i); // Remove door in blast radius
          break;
        }
      }

      for (size_t i = 0; i < obstacles.size(); ++i) {
        if (obstacles[i].contains(nx, ny)) {
          obstacles.erase(obstacles.begin() + i);
          break;
        }
      }
    }
  }
  const int explosionRadius = 3;
  // For player 1
  int dx1 = p1.getX() - x;
  int dy1 = p1.getY() - y;
  int life_p1 = p1.getLife();
  if (abs(dx1) + abs(dy1) <= explosionRadius) {
    p1.setLife(life_p1 - 1);
    p1.updateHUD();
    life_p1 = p1.getLife();

    // Record life lost
    if (resultRecorder.isActive()) {
      resultRecorder.recordLifeLost(gameCycle, 1, life_p1);
    }
    if (loadMode && resultVerifier.isActive()) {
      resultVerifier.verifyLifeLost(gameCycle, 1, life_p1);
    }

    if (life_p1 <= 0)
      p1.deactivate();
  }

  // For player 2
  int dx2 = p2.getX() - x;
  int dy2 = p2.getY() - y;
  int life_p2 = p2.getLife();
  if (abs(dx2) + abs(dy2) <= explosionRadius) {
    p2.setLife(life_p2 - 1);
    p2.updateHUD();
    life_p2 = p2.getLife();

    // Record life lost
    if (resultRecorder.isActive()) {
      resultRecorder.recordLifeLost(gameCycle, 2, life_p2);
    }
    if (loadMode && resultVerifier.isActive()) {
      resultVerifier.verifyLifeLost(gameCycle, 2, life_p2);
    }

    if (life_p2 <= 0)
      p2.deactivate();
  }
}

void game::update_Internal_Wall_By_Switch(const Switch &sw) {
  int y = 15;            // Fixed row of internal wall
  const int xStart = 29; // Start x of wall segment
  const int xEnd = 32;   // End x of wall segment

  if (currentRoom != 0)
    return; // Safety: only in room 0

  for (size_t i = 0; i < doors.size(); ++i) {
    if (doors[i].getSymbol() == '2' && theScreen.getHudY() == 0) {
      y = doors[i].getY() - 2;
      break;
    }
    // cls();
    // std::cout << "Switching wall at row " << y << std::endl;
    // Sleep(1000);
  }
  if (sw.isOn()) {
    // ON -> remove wall (replace with spaces)
    for (int x = xStart; x <= xEnd; ++x) {
      theScreen.setChar(y, x, ' ');
      gotoxy(x, y);
      std::cout << ' ' << std::flush;
    }
  } else {
    // OFF -> restore wall (character 'W')
    for (int x = xStart; x <= xEnd; ++x) {
      theScreen.setChar(y, x, 'W');
      theScreen.drawChar(x, y, 'W', Color::LIGHT_CYAN);
    }
  }
}

player *game::getOtherPlayer(const player *pl) {
  if (pl == &p1)
    return &p2;
  if (pl == &p2)
    return &p1;
  return nullptr;
}

int game::countSpringCompressed(const player &pl) const {
  Direction d = pl.getPoint().getDir();
  int dx = d.dx();
  int dy = d.dy();

  if (dx == 0 && dy == 0)
    return 0;

  int x = pl.getX();
  int y = pl.getY();

  int count = 1;

  while (true) {
    x -= dx;
    y -= dy;

    if (x < 0 || x >= Screen::MAX_X || y < 0 || y >= Screen::MAX_Y)
      break;

    char ch = theScreen.board[y][x];

    if (ch == '#') {
      ++count;
    } else {
      break;
    }
  }

  return count;
}

void game::activateSpringIfNeeded(player &pl) {
  if (pl.isUnderSpringEffect())
    return;

  int x = pl.getX();
  int y = pl.getY();

  // 1. must stand on spring cell
  if (theScreen.board[y][x] != '#')
    return;

  Direction inDir = pl.getPoint().getDir();
  int dx = inDir.dx();
  int dy = inDir.dy();

  if (dx == 0 && dy == 0)
    return; // not moving into spring

  int nextX = x + dx;
  int nextY = y + dy;

  // 2. if next cell is also '#', we are still compressing -> do nothing yet
  if (nextX >= 0 && nextX <= Screen::MAX_X && nextY >= 0 &&
      nextY <= Screen::MAX_Y && theScreen.board[nextY][nextX] == '#') {
    return;
  }
  if (theScreen.board[nextY][nextX] == 'W') {
    int k = countSpringCompressed(
        pl); // Check how many spring cells are behind the player
    if (k <= 0)
      return;

    // release direction = opposite to compression
    int outDx = -dx;
    int outDy = -dy;
    Direction outDir(outDx, outDy);

    int speed = k;
    int ticks = k * k;

    pl.setSpringEffect(speed, ticks, outDir);
    pl.setDirection(outDir);
  }
}

// perplexity AI
bool game::tryPushObstacle(point &target, Direction dir) {
  // Try to find if an obstacle is at the target position
  Obstacle *found = nullptr;
  for (auto &obs : obstacles) {
    if (obs.contains(target.getX(), target.getY())) {
      found = &obs;
      break;
    }
  }
  if (!found)
    return false;

  Obstacle &obs = *found;
  int dx = dir.dx();
  int dy = dir.dy();

  int totalForce = 0;

  // Calculate total force: how many players are pushing?
  auto contributes = [&](player &pl) -> int {
    if (!pl.getIsActive())
      return 0;

    // Check if player is moving in the push direction
    if (pl.getPoint().getDir().dx() != dx || pl.getPoint().getDir().dy() != dy)
      return 0;

    // Check "front" cell (cell in front of player)
    int fx = pl.getX() + dx;
    int fy = pl.getY() + dy;

    // If front cell is not part of this obstacle, player doesn't push it
    if (!obs.contains(fx, fy))
      return 0;

    // Return push force (spring effect increases force)
    return pl.isUnderSpringEffect() ? pl.getSpringSpeed() : 1;
  };

  totalForce += contributes(p1);
  totalForce += contributes(p2);

  // If there is any force
  if (totalForce <= 0)
    return false;

  // If total force is enough and obstacle can be pushed
  if (totalForce >= obs.getSize() && obs.canPush(dir, theScreen)) {
    obs.push(dir, theScreen);
    mergeObstaclesOnBoard();
    return true;
  }

  return false;
}

// perplexity AI
void game::mergeObstaclesOnBoard() {
  // Re-scan the board to merge adjacent obstacle cells
  obstacles.clear();

  // Keep track of visited cells to form clusters
  bool visited[Screen::MAX_Y + 1][Screen::MAX_X + 1] = {false};

  for (int y = 0; y <= Screen::MAX_Y; ++y) {
    for (int x = 0; x <= Screen::MAX_X; ++x) {
      if (theScreen.board[y][x] == '*' && !visited[y][x]) {
        // BFS to find all connected parts
        std::vector<point> cluster;
        std::queue<std::pair<int, int>> q;
        q.push({x, y});
        visited[y][x] = true;

        while (!q.empty()) {
          auto [cx, cy] = q.front();
          q.pop();
          cluster.emplace_back(cx, cy, Direction::directions[Direction::STAY],
                               '*');

          const int dx[4] = {1, -1, 0, 0};
          const int dy[4] = {0, 0, 1, -1};

          for (int k = 0; k < 4; ++k) {
            int nx = cx + dx[k];
            int ny = cy + dy[k];
            if (nx < 0 || nx > Screen::MAX_X || ny < 0 || ny > Screen::MAX_Y)
              continue;
            if (theScreen.board[ny][nx] == '*' && !visited[ny][nx]) {
              visited[ny][nx] = true;
              q.push({nx, ny});
            }
          }
        }

        // Store cluster into a new Obstacle object
        Obstacle newObs(0, 0, theScreen, this); // Create temporary obstacle
        newObs.getCells().clear();
        newObs.setSize(0);

        for (auto &p : cluster) {
          newObs.getCells().push_back(p);
        }
        newObs.setSize((int)cluster.size());

        obstacles.push_back(newObs);
      }
    }
  }
}

bool game::isHudArea(int x, int y) const {
  if (!theScreen.isHudActive())
    return false;
  int hX = theScreen.getHudX();
  int hY = theScreen.getHudY();
  return (x >= hX && x < hX + 67 && // 20 columns
          y >= hY && y < hY + 3);   // 3 rows
}

bool game::findLegendPosition() {
  Screen &currentScreen = getCurrentRoomScreen();
  for (int y = 0; y <= Screen::MAX_Y; ++y) {
    for (int x = 0; x <= Screen::MAX_X; ++x) {
      if (currentScreen.board[y][x] == 'L') {
        currentScreen.setHudInfo(x, y, true);
        return true;
      }
    }
  }
  currentScreen.setHudInfo(0, 0, false);
  return false; // No L found
}

// perplexity AI
bool game::loadSingleScreen(const std::string &filename, int roomIndex) {
  std::ifstream in(filename);
  if (!in) {
    return false; // file not found / cannot open
  }
  Screen room; // local screen to fill

  std::string line;
  for (int y = 0; y <= Screen::MAX_Y; ++y) {
    if (!std::getline(in, line)) {
      line = ""; // Use blank line if file ends early
    }

    // Pad / trim to exactly MAX_X+1 characters
    if ((int)line.size() < Screen::MAX_X)
      line.append(Screen::MAX_X - line.size(), ' ');
    else if ((int)line.size() > Screen::MAX_X)
      line = line.substr(0, Screen::MAX_X);

    for (int x = 0; x <= Screen::MAX_X; ++x) {
      char ch = line[x];

      if (ch == 'L') { // Legend position
        room.setHudInfo(x, y, true);
        ch = ' ';
      }

      if (ch == 'D') { // Dark area
        if (roomIndex == 0) {
          const int DARK_WIDTH = 10;
          const int DARK_HEIGHT = 10;
          room.markDarkRect(x, y, DARK_WIDTH, DARK_HEIGHT);
          ch = ' ';
        } else if (roomIndex == 2) {
          const int DARK_WIDTH = 15;
          const int DARK_HEIGHT = 7;
          room.markDarkRect(x, y, DARK_WIDTH, DARK_HEIGHT);
          ch = ' ';
        }
      }

      room.board[y][x] = ch;
    }
  }

  // Read configuration lines after the board
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;
    if (line.rfind("KEYS:", 0) == 0 && line.size() >= 8) {
      size_t eq = line.find('=');
      if (eq != std::string::npos && eq > 5) {
        doorKeyOverrides[roomIndex].push_back(
            {line[5], std::stoi(line.substr(eq + 1))});
      }
    }
  }

  if ((int)allRooms.size() <= roomIndex)
    allRooms.resize(roomIndex + 1);

  allRooms[roomIndex] = room;
  return true;
}

bool game::loadScreensFromFiles(std::string &errorMsg) {
  allRooms.clear();
  screenFileNames.clear();
  doorKeyOverrides.clear();
  doorKeyOverrides.resize(100); // Support up to 100 rooms (0-99)

  for (int i = 1; i <= 99; ++i) {
    std::string filename = (i < 10)
                               ? "adv-world0" + std::to_string(i) + ".screen"
                               : "adv-world" + std::to_string(i) + ".screen";

    std::ifstream test(filename);
    if (!test)
      break;
    test.close();

    if (!loadSingleScreen(filename, i - 1)) {
      errorMsg = "Failed to load: " + filename;
      return false;
    }
    screenFileNames.push_back(filename);
  }

  if (screenFileNames.empty()) {
    errorMsg = "No screen files found.";
    return false;
  }
  return true;
}

void game::findPlayerStartPositions(int &p1x, int &p1y, int &p2x, int &p2y) {
  bool foundP1 = false, foundP2 = false;

  for (int y = 1; y < Screen::MAX_Y && (!foundP1 || !foundP2); ++y) {
    for (int x = 1; x < Screen::MAX_X && (!foundP1 || !foundP2); ++x) {
      char ch = theScreen.board[y][x];
      if (ch == '$' && !foundP1) {
        p1x = x;
        p1y = y;
        foundP1 = true;
        theScreen.setChar(y, x, ' ');
      } else if (ch == '&' && !foundP2) {
        p2x = x;
        p2y = y;
        foundP2 = true;
        theScreen.setChar(y, x, ' ');
      }
    }
  }

  if (!foundP1 || !foundP2) {
    for (int y = 1; y < Screen::MAX_Y && (!foundP1 || !foundP2); ++y) {
      for (int x = 1; x < Screen::MAX_X && (!foundP1 || !foundP2); ++x) {
        if (theScreen.board[y][x] == ' ' && !isHudArea(x, y)) {
          if (!foundP1) {
            p1x = x;
            p1y = y;
            foundP1 = true;
          } else if (!foundP2 && (x != p1x || y != p1y)) {
            p2x = x;
            p2y = y;
            foundP2 = true;
          }
        }
      }
    }
  }
  if (!foundP1) {
    p1x = 2;
    p1y = 2;
  }
  if (!foundP2) {
    p2x = 3;
    p2y = 2;
  }
}

void game::drawHUD() const {
  if (silentMode || !theScreen.isHudActive())
    return;

  int hX = theScreen.getHudX();
  int hY = theScreen.getHudY();
  const int maxWidth = 62;

  {
    gotoxy(hX, hY);
    std::string line = "P1($) carry item:    life: ";
    if ((int)line.size() > maxWidth)
      line = line.substr(0, maxWidth);
    std::cout << line << std::flush;
  }

  //  1: P2 item + HP
  {
    gotoxy(hX, hY + 1);
    std::string line = "P2(&) carry item:    life: ";
    if ((int)line.size() > maxWidth)
      line = line.substr(0, maxWidth);
    std::cout << line << std::flush;
  }

  //  2: Door/Switch state
  {
    gotoxy(hX, hY + 2);

    std::string line = "Door open? : ";
    line += (openedDoor ? 'Y' : 'N');
    line += " || Switch open? : ";
    line += (switchOn ? 'Y' : 'N');
    line += " || Score: " + std::to_string(getScore());
    line += " || Time left: ";
    if ((int)line.size() > maxWidth)
      line = line.substr(0, maxWidth);
    std::cout << line << std::flush;
  }
}

// perplexity AI
bool game::loadRiddlesFromFile(const std::string &filename) {
  std::ifstream in(filename);
  if (!in) {
    return false;
  }

  all_riddles.clear();
  std::string line;
  RiddleData currentRiddle;
  int optionIndex = 0;
  bool inRiddle = false;

  while (std::getline(in, line)) {
    if (line.empty())
      continue;

    // Remove CR if present (Windows/Unix line ending diffs)
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line == "[RIDDLE]") {
      if (inRiddle) {
        // Save previous riddle
        all_riddles.push_back(currentRiddle);
      }
      // Reset for new riddle
      currentRiddle = RiddleData();
      currentRiddle.correctIndex = 0;
      optionIndex = 0;
      inRiddle = true;
    } else if (inRiddle) {
      if (line[0] == '*') {
        // It's an option
        if (optionIndex < 4) {
          currentRiddle.options[optionIndex] = line.substr(1);
          optionIndex++;
        }
      } else if (line.rfind("Correct:", 0) == 0) { // Starts with "Correct:"
        int val = std::stoi(line.substr(8));
        currentRiddle.correctIndex = val - 1; // Convert 1-based to 0-based
      } else {
        // It's part of the question
        if (!currentRiddle.question.empty()) {
          currentRiddle.question += "\n";
        }
        currentRiddle.question += line;
      }
    }
  }
  // Save last riddle
  if (inRiddle) {
    all_riddles.push_back(currentRiddle);
  }

  return !all_riddles.empty();
}
