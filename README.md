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

Firewall Defender was developed by **XINGQI LIU**.

This game is a retro-style top-down shooter. The player controls a character inside a small arena and needs to defend against enemies. The game includes shooting, enemy movement, score, lives, power-ups, and special skills.

Main features:

- Retro pixel-style graphics
- Player movement and shooting
- Different enemy behaviours
- Score and lives system
- Power-ups during gameplay
- Special abilities such as EMP and overdrive
- Explosion effects
- Buzzer sound feedback
- LED/PWM feedback

### Game 2: Dodge Dash

Dodge Dash was developed by **YI CHENG CHE**.

This game is a fast reaction dodging game. The player controls a small character and tries to avoid falling obstacles for as long as possible. The longer the player survives, the higher the score becomes.

Main features:

- Joystick-controlled player movement
- Random falling obstacles
- Increasing difficulty over time
- Score and lives system
- Shield ability using button input
- Shield cooldown system
- Game over and restart screen
- Buzzer feedback
- LED/PWM feedback

### Game 3: Submarine Rescue

Submarine Rescue was developed by **SHUO NIU**.

This game is an underwater rescue game. The player controls a submarine, rescues divers, returns them to the base, avoids moving sea mines, and manages oxygen. The game also includes a sonar ability and different hardware feedback effects.

Main features:

- Submarine movement using joystick
- Diver rescue objective
- Rescue base system
- Moving sea mines
- Oxygen bar and low oxygen warning
- Oxygen tank pickup
- Sonar ability
- Score, lives, and rescued diver count
- Buzzer sound effects
- LED/PWM warning feedback

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

The exact controls may vary slightly between games, but the general control system is:

| Control | Function |
|---|---|
| Joystick | Move player / navigate menu |
| Button input | Start game / activate ability / restart |
| BT2 | Special ability or restart in some games |
| BT3 | Return to main menu |
| Buzzer | Sound feedback |
| LED/PWM | Visual hardware feedback |

## Software Structure

The project is organised into shared system files and individual game files.

```text
Project Root
|
|-- main.c
|-- Menu.c / Menu.h
|-- LCD.c / LCD.h
|-- Joystick.c / Joystick.h
|-- InputHandler.c / InputHandler.h
|-- Buzzer.c / Buzzer.h
|-- PWM.c / PWM.h
|
|-- Game_1.c / Game_1.h
|-- Game_2.c / Game_2.h
|-- Game_3.c / Game_3.h
|
|-- README.md
```

Each game is stored in its own source file so that each group member could work on their own section of the project. The games are then connected through the shared menu system.

## Group Work and Integration

The final project was designed to work as one complete game console rather than three separate programs. Each member developed one game, and then the games were combined into the same project structure.

The shared menu system allows the user to choose between the three games. Each game uses the same LCD screen, joystick, push buttons, buzzer, and LED/PWM output. This helped keep the final project consistent and easier to demonstrate.

During integration, we tested that:

- Each game could be selected from the main menu
- Each game could run on the physical Nucleo board
- The joystick and buttons worked correctly
- The LCD display updated properly
- The buzzer and LED/PWM feedback worked
- The player could return to the main menu after playing
- The final project could compile as one complete program

## How to Run

1. Open the project in the STM32 development environment.
2. Make sure all source and header files are included in the project.
3. Connect the STM32 Nucleo board to the computer.
4. Build or compile the project.
5. Flash the program to the Nucleo board.
6. Use the joystick and buttons to select and play the games from the main menu.

## Group Members

| Name | Main Contribution |
|---|---|
| XINGQI LIU | Game 1: Firewall Defender |
| YICHENG CHE | Game 2: Dodge Dash |
| SHUO NIU | Game 3: Submarine Rescue |

## Final Submission

This repository is part of the final ELEC2645 Unit 4 group submission.

The final submission includes:

- Final source code
- GitHub repository
- Group project video
- Group journal entries
- Release version post
- Group software design post
- Group project video post

## Notes

The project was tested on the physical Nucleo board. Some gameplay values, such as enemy speed, obstacle speed, oxygen drain, sound effects, and ability cooldown times, were adjusted during testing to make the games more playable on the small LCD screen and embedded hardware.

The final version is not just a code demo. It is a working embedded game console with three playable games, shared hardware controls, visual display output, and physical feedback through the buzzer and LED/PWM output.

## License

This project is for university coursework submission only.
