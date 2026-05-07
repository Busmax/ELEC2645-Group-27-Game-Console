#include "main.h"
#include "LCD.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "Menu.h"
#include "PWM.h"
#include "Buzzer.h"
#include "Game_3.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
 *  1. Small adapter section
 * ============================================================ */
extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern PWM_cfg_t pwm_cfg;
extern Buzzer_cfg_t buzzer_cfg;

/* Current joystick value used by this game. */
static UserInput sub_input;

#define SUB_LCD_CLEAR(colour)       LCD_Fill_Buffer((colour))
#define SUB_LCD_PIXEL(x, y, colour) LCD_Draw_Line((x), (y), (x), (y), (colour))
#define SUB_LCD_FLUSH()             LCD_Refresh(&cfg0)

/* Input adapter.
 */
#define SUB_JOY_DIR                 (sub_input.direction)
#define SUB_BTN_SONAR               (current_input.btn2_pressed)
#define SUB_BTN_MENU                (current_input.btn3_pressed)
#define SUB_BTN_RESTART             (current_input.btn2_pressed)

#define SUB_INPUT_UP()              (SUB_JOY_DIR == N  || SUB_JOY_DIR == NE || SUB_JOY_DIR == NW)
#define SUB_INPUT_DOWN()            (SUB_JOY_DIR == S  || SUB_JOY_DIR == SE || SUB_JOY_DIR == SW)
#define SUB_INPUT_LEFT()            (SUB_JOY_DIR == W  || SUB_JOY_DIR == NW || SUB_JOY_DIR == SW)
#define SUB_INPUT_RIGHT()           (SUB_JOY_DIR == E  || SUB_JOY_DIR == NE || SUB_JOY_DIR == SE)

static uint32_t buzzer_stop_time = 0;

static void sub_beep(uint16_t freq_hz, uint16_t ms)
{
    if (freq_hz == 0 || ms == 0) {
        buzzer_off(&buzzer_cfg);
        buzzer_stop_time = 0;
        return;
    }

    buzzer_tone(&buzzer_cfg, freq_hz, 35);
    buzzer_stop_time = HAL_GetTick() + (uint32_t)ms;
}

static void sub_buzzer_update(void)
{
    if (buzzer_stop_time != 0 && (int32_t)(HAL_GetTick() - buzzer_stop_time) >= 0) {
        buzzer_off(&buzzer_cfg);
        buzzer_stop_time = 0;
    }
}

static void sub_led_level(uint8_t level)
{
    if (level > 100) level = 100;
    PWM_SetDuty(&pwm_cfg, level);
}

/* ============================================================
 *  2. Display settings and colours
 * ============================================================ */

#define SCREEN_W        240
#define SCREEN_H        240
#define HUD_H           34
#define PLAY_TOP        HUD_H
#define PLAY_BOTTOM     (SCREEN_H - 1)

#define RGB565(r,g,b)   (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define C_BLACK         RGB565(0, 0, 0)
#define C_WHITE         RGB565(255, 255, 255)
#define C_WATER_DARK    RGB565(2, 16, 45)
#define C_WATER         RGB565(3, 42, 95)
#define C_WATER_LINE    RGB565(4, 75, 135)
#define C_CYAN          RGB565(60, 230, 255)
#define C_BLUE          RGB565(20, 105, 220)
#define C_YELLOW        RGB565(245, 200, 45)
#define C_ORANGE        RGB565(245, 135, 35)
#define C_RED           RGB565(235, 45, 45)
#define C_GREEN         RGB565(65, 230, 110)
#define C_DARK_GREEN    RGB565(20, 120, 70)
#define C_GREY          RGB565(130, 145, 155)
#define C_DARK_GREY     RGB565(55, 68, 80)
#define C_PURPLE        RGB565(145, 80, 230)
#define C_SEAWEED       RGB565(35, 165, 95)
#define C_SAND          RGB565(135, 105, 65)

