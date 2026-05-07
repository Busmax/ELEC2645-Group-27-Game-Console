#include "Game_2.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "Joystick.h"
#include "PWM.h"
#include "Buzzer.h"
#include "stm32l4xx_hal.h"

#include <stdint.h>
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern PWM_cfg_t pwm_cfg;
extern Buzzer_cfg_t buzzer_cfg;

 

#define SCREEN_W 240
#define SCREEN_H 240
#define HUD_H 28

#define GAME2_FRAME_TIME_MS 33
#define PLAYER_SIZE 12
#define PLAYER_SPEED 5
#define OBSTACLE_COUNT 6
#define STARTING_LIVES 3
#define HIT_INVULN_MS 900
#define SHIELD_TIME_MS 1000
#define SHIELD_COOLDOWN_MS 5000

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t w;
    uint8_t h;
    uint8_t speed;
} Obstacle;

static Obstacle obstacles[OBSTACLE_COUNT];
static int16_t player_x;
static int16_t player_y;
static uint32_t score;
static uint8_t lives;
static uint8_t game_over;
static uint32_t rng_state;
static uint32_t invulnerable_until;
static uint32_t shield_until;
static uint32_t shield_ready_at;
static uint32_t sound_until;

static uint32_t rand_next(void)
{
    rng_state = (rng_state * 1664525u) + 1013904223u;
    return rng_state;
}

static uint8_t current_base_speed(void)
{
    uint8_t speed = 2 + (uint8_t)(score / 250u);
    if (speed > 7) {
        speed = 7;
    }
    return speed;
}

static void start_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    buzzer_tone(&buzzer_cfg, freq_hz, 25);
    sound_until = HAL_GetTick() + duration_ms;
}

static void update_sound(void)
{
    if (sound_until != 0u && HAL_GetTick() >= sound_until) {
        buzzer_off(&buzzer_cfg);
        sound_until = 0u;
    }
}

static void draw_rect_safe(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour)
{
    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + w;
    int16_t y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;

    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    for (int16_t row = y0; row < y1; row++) {
        LCD_Draw_Line((uint16_t)x0, (uint16_t)row, (uint16_t)(x1 - 1), (uint16_t)row, colour);
    }
}

static void reset_obstacle(uint8_t index, uint8_t stagger)
{
    Obstacle *ob = &obstacles[index];
    ob->w = 10u + (uint8_t)(rand_next() % 18u);
    ob->h = 10u + (uint8_t)(rand_next() % 18u);
    ob->x = (int16_t)(rand_next() % (uint32_t)(SCREEN_W - ob->w));
    ob->y = HUD_H + 2;

    if (stagger) {
        ob->y += (int16_t)(index * 33u);
    }

    ob->speed = current_base_speed() + (uint8_t)(rand_next() % 3u);
}

static uint8_t rectangles_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                                  int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

static void reset_game(void)
{
    rng_state = HAL_GetTick() ^ 0x4D324447u;
    player_x = (SCREEN_W - PLAYER_SIZE) / 2;
    player_y = SCREEN_H - PLAYER_SIZE - 8;
    score = 0;
    lives = STARTING_LIVES;
    game_over = 0;
    invulnerable_until = 0;
    shield_until = 0;
    shield_ready_at = 0;
    sound_until = 0;

    for (uint8_t i = 0; i < OBSTACLE_COUNT; i++) {
        reset_obstacle(i, 1);
    }

    PWM_SetDuty(&pwm_cfg, 70);
    start_tone(NOTE_C5, 70);
}

static void update_player(void)
{
    Joystick_Read(&joystick_cfg, &joystick_data);

    player_x += (int16_t)(joystick_data.coord_mapped.x * PLAYER_SPEED);
    player_y -= (int16_t)(joystick_data.coord_mapped.y * PLAYER_SPEED);

    if (player_x < 0) player_x = 0;
    if (player_x > SCREEN_W - PLAYER_SIZE) player_x = SCREEN_W - PLAYER_SIZE;
    if (player_y < HUD_H + 2) player_y = HUD_H + 2;
    if (player_y > SCREEN_H - PLAYER_SIZE) player_y = SCREEN_H - PLAYER_SIZE;
}

