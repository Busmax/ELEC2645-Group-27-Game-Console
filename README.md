# ELEC2645 Group 27 Game Console

This repository contains our ELEC2645 Unit 4 group project.  
The project is an STM32 Nucleo-based mini game console with a shared menu system and three playable games. The games run on the LCD screen and use the joystick, push buttons, buzzer, and LED/PWM feedback.

## Project Overview

The aim of this project was to create a small embedded game console where different games can be selected from one shared menu. Each group member developed one game, and the final version combines all games into a single program that runs on the Nucleo board.

The final game console includes:

- A shared main menu system
- Three individual games
- LCD graphics output
- Joystick movement control
- Button input
- Buzzer sound effects
- LED/PWM feedback
- Score, lives, and game state systems

## Games Included

### Game 1: Firewall Defender

Firewall Defender is a retro-style top-down shooter game. The player moves around the screen, avoids enemies, shoots targets, collects power-ups, and uses special abilities during gameplay.

Main features:

- Player movement and shooting
- Multiple enemy behaviours
- Score and lives system
- Power-ups and special abilities
- Explosion effects
- Buzzer feedback
- Retro pixel-style graphics

### Game 2: Dodge Dash

Dodge Dash is a reaction-based dodging game. The player controls a small character and tries to avoid falling obstacles for as long as possible.

Main features:

- Joystick-controlled player movement
- Random falling obstacles
- Increasing difficulty
- Score and lives system
- Shield ability using button input
- Game over and restart screen
- Buzzer and LED/PWM feedback

### Game 3: Submarine Rescue

Submarine Rescue is an underwater rescue game. The player controls a submarine, rescues divers, returns them to the base, avoids sea mines, and manages oxygen.

Main features:

- Submarine movement using joystick
- Diver rescue objective
- Rescue base system
- Moving sea mines
- Oxygen bar and low oxygen warning
- Sonar ability
- Score, lives, and rescued diver count
- Buzzer and LED/PWM feedback

## Hardware Used

The project was developed and tested using the following hardware:

- STM32 Nucleo development board
- LCD display
- Joystick module
- Push buttons
- Buzzer
- LED / PWM output
- Breadboard and jumper wires

## Basic Controls

The controls may vary slightly between games, but the general control system is:

| Control | Function |
|---|---|
| Joystick | Move player / navigate menu |
| Button input | Start game / activate ability / restart |
| BT2 | Special ability in some games |
| BT3 | Return to main menu |
| Buzzer | Sound feedback |
| LED/PWM | Visual hardware feedback |

## Software Structure

The project is organised into shared system files and individual game files.

```text
Project Root
│
├── main.c
├── Menu.c / Menu.h
├── LCD.c / LCD.h
├── Joystick.c / Joystick.h
├── InputHandler.c / InputHandler.h
├── Buzzer.c / Buzzer.h
├── PWM.c / PWM.h
│
├── Game_1.c / Game_1.h
├── Game_2.c / Game_2.h
├── Game_3.c / Game_3.h
│
└── README.md