/* ============================================================
 *  3. Tiny drawing functions
 *  Uses only LCD pixel + clear + flush, so it does not depend on text library.
 * ============================================================ */

static void px(int x, int y, uint16_t c)
{
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    SUB_LCD_PIXEL((uint16_t)x, (uint16_t)y, c);
}

static void fill_rect(int x, int y, int w, int h, uint16_t c)
{
    if (w <= 0 || h <= 0) return;

    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > SCREEN_W) x2 = SCREEN_W;
    if (y2 > SCREEN_H) y2 = SCREEN_H;

    for (int yy = y; yy < y2; yy++) {
        for (int xx = x; xx < x2; xx++) {
            px(xx, yy, c);
        }
    }
}

static void draw_rect(int x, int y, int w, int h, uint16_t c)
{
    for (int i = 0; i < w; i++) {
        px(x + i, y, c);
        px(x + i, y + h - 1, c);
    }
    for (int i = 0; i < h; i++) {
        px(x, y + i, c);
        px(x + w - 1, y + i, c);
    }
}

static void draw_circle(int cx, int cy, int r, uint16_t c)
{
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y) {
        px(cx + x, cy + y, c); px(cx + y, cy + x, c);
        px(cx - y, cy + x, c); px(cx - x, cy + y, c);
        px(cx - x, cy - y, c); px(cx - y, cy - x, c);
        px(cx + y, cy - x, c); px(cx + x, cy - y, c);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static int iabs_small(int v)
{
    return (v < 0) ? -v : v;
}

static void draw_dotted_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = iabs_small(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -iabs_small(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int step = 0;

    while (1) {
        if ((step % 9) < 4) px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        step++;
    }
}

static void fill_circle(int cx, int cy, int r, uint16_t c)
{
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                px(cx + x, cy + y, c);
            }
        }
    }
}

/* 5x7 font, only digits and capital letters are used. */
typedef struct {
    char ch;
    uint8_t col[5];
} FontChar;

static const FontChar font5x7[] = {
    {'0',{0x3E,0x51,0x49,0x45,0x3E}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}}, {'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}}, {'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}}, {'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}}, {'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'I',{0x00,0x41,0x7F,0x41,0x00}}, {'J',{0x20,0x40,0x41,0x3F,0x01}},
    {'K',{0x7F,0x08,0x14,0x22,0x41}}, {'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x0C,0x02,0x7F}}, {'N',{0x7F,0x04,0x08,0x10,0x7F}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}}, {'P',{0x7F,0x09,0x09,0x09,0x06}},
    {'Q',{0x3E,0x41,0x51,0x21,0x5E}}, {'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}}, {'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}}, {'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {'W',{0x7F,0x20,0x18,0x20,0x7F}}, {'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x07,0x08,0x70,0x08,0x07}}, {'Z',{0x61,0x51,0x49,0x45,0x43}},
    {' ',{0x00,0x00,0x00,0x00,0x00}}, {'-',{0x08,0x08,0x08,0x08,0x08}},
    {':',{0x00,0x36,0x36,0x00,0x00}}, {'!',{0x00,0x00,0x5F,0x00,0x00}}
};

static const uint8_t* get_char_pattern(char ch)
{
    for (uint32_t i = 0; i < sizeof(font5x7) / sizeof(font5x7[0]); i++) {
        if (font5x7[i].ch == ch) return font5x7[i].col;
    }
    return font5x7[sizeof(font5x7)/sizeof(font5x7[0]) - 3].col; // space
}