static void update_obstacles(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t has_protection = (now < invulnerable_until) || (now < shield_until);

    for (uint8_t i = 0; i < OBSTACLE_COUNT; i++) {
        Obstacle *ob = &obstacles[i];
        ob->y += ob->speed;

        if (ob->y >= SCREEN_H) {
            score += 10u;
            reset_obstacle(i, 0);
            continue;
        }

        if (!has_protection && rectangles_overlap(player_x, player_y, PLAYER_SIZE, PLAYER_SIZE,
                                                  ob->x, ob->y, ob->w, ob->h)) {
            if (lives > 0) {
                lives--;
            }

            invulnerable_until = now + HIT_INVULN_MS;
            reset_obstacle(i, 0);
            start_tone((lives == 0) ? NOTE_C4 : NOTE_A4, 120);

            if (lives == 0) {
                game_over = 1;
                PWM_SetDuty(&pwm_cfg, 10);
                start_tone(NOTE_C4, 300);
            }
        }
    }
}

static void render_hud(void)
{
    char text[24];

    draw_rect_safe(0, 0, SCREEN_W, HUD_H, 9);
    LCD_printString("DODGE DASH", 5, 5, 1, 1);

    sprintf(text, "S:%lu", (unsigned long)score);
    LCD_printString(text, 96, 5, 1, 1);

    sprintf(text, "L:%u", (unsigned)lives);
    LCD_printString(text, 184, 5, 1, 1);

    if (HAL_GetTick() < shield_until) {
        LCD_printString("SHIELD", 5, 18, 14, 1);
    } else if (HAL_GetTick() >= shield_ready_at) {
        LCD_printString("BT2 READY", 5, 18, 3, 1);
    } else {
        uint32_t seconds = (shield_ready_at - HAL_GetTick() + 999u) / 1000u;
        sprintf(text, "BT2:%lus", (unsigned long)seconds);
        LCD_printString(text, 5, 18, 6, 1);
    }

    LCD_Draw_Line(0, HUD_H, SCREEN_W - 1, HUD_H, 1);
}

static void render_game(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t player_colour = 3;

    LCD_Fill_Buffer(0);
    render_hud();

    for (uint8_t i = 0; i < OBSTACLE_COUNT; i++) {
        draw_rect_safe(obstacles[i].x, obstacles[i].y, obstacles[i].w, obstacles[i].h, 2);
    }

    if (now < shield_until) {
        player_colour = 14;
        draw_rect_safe(player_x - 3, player_y - 3, PLAYER_SIZE + 6, PLAYER_SIZE + 6, 8);
    } else if (now < invulnerable_until && ((now / 120u) & 1u)) {
        player_colour = 6;
    }

    draw_rect_safe(player_x, player_y, PLAYER_SIZE, PLAYER_SIZE, player_colour);
    LCD_Refresh(&cfg0);
}

static void render_game_over(void)
{
    char text[28];

    LCD_Fill_Buffer(0);
    LCD_printString("GAME OVER", 38, 50, 2, 3);

    sprintf(text, "Score: %lu", (unsigned long)score);
    LCD_printString(text, 55, 105, 1, 2);

    LCD_printString("BT2 Restart", 54, 160, 3, 2);
    LCD_printString("BT3 Menu", 68, 190, 6, 2);

    LCD_Refresh(&cfg0);
}

MenuState Game2_Run(void)
{
    MenuState exit_state = MENU_STATE_HOME;

    reset_game();

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        update_sound();

        if (current_input.btn3_pressed) {
            buzzer_off(&buzzer_cfg);
            PWM_SetDuty(&pwm_cfg, 50);
            exit_state = MENU_STATE_HOME;
            break;
        }

        if (game_over) {
            if (current_input.btn2_pressed) {
                reset_game();
            }
            render_game_over();
        } else {
            uint32_t now = HAL_GetTick();

            if (current_input.btn2_pressed && now >= shield_ready_at) {
                shield_until = now + SHIELD_TIME_MS;
                shield_ready_at = now + SHIELD_COOLDOWN_MS;
                start_tone(NOTE_E5, 80);
            }

            score++;
            update_player();
            update_obstacles();

            uint8_t brightness = 20u + (lives * 20u);
            if (now < shield_until) {
                brightness = 95;
            }
            PWM_SetDuty(&pwm_cfg, brightness);

            render_game();
        }

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME2_FRAME_TIME_MS) {
            HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
        }
    }

    return exit_state;
}