static void draw_char(int x, int y, char ch, uint16_t c, int scale)
{
    const uint8_t* p = get_char_pattern(ch);
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (p[col] & (1 << row)) {
                fill_rect(x + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
}

static void draw_text(int x, int y, const char* s, uint16_t c, int scale)
{
    while (*s) {
        draw_char(x, y, *s, c, scale);
        x += 6 * scale;
        s++;
    }
}

static void draw_number(int x, int y, int num, uint16_t c, int scale)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", num);
    draw_text(x, y, buf, c, scale);
}

/* ============================================================
 *  4. Game data
 * ============================================================ */

typedef struct {
    int x, y, w, h;
} Rect;

typedef struct {
    int x, y;
    int vx, vy;
    int size;
} Mine;

typedef struct {
    int x, y;
    uint8_t active;
} Item;

typedef enum {
    SUB_STATE_TITLE = 0,
    SUB_STATE_PLAYING,
    SUB_STATE_GAME_OVER
} SubState;

static SubState state;
static Rect player;
static Rect base;
static Item diver;
static Item oxygen_tank;
static Mine mines[4];

static uint8_t carrying_diver;
static uint8_t lives;
static int score;
static int rescued;
static int oxygen;
static int sonar_timer;
static int sonar_cooldown;
static int invuln_timer;
static int difficulty;
static int frame_count;
static uint32_t last_update_ms;
static uint32_t rng_state;
static uint8_t exit_game;
static uint32_t run_start_ms;
static int high_score = 0;
static uint8_t banner_timer;
static uint16_t banner_colour;
static char banner_text[18];

/* ============================================================
 *  5. Utility and game logic
 * ============================================================ */

static uint16_t rnd(uint16_t max)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    if (max == 0) return 0;
    return (uint16_t)((rng_state >> 16) % max);
}

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void set_banner(const char* text, uint16_t colour)
{
    strncpy(banner_text, text, sizeof(banner_text) - 1);
    banner_text[sizeof(banner_text) - 1] = '\0';
    banner_colour = colour;
    banner_timer = 28;  
}

static void enter_game_over(void)
{
    if (score > high_score) high_score = score;
    state = SUB_STATE_GAME_OVER;
    sub_led_level(100);
}

static uint8_t rect_hit(Rect a, Rect b)
{
    return (a.x < b.x + b.w &&
            a.x + a.w > b.x &&
            a.y < b.y + b.h &&
            a.y + a.h > b.y);
}

static Rect item_rect(Item it, int size)
{
    Rect r;
    r.x = it.x - size / 2;
    r.y = it.y - size / 2;
    r.w = size;
    r.h = size;
    return r;
}

static Rect mine_rect(Mine m)
{
    Rect r;
    r.x = m.x - m.size;
    r.y = m.y - m.size;
    r.w = m.size * 2;
    r.h = m.size * 2;
    return r;
}

static void spawn_diver(void)
{
    diver.active = 1;
    diver.x = 20 + rnd(SCREEN_W - 80);
    diver.y = PLAY_TOP + 45 + rnd(SCREEN_H - PLAY_TOP - 95);

    /* Avoid spawning exactly inside the base area */
    if (diver.x > base.x - 20 && diver.y > base.y - 20) {
        diver.x = 30;
        diver.y = PLAY_TOP + 60;
    }
}

static void spawn_oxygen_tank(void)
{
    oxygen_tank.active = 1;
    oxygen_tank.x = 20 + rnd(SCREEN_W - 40);
    oxygen_tank.y = PLAY_TOP + 50 + rnd(SCREEN_H - PLAY_TOP - 80);
}

static void reset_player(void)
{
    player.x = 18;
    player.y = PLAY_TOP + 120;
    player.w = 22;
    player.h = 12;
}

static void init_mines(void)
{
    for (int i = 0; i < 4; i++) {
        mines[i].size = 7 + (i % 2);
        mines[i].x = 70 + rnd(SCREEN_W - 100);
        mines[i].y = PLAY_TOP + 35 + rnd(SCREEN_H - PLAY_TOP - 75);

        int spd = 1 + (i % 2);
        mines[i].vx = (i % 2 == 0) ? spd : -spd;
        mines[i].vy = (i % 3 == 0) ? 1 : 0;
    }
}

static void reset_game(void)
{
    state = SUB_STATE_TITLE;

    base.w = 58;
    base.h = 42;
    base.x = SCREEN_W - base.w - 8;
    base.y = SCREEN_H - base.h - 8;

    reset_player();
    init_mines();
    spawn_diver();
    oxygen_tank.active = 0;

    carrying_diver = 0;
    lives = 3;
    score = 0;
    rescued = 0;
    oxygen = 100;
    sonar_timer = 0;
    sonar_cooldown = 0;
    invuln_timer = 0;
    difficulty = 0;
    frame_count = 0;
    last_update_ms = HAL_GetTick();
    banner_timer = 0;
    banner_text[0] = '\0';
    banner_colour = C_WHITE;

    sub_led_level(30);
}

static void damage_player(void)
{
    if (invuln_timer > 0 || sonar_timer > 0) return;

    if (lives > 0) lives--;
    oxygen -= 12;
    if (oxygen < 0) oxygen = 0;
    carrying_diver = 0;
    invuln_timer = 35;
    reset_player();
    set_banner("MINE HIT", C_RED);
    sub_beep(260, 90);

    if (lives == 0 || oxygen == 0) {
        enter_game_over();
        sub_beep(180, 300);
    }
}

static void update_player(void)
{
    int speed = carrying_diver ? 2 : 3;

    if (SUB_INPUT_UP())    player.y -= speed;
    if (SUB_INPUT_DOWN())  player.y += speed;
    if (SUB_INPUT_LEFT())  player.x -= speed;
    if (SUB_INPUT_RIGHT()) player.x += speed;

    player.x = clamp_i(player.x, 2, SCREEN_W - player.w - 2);
    player.y = clamp_i(player.y, PLAY_TOP + 4, SCREEN_H - player.h - 4);
}

static void update_mines(void)
{
    for (int i = 0; i < 4; i++) {
        int speed_bonus = difficulty / 3;
        int vx = mines[i].vx;
        int vy = mines[i].vy;

        if (vx > 0) vx += speed_bonus;
        if (vx < 0) vx -= speed_bonus;
        if (vy > 0) vy += speed_bonus / 2;
        if (vy < 0) vy -= speed_bonus / 2;

        mines[i].x += vx;
        mines[i].y += vy;

        if (mines[i].x < 15 || mines[i].x > SCREEN_W - 15) {
            mines[i].vx = -mines[i].vx;
            mines[i].x = clamp_i(mines[i].x, 15, SCREEN_W - 15);
        }
        if (mines[i].y < PLAY_TOP + 20 || mines[i].y > SCREEN_H - 20) {
            mines[i].vy = -mines[i].vy;
            mines[i].y = clamp_i(mines[i].y, PLAY_TOP + 20, SCREEN_H - 20);
        }
    }
}

static void check_collisions(void)
{
    if (diver.active && !carrying_diver && rect_hit(player, item_rect(diver, 12))) {
        diver.active = 0;
        carrying_diver = 1;
        score += 5;
        set_banner("DIVER FOUND", C_GREEN);
        sub_beep(1200, 60);
    }

    if (carrying_diver && rect_hit(player, base)) {
        rescued++;
        score += 20 + oxygen / 10;
        carrying_diver = 0;
        oxygen += 12;
        if (oxygen > 100) oxygen = 100;
        difficulty = rescued;
        set_banner("RESCUE OK", C_GREEN);
        spawn_diver();
        sub_beep(1700, 90);
    }

    if (oxygen_tank.active && rect_hit(player, item_rect(oxygen_tank, 12))) {
        oxygen_tank.active = 0;
        oxygen += 30;
        if (oxygen > 100) oxygen = 100;
        score += 3;
        set_banner("OXYGEN UP", C_CYAN);
        sub_beep(1450, 50);
    }

    for (int i = 0; i < 4; i++) {
        if (rect_hit(player, mine_rect(mines[i]))) {
            damage_player();
            break;
        }
    }
}

static void update_oxygen_and_power(void)
{
    /* Called roughly 20 times per second. 20 frames is about 1 second. */
    if (frame_count % 20 == 0 && state == SUB_STATE_PLAYING) {
        oxygen -= carrying_diver ? 2 : 1;
        if (oxygen < 0) oxygen = 0;

        if (oxygen == 0) {
            set_banner("NO OXYGEN", C_CYAN);
            enter_game_over();
            sub_beep(180, 300);
        }
    }

    if (oxygen < 25) {
        sub_led_level((frame_count % 10 < 5) ? 100 : 0);

        /* Small periodic warning. */
        if (frame_count % 40 == 0 && state == SUB_STATE_PLAYING) {
            sub_beep(620, 35);
        }
    } else if (carrying_diver) {
        sub_led_level(70);
    } else {
        sub_led_level(25);
    }
}

static void update_sonar(void)
{
    if (sonar_timer > 0) sonar_timer--;
    if (sonar_cooldown > 0) sonar_cooldown--;
    if (invuln_timer > 0) invuln_timer--;

    if (SUB_BTN_SONAR && sonar_cooldown == 0 && oxygen > 12 && state == SUB_STATE_PLAYING) {
        sonar_timer = 24;
        sonar_cooldown = 90;
        oxygen -= 8;
        set_banner("SONAR ON", C_CYAN);
        sub_beep(900, 40);
    }
}

static void step_game(void)
{
    frame_count++;
    if (banner_timer > 0) banner_timer--;

    if (state == SUB_STATE_TITLE) {
        /* BTN3 returns to the shared menu.  A short ignore time prevents the
           menu-select button press from immediately quitting the game. */
        if (SUB_BTN_MENU && (HAL_GetTick() - run_start_ms > 450u)) {
            exit_game = 1;
            return;
        }

        if (SUB_BTN_SONAR || SUB_INPUT_UP() || SUB_INPUT_DOWN() || SUB_INPUT_LEFT() || SUB_INPUT_RIGHT()) {
            state = SUB_STATE_PLAYING;
            sub_beep(1000, 60);
        }
        return;
    }

    if (state == SUB_STATE_GAME_OVER) {
        if (SUB_BTN_MENU && (HAL_GetTick() - run_start_ms > 450u)) {
            exit_game = 1;
            return;
        }
        if (SUB_BTN_RESTART) {
            reset_game();
            state = SUB_STATE_PLAYING;
        }
        return;
    }

    if (SUB_BTN_MENU && (HAL_GetTick() - run_start_ms > 450u)) {
        exit_game = 1;
        return;
    }

    update_player();
    update_mines();
    update_sonar();
    check_collisions();
    update_oxygen_and_power();

    /* Make the game fairer: if oxygen is getting low, a tank appears sooner. */
    if (!oxygen_tank.active && ((frame_count % 310 == 0) || (oxygen < 42 && frame_count % 90 == 0))) {
        spawn_oxygen_tank();
    }
}

/* ============================================================
 *  6. Drawing game objects
 * ============================================================ */

static void draw_water_background(void)
{
    SUB_LCD_CLEAR(C_WATER_DARK);

    for (int y = PLAY_TOP; y < SCREEN_H; y += 18) {
        uint16_t line_col = (y / 18) % 2 ? C_WATER : C_WATER_LINE;
        for (int x = 0; x < SCREEN_W; x += 9) {
            px(x, y, line_col);
            px(x + 1, y, line_col);
        }
    }

    /* Simple bubbles */
    for (int i = 0; i < 8; i++) {
        int bx = (int)((i * 47 + frame_count * (i + 1)) % SCREEN_W);
        int by = PLAY_TOP + (int)((i * 31 + frame_count * 2) % (SCREEN_H - PLAY_TOP));
        draw_circle(bx, by, 2 + (i % 2), C_BLUE);
    }

    /* Sea floor decoration. This is only cosmetic but makes the game screen
       look less empty in the video demonstration. */
    fill_rect(0, SCREEN_H - 5, SCREEN_W, 5, C_SAND);
    for (int x = 8; x < SCREEN_W; x += 34) {
        fill_rect(x, SCREEN_H - 14, 2, 9, C_SEAWEED);
        px(x - 2, SCREEN_H - 11, C_SEAWEED);
        px(x + 2, SCREEN_H - 9, C_SEAWEED);
    }
    for (int x = 20; x < SCREEN_W; x += 58) {
        fill_circle(x, SCREEN_H - 7, 4, C_DARK_GREY);
    }
}

static void draw_play_border(void)
{
    /* Clear visible border for the playable area.  This also makes it obvious
       that the submarine cannot go below the screen. */
    draw_rect(0, PLAY_TOP, SCREEN_W, SCREEN_H - PLAY_TOP, C_CYAN);
    fill_rect(1, SCREEN_H - 4, SCREEN_W - 2, 3, C_DARK_GREY);
}

static void draw_hud(void)
{
    fill_rect(0, 0, SCREEN_W, HUD_H, C_BLACK);
    draw_text(4, 4, "OXY", C_CYAN, 1);
    draw_rect(28, 4, 72, 9, C_WHITE);
    fill_rect(30, 6, oxygen * 68 / 100, 5, oxygen < 25 ? C_RED : C_CYAN);

    draw_text(4, 19, "L", C_GREEN, 1);
    draw_number(14, 19, lives, C_GREEN, 1);

    draw_text(45, 19, "S", C_YELLOW, 1);
    draw_number(55, 19, score, C_YELLOW, 1);

    draw_text(118, 4, "SAVE", C_WHITE, 1);
    draw_number(150, 4, rescued, C_WHITE, 1);

    if (carrying_diver) {
        draw_text(118, 19, "GO BASE", C_GREEN, 1);
    } else if (sonar_timer > 0) {
        draw_text(118, 19, "SONAR", C_CYAN, 1);
    } else if (oxygen < 25) {
        draw_text(118, 19, "LOW OXY", C_RED, 1);
    } else {
        draw_text(118, 19, "FIND", C_WHITE, 1);
    }

    if (sonar_cooldown > 0 && sonar_timer == 0) {
        draw_text(197, 4, "CD", C_GREY, 1);
        draw_number(215, 4, sonar_cooldown / 20 + 1, C_GREY, 1);
    }
}

static void draw_base(void)
{
    uint16_t edge_col = carrying_diver ? C_GREEN : C_CYAN;

    /* A brighter rescue base. It is placed at the bottom-right corner and
       has a label inside it so it is easier to find on the small LCD. */
    fill_rect(base.x, base.y, base.w, base.h, C_DARK_GREY);
    draw_rect(base.x, base.y, base.w, base.h, edge_col);
    draw_rect(base.x + 2, base.y + 2, base.w - 4, base.h - 4, C_GREY);

    fill_rect(base.x + 6, base.y + 8, base.w - 12, 9, C_CYAN);
    draw_text(base.x + 12, base.y + 9, "BASE", C_BLACK, 1);

    fill_rect(base.x + 20, base.y + 24, 18, 15, C_BLACK);
    fill_rect(base.x + 23, base.y + 27, 12, 12, carrying_diver ? C_GREEN : C_BLUE);

    if (carrying_diver) {
        draw_circle(base.x + base.w / 2, base.y + base.h / 2, 28 + (frame_count % 8), C_GREEN);
    }
}

static void draw_submarine(void)
{
    uint16_t body_col = (invuln_timer > 0 && (frame_count % 6 < 3)) ? C_WHITE : C_YELLOW;
    if (sonar_timer > 0) body_col = C_CYAN;

    int x = player.x;
    int y = player.y;

    fill_rect(x + 4, y + 2, player.w - 8, player.h - 4, body_col);
    fill_circle(x + 5, y + player.h / 2, 5, body_col);
    fill_circle(x + player.w - 5, y + player.h / 2, 5, body_col);

    fill_rect(x + 8, y - 4, 4, 6, body_col);
    fill_rect(x + 10, y - 7, 8, 3, body_col);

    fill_circle(x + 14, y + 6, 2, C_BLUE);
    fill_rect(x - 4, y + 3, 5, 2, C_ORANGE);
    fill_rect(x - 4, y + 7, 5, 2, C_ORANGE);

    if (carrying_diver) {
        fill_rect(x + 4, y + player.h + 1, 14, 3, C_GREEN);
    }

    if (sonar_timer > 0) {
        int r = 8 + (24 - sonar_timer) * 3;
        draw_circle(x + player.w / 2, y + player.h / 2, r, C_CYAN);
    }
}

static void draw_diver(void)
{
    if (!diver.active) return;

    fill_circle(diver.x, diver.y - 3, 3, C_GREEN);
    fill_rect(diver.x - 2, diver.y, 4, 7, C_GREEN);
    px(diver.x - 5, diver.y + 2, C_GREEN);
    px(diver.x + 5, diver.y + 2, C_GREEN);
    px(diver.x - 3, diver.y + 8, C_GREEN);
    px(diver.x + 3, diver.y + 8, C_GREEN);
}

static void draw_oxygen_tank(void)
{
    if (!oxygen_tank.active) return;

    fill_rect(oxygen_tank.x - 4, oxygen_tank.y - 7, 8, 14, C_CYAN);
    draw_rect(oxygen_tank.x - 5, oxygen_tank.y - 8, 10, 16, C_WHITE);
    fill_rect(oxygen_tank.x - 2, oxygen_tank.y - 11, 4, 3, C_GREY);
}

static void draw_mines(void)
{
    for (int i = 0; i < 4; i++) {
        uint16_t col = (sonar_timer > 0) ? C_PURPLE : C_RED;
        int x = mines[i].x;
        int y = mines[i].y;
        int s = mines[i].size;

        fill_circle(x, y, s, col);
        px(x - s - 2, y, col); px(x + s + 2, y, col);
        px(x, y - s - 2, col); px(x, y + s + 2, col);
        px(x - s, y - s, col); px(x + s, y - s, col);
        px(x - s, y + s, col); px(x + s, y + s, col);
        fill_circle(x - 2, y - 2, 2, C_ORANGE);
    }
}

static void draw_objective_hint(void)
{
    int sx = player.x + player.w / 2;
    int sy = player.y + player.h / 2;
    int tx, ty;
    uint16_t col;

    if (carrying_diver) {
        tx = base.x + base.w / 2;
        ty = base.y + base.h / 2;
        col = C_GREEN;
        draw_text(base.x + 4, base.y - 10, "DROP", C_GREEN, 1);
    } else {
        tx = diver.x;
        ty = diver.y;
        col = C_CYAN;
        draw_text(clamp_i(tx - 16, 2, SCREEN_W - 42), clamp_i(ty - 18, PLAY_TOP + 2, SCREEN_H - 12), "DIVER", C_CYAN, 1);
    }

    draw_dotted_line(sx, sy, tx, ty, col);
    draw_circle(tx, ty, 14 + (frame_count % 6), col);
}

static void draw_banner(void)
{
    if (banner_timer == 0 || banner_text[0] == '\0') return;

    int len = (int)strlen(banner_text);
    int w = len * 6 + 12;
    int x = (SCREEN_W - w) / 2;
    int y = 42;

    fill_rect(x, y, w, 18, C_BLACK);
    draw_rect(x, y, w, 18, banner_colour);
    draw_text(x + 6, y + 5, banner_text, banner_colour, 1);
}

static void draw_title_screen(void)
{
    draw_water_background();
    draw_text(18, 62, "SUBMARINE", C_CYAN, 2);
    draw_text(39, 86, "RESCUE", C_YELLOW, 2);

    fill_rect(78, 128, 80, 34, C_DARK_GREY);
    draw_rect(78, 128, 80, 34, C_CYAN);
    draw_text(93, 138, "MISSION", C_WHITE, 1);
    draw_text(31, 164, "SAVE DIVERS", C_GREEN, 1);
    draw_text(31, 181, "RETURN TO BASE", C_CYAN, 1);
    draw_text(31, 198, "BT2 SONAR", C_PURPLE, 1);
    draw_text(19, 216, "MOVE STICK TO START", C_WHITE, 1);
    draw_text(34, 229, "BEST", C_GREY, 1);
    draw_number(68, 229, high_score, C_YELLOW, 1);
    draw_text(142, 229, "BTN3 BACK", C_GREY, 1);
}

static void draw_game_over(void)
{
    draw_water_background();
    draw_text(31, 82, "GAME OVER", C_RED, 2);
    draw_text(36, 130, "SCORE", C_WHITE, 1);
    draw_number(86, 130, score, C_YELLOW, 1);
    draw_text(36, 150, "DIVERS", C_WHITE, 1);
    draw_number(92, 150, rescued, C_GREEN, 1);
    draw_text(36, 170, "BEST", C_WHITE, 1);
    draw_number(80, 170, high_score, C_YELLOW, 1);

    if (oxygen == 0) {
        draw_text(35, 195, "OXYGEN EMPTY", C_CYAN, 1);
    } else {
        draw_text(45, 195, "SUB LOST", C_RED, 1);
    }

    draw_text(24, 216, "BT2 RESTART", C_WHITE, 1);
    draw_text(42, 230, "BT3 MENU", C_GREY, 1);
}

/* ============================================================
 *  7. Public game functions for the menu template
 * ============================================================ */

void Game3_Init(void)
{
    rng_state = HAL_GetTick() ^ 0x26452645u;
    exit_game = 0;
    run_start_ms = HAL_GetTick();
    buzzer_stop_time = 0;
    buzzer_off(&buzzer_cfg);
    reset_game();
}

void Game3_Update(void)
{
    uint32_t now = HAL_GetTick();

    /* Read joystick directly.  Button events are updated by Input_Read() in
       Game3_Run(), while joystick values come from the ADC driver. */
    Joystick_Read(&joystick_cfg, &joystick_data);
    sub_input = Joystick_GetInput(&joystick_data);

    /* Let short buzzer tones stop even while the game is between update steps. */
    sub_buzzer_update();

    /* Fixed-ish update. It keeps the game speed more stable if the LCD drawing
       takes slightly different time on different machines/builds. */
    if ((now - last_update_ms) < 50u) {
        return;
    }
    last_update_ms = now;

    step_game();
}

void Game3_Render(void)
{
    if (state == SUB_STATE_TITLE) {
        draw_title_screen();
        SUB_LCD_FLUSH();
        return;
    }

    if (state == SUB_STATE_GAME_OVER) {
        draw_game_over();
        SUB_LCD_FLUSH();
        return;
    }

    draw_water_background();
    draw_play_border();
    draw_hud();
    draw_base();
    draw_diver();
    draw_oxygen_tank();
    draw_mines();
    draw_objective_hint();
    draw_submarine();
    draw_banner();

    SUB_LCD_FLUSH();
}


MenuState Game3_Run(void)
{
    Game3_Init();

    while (!exit_game) {
        Input_Read();
        Game3_Update();
        Game3_Render();

        
        HAL_Delay(8);
    }

    sub_led_level(30);
    buzzer_off(&buzzer_cfg);
    SUB_LCD_CLEAR(C_BLACK);
    SUB_LCD_FLUSH();

    return MENU_STATE_HOME;
}
