#include "Game_1.h"
#include "LCD.h"
#include "Joystick.h"
#include "InputHandler.h"
#include "Menu.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PALETTE — Vintage 16pal (Arne)
#define C_BG          0
#define C_GREY        1
#define C_WHITE       2
#define C_RED         3
#define C_PINK        4
#define C_DARK        5
#define C_BROWN       6
#define C_ORANGE      7
#define C_YELLOW      8
#define C_TEAL        9
#define C_GREEN      10
#define C_LIME       11
#define C_NAVY       12
#define C_BLUE       13
#define C_CYAN       14
#define C_LIGHT      15
#define C_MAGENTA    C_PINK
#define C_PURPLE     C_NAVY
#define C_GOLD       C_LIME
#define C_VIOLET     C_PINK

// SFX
#define ENABLE_SFX 1
#if ENABLE_SFX
  #include "Buzzer.h"
  extern Buzzer_cfg_t buzzer_cfg;
#endif
static inline void sfx(uint32_t freq, uint32_t ms) {
#if ENABLE_SFX
    if (freq == 0 || ms == 0) { buzzer_off(&buzzer_cfg); return; }
    buzzer_tone(&buzzer_cfg, freq, 50);
    HAL_Delay(ms);
    buzzer_off(&buzzer_cfg);
#else
    (void)freq; (void)ms;
#endif
}

// LED
static inline void led_set(uint8_t on) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// Layout
#define CELL       16
#define COLS       15
#define ROWS       13
#define HEADER_H   32           // +8px for skill bar
#define PLAY_X     0
#define PLAY_Y     HEADER_H
#define SCREEN_W   240
#define SCREEN_H   240
#define MAX_LEN    (COLS * ROWS)

// Timing
#define TICK_MS_BASE   90
#define TICK_MS_MIN    55
#define JUICE_FRAMES   5

// Player
#define MAX_HP                4
#define PLAYER_FIRE_BASE      7
#define INVULN_AFTER_HIT      18

// Bullets
#define MAX_BULLETS    24
#define BULLET_PLAYER  0
#define BULLET_ENEMY   1
#define MAX_BULLET_COUNT 5

// Enemies
#define MAX_ENEMIES         6
#define ENEMY_MAX_LEN       4
#define ENEMY_TYPE_CHASER   0
#define ENEMY_TYPE_SHOOTER  1
#define ENEMY_TYPE_FAST     2
#define ENEMY_TYPE_TANK     3
#define ENEMY_FIRE_COOLDOWN 28

// Levels
#define MAX_LIVES       3
#define MAX_LEVEL       8
#define MAX_OBSTACLES   24

// Power-ups
#define PU_NONE     0
#define PU_HEAL     1
#define PU_RAPID    2
#define PU_SHIELD   3
#define PU_SPREAD   4
#define PU_AMMO     5
#define EFFECT_DURATION 80

// Combo
#define COMBO_WINDOW    25
#define COMBO_MAX       9

// Stars
#define STAR_COUNT      20

// Explosions
#define EXPLOSION_MAX    6
#define EXPLOSION_FRAMES 5

// SKILLS
#define SKILL_COUNT          2
#define SKILL_EMP            0
#define SKILL_OVERDRIVE      1
#define SKILL_EMP_MAX        80     // energy required to fire
#define SKILL_OD_MAX         50
#define SKILL_OD_DURATION    50     // ticks rapid+spread stays on
#define SKILL_FLASH_FRAMES   4      // white flash on EMP
#define SKILL_GAIN_KILL_EMP  12
#define SKILL_GAIN_KILL_OD   15
#define SKILL_GAIN_FOOD      5
#define SKILL_GAIN_PICKUP    20

// SFX freq
#define SFX_SHOOT_FREQ      900
#define SFX_SHOOT_MS         15
#define SFX_HIT_FREQ        400
#define SFX_HIT_MS           30
#define SFX_KILL_FREQ       180
#define SFX_KILL_MS          60
#define SFX_HURT_FREQ       120
#define SFX_HURT_MS         100
#define SFX_PICKUP_FREQ    1500
#define SFX_PICKUP_MS        70
#define SFX_DEATH_FREQ      150
#define SFX_DEATH_MS        300
#define SFX_VICTORY_FREQ   1760
#define SFX_VICTORY_MS      400
#define SFX_LEVEL_FREQ     1500
#define SFX_LEVEL_MS        140

// Externs
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t     joystick_data;
extern ST7789V2_cfg_t cfg0;

// Types
typedef struct { int8_t x, y; } Pt;

typedef struct {
    Pt        body[ENEMY_MAX_LEN];
    uint8_t   len;
    Direction dir;
    uint8_t   alive;
    uint8_t   hp;
    uint8_t   type;
    uint8_t   shoot_cd;
    uint8_t   move_cd;
} Enemy_t;

typedef struct {
    int8_t    x, y;
    Direction dir;
    uint8_t   active;
    uint8_t   owner;
} Bullet_t;

typedef struct {
    int16_t  x, y;
    uint8_t  speed;
    uint8_t  twinkle;
} Star_t;

typedef struct {
    int8_t   x, y;
    uint8_t  frame;
    uint8_t  active;
} Explosion_t;

// State
static Pt        snake[MAX_LEN];
static uint16_t  snake_len;
static Pt        food;
static Direction dir, next_dir;
static uint16_t  score;
static uint16_t  hi_score = 0;
static uint8_t   game_over;
static uint8_t   victory;

static uint8_t   juice_counter;
static uint32_t  anim_tick;
static int8_t    shake_x, shake_y;

static Enemy_t   enemies[MAX_ENEMIES];
static uint16_t  enemy_spawn_timer;
static uint8_t   enemy_move_phase;

static Pt        bonus;
static uint8_t   bonus_active;
static uint8_t   bonus_ttl;
static uint8_t   bonus_type;

static uint8_t   intro_played = 0;

static uint8_t   lives;
static uint8_t   led_flash_counter;
static uint8_t   led_state;

static uint8_t   level;
static uint16_t  packets_needed;
static uint16_t  packets_collected;
static uint8_t   level_complete;

static Pt        obstacles[MAX_OBSTACLES];
static uint8_t   obstacle_count;

static Pt        portal_a, portal_b;
static uint8_t   portals_active;
static uint8_t   portal_cooldown;

static uint8_t   player_hp;
static uint8_t   player_invuln;
static uint8_t   fire_cooldown;
static uint8_t   shield_charges;
static uint16_t  speed_timer;
static uint16_t  ghost_timer;
static uint16_t  mult_timer;

static uint8_t   combo_count;
static uint16_t  combo_timer;

static Bullet_t  bullets[MAX_BULLETS];

static uint8_t     bullet_count = 1;
static Star_t      stars[STAR_COUNT];
static Explosion_t explosions[EXPLOSION_MAX];

// SKILL STATE
static uint16_t skill_energy[SKILL_COUNT];
static const uint16_t skill_max[SKILL_COUNT] = {
    SKILL_EMP_MAX, SKILL_OD_MAX
};
static uint8_t  skill_flash;          // EMP white-flash counter
static uint8_t  skill_ready_seen[SKILL_COUNT];  // avoid re-chime spam
static uint8_t  prev_btn1, prev_btn2; // edge detection

// fwd decls
static void draw_frame(void);

// helpers
static inline int8_t abs8(int8_t v){ return v<0?-v:v; }

static inline void skill_charge(uint16_t emp_gain, uint16_t od_gain) {
    skill_energy[SKILL_EMP] += emp_gain;
    if (skill_energy[SKILL_EMP] > SKILL_EMP_MAX)
        skill_energy[SKILL_EMP] = SKILL_EMP_MAX;
    skill_energy[SKILL_OVERDRIVE] += od_gain;
    if (skill_energy[SKILL_OVERDRIVE] > SKILL_OD_MAX)
        skill_energy[SKILL_OVERDRIVE] = SKILL_OD_MAX;
}

static uint8_t in_bounds(int8_t x, int8_t y) {
    return x >= 0 && x < COLS && y >= 0 && y < ROWS;
}
static uint8_t cell_in_obstacle(int8_t x, int8_t y) {
    for (uint8_t i = 0; i < obstacle_count; i++)
        if (obstacles[i].x == x && obstacles[i].y == y) return 1;
    return 0;
}
static int find_enemy_at(int8_t x, int8_t y) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive &&
            enemies[i].body[0].x == x && enemies[i].body[0].y == y) return i;
    return -1;
}
static uint8_t cell_in_player(int8_t x, int8_t y) {
    return snake[0].x == x && snake[0].y == y;
}
static uint8_t cell_blocked(int8_t x, int8_t y) {
    if (!in_bounds(x, y)) return 1;
    if (cell_in_obstacle(x, y)) return 1;
    return 0;
}
static uint8_t cell_blocked_for_enemy(int8_t x, int8_t y, Enemy_t *self) {
    if (cell_blocked(x, y)) return 1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy_t *o = &enemies[i];
        if (!o->alive || o == self) continue;
        if (o->body[0].x == x && o->body[0].y == y) return 1;
    }
    return 0;
}

// starfield
static void init_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x       = (int16_t)(rand() % SCREEN_W);
        stars[i].y       = (int16_t)(PLAY_Y + rand() % (SCREEN_H - PLAY_Y));
        stars[i].speed   = 1 + (uint8_t)(rand() % 3);
        stars[i].twinkle = (uint8_t)(rand() % 8);
    }
}
static void update_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].y += stars[i].speed;
        stars[i].twinkle++;
        if (stars[i].y >= SCREEN_H) {
            stars[i].y     = PLAY_Y;
            stars[i].x     = (int16_t)(rand() % SCREEN_W);
            stars[i].speed = 1 + (uint8_t)(rand() % 3);
        }
    }
}
static void draw_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        uint8_t t   = stars[i].twinkle & 0x07;
        uint8_t spd = stars[i].speed;
        uint8_t col;
        if (spd >= 3)       col = (t < 5) ? C_YELLOW : C_WHITE;
        else if (spd == 2)  col = (t < 6) ? C_LIGHT  : C_CYAN;
        else                col = (t < 5) ? C_BLUE   : C_NAVY;
        if (col == C_BG) continue;
        if (stars[i].x < 0 || stars[i].x >= SCREEN_W - spd) continue;
        if (stars[i].y < PLAY_Y || stars[i].y >= SCREEN_H - spd) continue;
        LCD_Draw_Rect((uint16_t)stars[i].x, (uint16_t)stars[i].y, spd, spd, col, 1);
        if (spd >= 3 && stars[i].y >= PLAY_Y + 2)
            LCD_Draw_Rect((uint16_t)stars[i].x, (uint16_t)(stars[i].y - 2),
                          1, 2, C_ORANGE, 1);
    }
}

// explosions
static void spawn_explosion(int8_t x, int8_t y) {
    for (int i = 0; i < EXPLOSION_MAX; i++) {
        if (!explosions[i].active) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].frame  = 0;
            explosions[i].active = 1;
            return;
        }
    }
}
static void update_explosions(void) {
    for (int i = 0; i < EXPLOSION_MAX; i++) {
        if (!explosions[i].active) continue;
        explosions[i].frame++;
        if (explosions[i].frame >= EXPLOSION_FRAMES) explosions[i].active = 0;
    }
}
static void draw_explosions(void) {
    for (int i = 0; i < EXPLOSION_MAX; i++) {
        if (!explosions[i].active) continue;
        int16_t px = PLAY_X + explosions[i].x * CELL + shake_x;
        int16_t py = PLAY_Y + explosions[i].y * CELL + shake_y;
        switch (explosions[i].frame) {
            case 0:
                LCD_Draw_Rect(px + 6, py + 6, 4, 4, C_WHITE,  1);
                LCD_Draw_Rect(px + 7, py + 7, 2, 2, C_YELLOW, 1);
                break;
            case 1:
                LCD_Draw_Rect(px + 4, py + 4, 8, 8, C_YELLOW, 1);
                LCD_Draw_Rect(px + 6, py + 6, 4, 4, C_WHITE,  1);
                break;
            case 2:
                LCD_Draw_Rect(px + 2, py + 2, 12, 12, C_ORANGE, 0);
                LCD_Draw_Rect(px + 4, py + 4, 8, 8,  C_YELLOW, 1);
                LCD_Draw_Rect(px + 6, py + 6, 4, 4,  C_WHITE,  1);
                LCD_Draw_Rect(px + 1,        py + CELL/2,    2, 2, C_RED, 1);
                LCD_Draw_Rect(px + CELL - 3, py + CELL/2,    2, 2, C_RED, 1);
                LCD_Draw_Rect(px + CELL/2,   py + 1,         2, 2, C_RED, 1);
                LCD_Draw_Rect(px + CELL/2,   py + CELL - 3,  2, 2, C_RED, 1);
                break;
            case 3:
                LCD_Draw_Rect(px,    py,    CELL, CELL, C_RED,    0);
                LCD_Draw_Rect(px + 5,py + 5,6, 6,       C_ORANGE, 1);
                LCD_Draw_Rect(px + 7,py + 7,2, 2,       C_YELLOW, 1);
                LCD_Draw_Rect(px,                 py + CELL/2 - 1, 2, 2, C_BROWN, 1);
                LCD_Draw_Rect(px + CELL - 2,      py + CELL/2 - 1, 2, 2, C_BROWN, 1);
                LCD_Draw_Rect(px + CELL/2 - 1,    py,              2, 2, C_BROWN, 1);
                LCD_Draw_Rect(px + CELL/2 - 1,    py + CELL - 2,   2, 2, C_BROWN, 1);
                break;
            case 4:
                LCD_Draw_Rect(px + 1,        py + 1,        2, 2, C_DARK, 1);
                LCD_Draw_Rect(px + CELL - 3, py + 1,        2, 2, C_GREY, 1);
                LCD_Draw_Rect(px + 1,        py + CELL - 3, 2, 2, C_GREY, 1);
                LCD_Draw_Rect(px + CELL - 3, py + CELL - 3, 2, 2, C_DARK, 1);
                LCD_Draw_Rect(px + CELL/2-1, py + CELL/2-1, 2, 2, C_RED,  1);
                break;
            default: break;
        }
    }
}

// food place
static void place_food(void) {
    static Pt free_cells[MAX_LEN];
    uint16_t free_count = 0;
    for (int8_t y = 0; y < ROWS; y++) {
        for (int8_t x = 0; x < COLS; x++) {
            if (cell_blocked(x, y))                       continue;
            if (cell_in_player(x, y))                     continue;
            if (find_enemy_at(x, y) >= 0)                 continue;
            if (bonus_active && bonus.x == x && bonus.y == y) continue;
            free_cells[free_count].x = x;
            free_cells[free_count].y = y;
            free_count++;
        }
    }
    if (free_count == 0) return;
    uint32_t r = ((uint32_t)rand() << 8) ^ (uint32_t)rand();
    food = free_cells[r % free_count];
}

// bullets
static void fire_bullet(int8_t x, int8_t y, Direction d, uint8_t owner) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].x = x; bullets[i].y = y;
            bullets[i].dir = d; bullets[i].owner = owner;
            bullets[i].active = 1;
            return;
        }
    }
}

static void player_shoot(void) {
    if (fire_cooldown > 0) return;
    if (dir == CENTRE) return;
    int8_t bx = snake[0].x, by = snake[0].y;

    if (mult_timer > 0) {
        Direction left = N, right = S;
        switch (dir) {
            case N: left = W; right = E; break;
            case S: left = E; right = W; break;
            case E: left = N; right = S; break;
            case W: left = S; right = N; break;
            default: break;
        }
        fire_bullet(bx, by, dir, BULLET_PLAYER);
        int8_t lx = bx, ly = by, rx = bx, ry = by;
        switch (left)  { case N: ly--; break; case S: ly++; break;
                         case E: lx++; break; case W: lx--; break; default: break; }
        switch (right) { case N: ry--; break; case S: ry++; break;
                         case E: rx++; break; case W: rx--; break; default: break; }
        fire_bullet(lx, ly, dir, BULLET_PLAYER);
        fire_bullet(rx, ry, dir, BULLET_PLAYER);
    } else {
        fire_bullet(bx, by, dir, BULLET_PLAYER);
        for (uint8_t s = 1; s < bullet_count; s++) {
            int8_t off = (int8_t)((s + 1) >> 1);
            if (s & 1) off = (int8_t)(-off);
            int8_t ox = bx, oy = by;
            switch (dir) {
                case N: case S: ox = (int8_t)(bx + off); break;
                case E: case W: oy = (int8_t)(by + off); break;
                default: break;
            }
            if (in_bounds(ox, oy) && !cell_in_obstacle(ox, oy))
                fire_bullet(ox, oy, dir, BULLET_PLAYER);
        }
    }
    fire_cooldown = (speed_timer > 0) ? 3 : PLAYER_FIRE_BASE;
    sfx(SFX_SHOOT_FREQ, SFX_SHOOT_MS);
}

// player hurt
static void hurt_player(void) {
    if (player_invuln > 0) return;
    if (ghost_timer  > 0) return;
    if (shield_charges > 0) {
        shield_charges--;
        player_invuln = INVULN_AFTER_HIT;
        sfx(500, 80);
        juice_counter = JUICE_FRAMES;
        return;
    }
    player_hp--;
    player_invuln     = INVULN_AFTER_HIT;
    juice_counter     = JUICE_FRAMES;
    led_flash_counter = 8;
    sfx(SFX_HURT_FREQ, SFX_HURT_MS);
    if (player_hp == 0) game_over = 1;
}

// enemy kill
static void kill_enemy(int idx) {
    Enemy_t *e = &enemies[idx];
    Pt drop = e->body[0];

    spawn_explosion(drop.x, drop.y);
    juice_counter = JUICE_FRAMES;

    if (!cell_in_player(drop.x, drop.y)    &&
        !cell_in_obstacle(drop.x, drop.y)  &&
        !(food.x == drop.x && food.y == drop.y) &&
        (rand() % 100) < 38 && !bonus_active) {
        bonus = drop;
        bonus_active = 1;
        bonus_ttl    = 80;
        uint8_t r = rand() % 100;
        if      (r < 25) bonus_type = PU_HEAL;
        else if (r < 45) bonus_type = PU_RAPID;
        else if (r < 65) bonus_type = PU_SHIELD;
        else if (r < 80) bonus_type = PU_SPREAD;
        else             bonus_type = PU_AMMO;
    }
    e->alive = 0;

    if (combo_timer > 0 && combo_count < COMBO_MAX) combo_count++;
    if (combo_count == 0) combo_count = 1;
    combo_timer = COMBO_WINDOW;

    uint16_t pts = 10 * (e->type == ENEMY_TYPE_TANK ? 3 : 1)
                 + (combo_count - 1) * 5;
    score += pts;
    packets_collected++;
    sfx(SFX_KILL_FREQ + (uint32_t)(combo_count - 1) * 60, SFX_KILL_MS);

    // SKILL: charge from kill
    skill_charge(SKILL_GAIN_KILL_EMP, SKILL_GAIN_KILL_OD);
}

// bullet update
static void update_bullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet_t *b = &bullets[i];
        if (!b->active) continue;
        switch (b->dir) {
            case N: b->y--; break;
            case S: b->y++; break;
            case E: b->x++; break;
            case W: b->x--; break;
            default: b->active = 0; continue;
        }
        if (!in_bounds(b->x, b->y))      { b->active = 0; continue; }
        if (cell_in_obstacle(b->x, b->y)){ b->active = 0; continue; }

        if (b->owner == BULLET_PLAYER) {
            int eidx = find_enemy_at(b->x, b->y);
            if (eidx >= 0) {
                Enemy_t *e = &enemies[eidx];
                e->hp--;
                b->active = 0;
                if (e->hp == 0) kill_enemy(eidx);
                else            sfx(SFX_HIT_FREQ, SFX_HIT_MS);
            }
        } else {
            if (cell_in_player(b->x, b->y)) {
                b->active = 0;
                hurt_player();
            }
        }
    }
}

// enemy AI
static void enemy_step(int idx) {
    Enemy_t *e = &enemies[idx];
    if (e->move_cd > 0) { e->move_cd--; return; }
    switch (e->type) {
        case ENEMY_TYPE_FAST:    e->move_cd = 1; break;
        case ENEMY_TYPE_TANK:    e->move_cd = 4; break;
        case ENEMY_TYPE_SHOOTER: e->move_cd = 3; break;
        default:                 e->move_cd = 2; break;
    }

    Pt cur = e->body[0];
    int8_t dx = (int8_t)(snake[0].x - cur.x);
    int8_t dy = (int8_t)(snake[0].y - cur.y);

    Direction prio[4];
    Direction primary, secondary;
    if (abs8(dx) > abs8(dy)) {
        primary   = (dx > 0) ? E : (dx < 0 ? W : E);
        secondary = (dy > 0) ? S : (dy < 0 ? N : S);
    } else {
        primary   = (dy > 0) ? S : (dy < 0 ? N : S);
        secondary = (dx > 0) ? E : (dx < 0 ? W : E);
    }
    prio[0] = primary;
    prio[1] = secondary;
    Direction all[4] = {N, S, E, W};
    int p = 2;
    for (int k = 0; k < 4 && p < 4; k++)
        if (all[k] != prio[0] && all[k] != prio[1]) prio[p++] = all[k];

    Direction chosen = CENTRE;
    Pt next = cur;
    for (int k = 0; k < 4; k++) {
        Pt n = cur;
        switch (prio[k]) {
            case N: n.y--; break; case S: n.y++; break;
            case E: n.x++; break; case W: n.x--; break;
            default: continue;
        }
        if (cell_in_player(n.x, n.y)) {
            hurt_player();
            return;
        }
        if (!cell_blocked_for_enemy(n.x, n.y, e)) {
            chosen = prio[k]; next = n; break;
        }
    }
    if (chosen == CENTRE) return;
    e->dir = chosen;
    e->body[0] = next;
}

static void enemy_try_shoot(int idx) {
    Enemy_t *e = &enemies[idx];
    if (e->type != ENEMY_TYPE_SHOOTER) return;
    if (e->shoot_cd > 0) { e->shoot_cd--; return; }

    int8_t dx = (int8_t)(snake[0].x - e->body[0].x);
    int8_t dy = (int8_t)(snake[0].y - e->body[0].y);
    Direction d = CENTRE;
    if      (dx == 0 && dy != 0) d = (dy > 0) ? S : N;
    else if (dy == 0 && dx != 0) d = (dx > 0) ? E : W;
    if (d == CENTRE) return;

    fire_bullet(e->body[0].x, e->body[0].y, d, BULLET_ENEMY);
    e->shoot_cd = ENEMY_FIRE_COOLDOWN;
    sfx(600, 12);
}

// spawn
static void try_spawn_enemy(void) {
    uint8_t alive_count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive) alive_count++;

    uint8_t max_concurrent = 2 + level / 2;
    if (max_concurrent > MAX_ENEMIES) max_concurrent = MAX_ENEMIES;

    uint16_t remaining =
        (packets_needed > packets_collected + alive_count)
            ? (packets_needed - packets_collected - alive_count) : 0;
    if (remaining == 0) return;
    if (alive_count >= max_concurrent) return;

    if (enemy_spawn_timer > 0) { enemy_spawn_timer--; return; }
    int16_t rearm = 40 - level * 3;
    if (rearm < 12) rearm = 12;
    enemy_spawn_timer = (uint16_t)rearm;

    int slot = -1;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (!enemies[i].alive) { slot = i; break; }
    if (slot < 0) return;

    int edge = rand() % 4;
    int8_t hx = 0, hy = 0;
    switch (edge) {
        case 0: hx = 0;        hy = rand() % ROWS; break;
        case 1: hx = COLS - 1; hy = rand() % ROWS; break;
        case 2: hx = rand() % COLS; hy = 0;        break;
        default:hx = rand() % COLS; hy = ROWS - 1; break;
    }
    if (cell_blocked(hx, hy) || cell_in_player(hx, hy)) return;
    if (find_enemy_at(hx, hy) >= 0) return;

    Enemy_t *e = &enemies[slot];
    uint8_t roll = rand() % 100;
    if      (level <= 1) e->type = ENEMY_TYPE_CHASER;
    else if (level <= 3) e->type = (roll < 70) ? ENEMY_TYPE_CHASER : ENEMY_TYPE_SHOOTER;
    else if (level <= 5) {
        if      (roll < 50) e->type = ENEMY_TYPE_CHASER;
        else if (roll < 80) e->type = ENEMY_TYPE_SHOOTER;
        else                e->type = ENEMY_TYPE_FAST;
    } else {
        if      (roll < 35) e->type = ENEMY_TYPE_CHASER;
        else if (roll < 60) e->type = ENEMY_TYPE_SHOOTER;
        else if (roll < 85) e->type = ENEMY_TYPE_FAST;
        else                e->type = ENEMY_TYPE_TANK;
    }

    e->alive = 1;
    e->len   = 1;
    e->body[0].x = hx; e->body[0].y = hy;
    e->dir   = E;
    e->hp    = (e->type == ENEMY_TYPE_TANK) ? 3 : 1;
    e->shoot_cd = 10 + rand() % 20;
    e->move_cd  = 0;
    sfx(450, 25);
}

// SKILL fire
static void fire_skill_emp(void) {
    // destroy every enemy
    uint8_t kills = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) {
            spawn_explosion(enemies[i].body[0].x, enemies[i].body[0].y);
            enemies[i].alive = 0;
            score += 15;
            packets_collected++;
            kills++;
        }
    }
    // clear enemy bullets
    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].active && bullets[i].owner == BULLET_ENEMY)
            bullets[i].active = 0;

    skill_flash      = SKILL_FLASH_FRAMES;
    juice_counter    = JUICE_FRAMES;
    led_flash_counter = 12;

    // dramatic 2-tone SFX
    sfx(180, 80);
    sfx(2000, 60);
    if (kills > 0) sfx(900, 50);
}

static void fire_skill_overdrive(void) {
    // boost both rapid + spread
    speed_timer = SKILL_OD_DURATION;
    mult_timer  = SKILL_OD_DURATION;
    juice_counter = JUICE_FRAMES;
    led_flash_counter = 6;
    // rising chord
    sfx(880, 40);
    sfx(1175, 40);
    sfx(1480, 60);
    sfx(1760, 60);
}

static void try_use_skill(uint8_t s) {
    if (s >= SKILL_COUNT) return;
    if (skill_energy[s] < skill_max[s]) {
        sfx(220, 25);   // "denied" buzz
        return;
    }
    skill_energy[s] = 0;
    skill_ready_seen[s] = 0;
    if      (s == SKILL_EMP)       fire_skill_emp();
    else if (s == SKILL_OVERDRIVE) fire_skill_overdrive();
}

// input
static void handle_input(void) {
    Joystick_Read(&joystick_cfg, &joystick_data);
    UserInput in = Joystick_GetInput(&joystick_data);
    Direction d = in.direction;
    Direction movement = CENTRE;

    switch (d) {
        case N: case S: case E: case W:
            movement = d; dir = d; next_dir = d; break;
        case NE: movement = (anim_tick & 1) ? N : E; dir = E; next_dir = N; break;
        case NW: movement = (anim_tick & 1) ? N : W; dir = W; next_dir = N; break;
        case SE: movement = (anim_tick & 1) ? S : E; dir = E; next_dir = S; break;
        case SW: movement = (anim_tick & 1) ? S : W; dir = W; next_dir = S; break;
        default: break;
    }

    if (movement == CENTRE) return;

    Pt next = snake[0];
    switch (movement) {
        case N: next.y--; break;
        case S: next.y++; break;
        case E: next.x++; break;
        case W: next.x--; break;
        default: return;
    }
    if (!in_bounds(next.x, next.y)) return;
    if (cell_in_obstacle(next.x, next.y)) return;

    int eidx = find_enemy_at(next.x, next.y);
    if (eidx >= 0) { hurt_player(); return; }

    snake[0] = next;

    if (snake[0].x == food.x && snake[0].y == food.y) {
        score += 5;
        if (combo_timer > 0 && combo_count < COMBO_MAX) combo_count++;
        if (combo_count == 0) combo_count = 1;
        combo_timer = COMBO_WINDOW;
        sfx(SFX_PICKUP_FREQ, 30);
        led_flash_counter = 8;
        skill_charge(SKILL_GAIN_FOOD, SKILL_GAIN_FOOD);
        place_food();
    }
    if (bonus_active && snake[0].x == bonus.x && snake[0].y == bonus.y) {
        bonus_active = 0;
        led_flash_counter = 12;
        skill_charge(SKILL_GAIN_PICKUP, SKILL_GAIN_PICKUP);
        switch (bonus_type) {
            case PU_HEAL:
                if (player_hp < MAX_HP) player_hp++;
                sfx(1200, 80); break;
            case PU_RAPID:
                speed_timer = EFFECT_DURATION;
                sfx(1500, 70); break;
            case PU_SHIELD:
                ghost_timer = EFFECT_DURATION;
                sfx(900, 70);  break;
            case PU_SPREAD:
                mult_timer  = EFFECT_DURATION;
                sfx(1700, 70); break;
            case PU_AMMO:
                if (bullet_count < MAX_BULLET_COUNT) bullet_count++;
                sfx(1100, 90); HAL_Delay(40);
                sfx(1400, 60);
                break;
            default: break;
        }
        bonus_type = PU_NONE;
    }
}

// drawing
static inline void nes_outline(int16_t x, int16_t y, int16_t w, int16_t h) {
    LCD_Draw_Rect(x, y, w, h, C_DARK, 0);
}

static void draw_player(void) {
    if (player_invuln > 0 && (anim_tick & 1)) return;
    uint8_t body  = (ghost_timer > 0) ? C_CYAN  : C_WHITE;
    uint8_t trim  = (ghost_timer > 0) ? C_BLUE  : C_RED;
    uint8_t cock  = C_CYAN;
    uint8_t fout  = (anim_tick & 1) ? C_ORANGE : C_RED;
    uint8_t fin   = (anim_tick & 1) ? C_YELLOW : C_ORANGE;

    int16_t px = PLAY_X + snake[0].x * CELL + shake_x;
    int16_t py = PLAY_Y + snake[0].y * CELL + shake_y;

    switch (dir) {
        case N:
            LCD_Draw_Rect(px + 7,  py + 1,  2, 3,  body, 1);
            LCD_Draw_Rect(px + 6,  py + 4,  4, 8,  body, 1);
            LCD_Draw_Rect(px + 2,  py + 7,  12,3,  body, 1);
            LCD_Draw_Rect(px + 1,  py + 8,  1, 2,  trim, 1);
            LCD_Draw_Rect(px + 14, py + 8,  1, 2,  trim, 1);
            LCD_Draw_Rect(px + 2,  py + 9,  3, 1,  trim, 1);
            LCD_Draw_Rect(px + 11, py + 9,  3, 1,  trim, 1);
            LCD_Draw_Rect(px + 7,  py + 5,  2, 2,  cock, 1);
            LCD_Draw_Rect(px + 7,  py + 5,  2, 2,  C_DARK, 0);
            LCD_Draw_Rect(px + 5,  py + 12, 2, 2,  fout, 1);
            LCD_Draw_Rect(px + 9,  py + 12, 2, 2,  fout, 1);
            LCD_Draw_Rect(px + 5,  py + 14, 2, 1,  fin,  1);
            LCD_Draw_Rect(px + 9,  py + 14, 2, 1,  fin,  1);
            break;
        case S:
            LCD_Draw_Rect(px + 7,  py + 12, 2, 3, body, 1);
            LCD_Draw_Rect(px + 6,  py + 4,  4, 8, body, 1);
            LCD_Draw_Rect(px + 2,  py + 6,  12,3, body, 1);
            LCD_Draw_Rect(px + 1,  py + 6,  1, 2, trim, 1);
            LCD_Draw_Rect(px + 14, py + 6,  1, 2, trim, 1);
            LCD_Draw_Rect(px + 2,  py + 6,  3, 1, trim, 1);
            LCD_Draw_Rect(px + 11, py + 6,  3, 1, trim, 1);
            LCD_Draw_Rect(px + 7,  py + 9,  2, 2, cock, 1);
            LCD_Draw_Rect(px + 7,  py + 9,  2, 2, C_DARK, 0);
            LCD_Draw_Rect(px + 5,  py + 1,  2, 2, fout, 1);
            LCD_Draw_Rect(px + 9,  py + 1,  2, 2, fout, 1);
            LCD_Draw_Rect(px + 5,  py + 1,  2, 1, fin,  1);
            LCD_Draw_Rect(px + 9,  py + 1,  2, 1, fin,  1);
            break;
        case E:
            LCD_Draw_Rect(px + 12, py + 7,  3, 2,  body, 1);
            LCD_Draw_Rect(px + 4,  py + 6,  8, 4,  body, 1);
            LCD_Draw_Rect(px + 7,  py + 2,  3, 12, body, 1);
            LCD_Draw_Rect(px + 7,  py + 1,  2, 1,  trim, 1);
            LCD_Draw_Rect(px + 7,  py + 14, 2, 1,  trim, 1);
            LCD_Draw_Rect(px + 6,  py + 2,  1, 3,  trim, 1);
            LCD_Draw_Rect(px + 6,  py + 11, 1, 3,  trim, 1);
            LCD_Draw_Rect(px + 9,  py + 7,  2, 2,  cock, 1);
            LCD_Draw_Rect(px + 9,  py + 7,  2, 2,  C_DARK, 0);
            LCD_Draw_Rect(px + 1,  py + 5,  3, 2,  fout, 1);
            LCD_Draw_Rect(px + 1,  py + 9,  3, 2,  fout, 1);
            LCD_Draw_Rect(px + 0,  py + 5,  1, 2,  fin,  1);
            LCD_Draw_Rect(px + 0,  py + 9,  1, 2,  fin,  1);
            break;
        case W:
            LCD_Draw_Rect(px + 1,  py + 7,  3, 2,  body, 1);
            LCD_Draw_Rect(px + 4,  py + 6,  8, 4,  body, 1);
            LCD_Draw_Rect(px + 6,  py + 2,  3, 12, body, 1);
            LCD_Draw_Rect(px + 7,  py + 1,  2, 1,  trim, 1);
            LCD_Draw_Rect(px + 7,  py + 14, 2, 1,  trim, 1);
            LCD_Draw_Rect(px + 9,  py + 2,  1, 3,  trim, 1);
            LCD_Draw_Rect(px + 9,  py + 11, 1, 3,  trim, 1);
            LCD_Draw_Rect(px + 5,  py + 7,  2, 2,  cock, 1);
            LCD_Draw_Rect(px + 5,  py + 7,  2, 2,  C_DARK, 0);
            LCD_Draw_Rect(px + 12, py + 5,  3, 2,  fout, 1);
            LCD_Draw_Rect(px + 12, py + 9,  3, 2,  fout, 1);
            LCD_Draw_Rect(px + 15, py + 5,  1, 2,  fin,  1);
            LCD_Draw_Rect(px + 15, py + 9,  1, 2,  fin,  1);
            break;
        default:
            LCD_Draw_Rect(px + 4, py + 4, 8, 8, body, 1);
            LCD_Draw_Rect(px + 6, py + 6, 4, 4, cock, 1);
            break;
    }

    if (ghost_timer > 0)
        LCD_Draw_Rect(px, py, CELL, CELL,
                      (anim_tick & 1) ? C_CYAN : C_PINK, 0);
    else if (shield_charges > 0)
        LCD_Draw_Rect(px, py, CELL, CELL, C_CYAN, 0);

    // OVERDRIVE aura: pixel-particles around the player
    if (speed_timer > 0 && mult_timer > 0) {
        uint8_t a = (uint8_t)(anim_tick & 0x03);
        uint8_t aura = (a & 1) ? C_YELLOW : C_PINK;
        switch (a) {
            case 0:
                LCD_Draw_Rect(px - 2,        py + CELL/2 - 1, 2, 2, aura, 1);
                LCD_Draw_Rect(px + CELL,     py + CELL/2 - 1, 2, 2, aura, 1);
                break;
            case 1:
                LCD_Draw_Rect(px + CELL/2-1, py - 2,          2, 2, aura, 1);
                LCD_Draw_Rect(px + CELL/2-1, py + CELL,       2, 2, aura, 1);
                break;
            case 2:
                LCD_Draw_Rect(px - 2,        py - 2,          2, 2, aura, 1);
                LCD_Draw_Rect(px + CELL,     py + CELL,       2, 2, aura, 1);
                break;
            default:
                LCD_Draw_Rect(px + CELL,     py - 2,          2, 2, aura, 1);
                LCD_Draw_Rect(px - 2,        py + CELL,       2, 2, aura, 1);
                break;
        }
    }
}

// enemy aircraft sprites
static void draw_one_enemy_plane(int16_t px, int16_t py, uint8_t type, uint8_t hp) {
    uint8_t flick = (anim_tick & 1);
    switch (type) {
        case ENEMY_TYPE_CHASER: {
            LCD_Draw_Rect(px + 7, py + 3,  2, 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 7, py + 4,  2, 8, C_RED,    1);
            LCD_Draw_Rect(px + 4, py + 7,  8, 2, C_RED,    1);
            LCD_Draw_Rect(px + 4, py + 8,  2, 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 10,py + 8,  2, 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 5, py + 11, 6, 1, C_RED,    1);
            LCD_Draw_Rect(px + 7, py + 6,  2, 2, C_YELLOW, 1);
            LCD_Draw_Rect(px + 7, py + 6,  2, 2, C_DARK,   0);
            LCD_Draw_Rect(px + 7, py + 12, 2, flick ? 2 : 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 7, py + 14, 2, flick ? 1 : 0, C_YELLOW, 1);
            break;
        }
        case ENEMY_TYPE_SHOOTER: {
            LCD_Draw_Rect(px + 7, py + 1,  2, 3,  C_WHITE,  1);
            LCD_Draw_Rect(px + 6, py + 4,  4, 7,  C_PINK,   1);
            LCD_Draw_Rect(px + 3, py + 7,  10,2,  C_TEAL,   1);
            LCD_Draw_Rect(px + 3, py + 8,  2, 1,  C_LIME,   1);
            LCD_Draw_Rect(px + 11,py + 8,  2, 1,  C_LIME,   1);
            LCD_Draw_Rect(px + 5, py + 11, 1, 2,  C_TEAL,   1);
            LCD_Draw_Rect(px + 10,py + 11, 1, 2,  C_TEAL,   1);
            LCD_Draw_Rect(px + 7, py + 5,  2, 2,  C_CYAN,   1);
            LCD_Draw_Rect(px + 7, py + 5,  2, 2,  C_DARK,   0);
            LCD_Draw_Rect(px + 7, py + 13, 2, flick ? 2 : 1, C_ORANGE, 1);
            break;
        }
        case ENEMY_TYPE_FAST: {
            LCD_Draw_Rect(px + 8,  py + 1, 1, 1,  C_WHITE, 1);
            LCD_Draw_Rect(px + 7,  py + 2, 2, 10, C_GREEN, 1);
            LCD_Draw_Rect(px + 7,  py + 3, 2, 2,  C_LIME,  1);
            LCD_Draw_Rect(px + 5,  py + 8, 6, 1,  C_LIME,  1);
            LCD_Draw_Rect(px + 4,  py + 9, 8, 1,  C_GREEN, 1);
            LCD_Draw_Rect(px + 6,  py + 12,1, 1,  C_GREEN, 1);
            LCD_Draw_Rect(px + 9,  py + 12,1, 1,  C_GREEN, 1);
            LCD_Draw_Rect(px + 7,  py + 13,2, flick ? 3 : 2, C_CYAN,  1);
            LCD_Draw_Rect(px + 7,  py + 15,2, flick ? 1 : 0, C_WHITE, 1);
            break;
        }
        case ENEMY_TYPE_TANK: {
            LCD_Draw_Rect(px + 7, py + 1,  2, 2,  C_BROWN, 1);
            LCD_Draw_Rect(px + 5, py + 3,  6, 10, C_BROWN, 1);
            LCD_Draw_Rect(px + 5, py + 4,  6, 1,  C_GREY,  1);
            LCD_Draw_Rect(px + 5, py + 12, 6, 1,  C_DARK,  1);
            LCD_Draw_Rect(px + 1, py + 6,  14,3,  C_GREY,  1);
            LCD_Draw_Rect(px + 1, py + 6,  14,1,  C_LIGHT, 1);
            LCD_Draw_Rect(px + 3, py + 13, 4, 1,  C_BROWN, 1);
            LCD_Draw_Rect(px + 9, py + 13, 4, 1,  C_BROWN, 1);
            LCD_Draw_Rect(px + 6, py + 4,  4, 2,  C_RED,   1);
            LCD_Draw_Rect(px + 7, py + 4,  2, 1,  C_YELLOW,1);
            LCD_Draw_Rect(px + 2, py + 7,  2, 1,  C_RED,   1);
            LCD_Draw_Rect(px + 12,py + 7,  2, 1,  C_RED,   1);
            LCD_Draw_Rect(px + 2, py + 8,  2, 1,  C_YELLOW,1);
            LCD_Draw_Rect(px + 12,py + 8,  2, 1,  C_YELLOW,1);
            LCD_Draw_Rect(px + 6, py + 13, 1, flick ? 2 : 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 9, py + 13, 1, flick ? 2 : 1, C_ORANGE, 1);
            for (uint8_t h = 0; h < hp; h++)
                LCD_Draw_Rect(px + 2 + h * 4, py - 1, 2, 2, C_LIME, 1);
            break;
        }
        default: break;
    }
}
static void draw_enemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        int16_t px = PLAY_X + e->body[0].x * CELL + shake_x;
        int16_t py = PLAY_Y + e->body[0].y * CELL + shake_y;
        draw_one_enemy_plane(px, py, e->type, e->hp);
    }
}

static void draw_bullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        int16_t px = PLAY_X + bullets[i].x * CELL + shake_x;
        int16_t py = PLAY_Y + bullets[i].y * CELL + shake_y;
        uint8_t core, glow;
        if (bullets[i].owner == BULLET_PLAYER) {
            core = C_YELLOW; glow = C_WHITE;
        } else {
            core = C_RED;    glow = C_ORANGE;
        }
        int16_t cx = px + CELL/2, cy = py + CELL/2;
        switch (bullets[i].dir) {
            case N:
                LCD_Draw_Rect(cx - 1, py + 3,    3, CELL - 6, core, 1);
                LCD_Draw_Rect(cx,     py + 3,    1, 3,        glow, 1);
                break;
            case S:
                LCD_Draw_Rect(cx - 1, py + 3,    3, CELL - 6, core, 1);
                LCD_Draw_Rect(cx,     py + CELL - 6, 1, 3,    glow, 1);
                break;
            case E:
                LCD_Draw_Rect(px + 3, cy - 1,    CELL - 6, 3, core, 1);
                LCD_Draw_Rect(px + CELL - 6, cy, 3, 1,        glow, 1);
                break;
            case W:
                LCD_Draw_Rect(px + 3, cy - 1,    CELL - 6, 3, core, 1);
                LCD_Draw_Rect(px + 3, cy,        3, 1,        glow, 1);
                break;
            default:
                LCD_Draw_Rect(cx - 2, cy - 2, 4, 4, core, 1); break;
        }
    }
}

static void draw_food(void) {
    int16_t size = (anim_tick & 0x02) ? CELL - 4 : CELL - 8;
    int16_t px = PLAY_X + food.x * CELL + shake_x;
    int16_t py = PLAY_Y + food.y * CELL + shake_y;
    int16_t pad = (CELL - size) / 2;
    LCD_Draw_Rect(px + pad,        py + pad,        size,    size,    C_CYAN,   1);
    LCD_Draw_Rect(px + pad + 1,    py + pad + 1,    size-2,  size-2,  C_YELLOW, 1);
    LCD_Draw_Rect(px + pad + 2,    py + pad + 2,    size-4,  size-4,  C_WHITE,  1);
    LCD_Draw_Rect(px + pad + 3,    py + pad + 3,    size-6,  size-6,  C_GREEN,  1);
    LCD_Draw_Rect(px + pad,        py + pad,        size,    size,    C_DARK,   0);
}

static void draw_obstacles(void) {
    for (uint8_t i = 0; i < obstacle_count; i++) {
        int16_t px = PLAY_X + obstacles[i].x * CELL + shake_x;
        int16_t py = PLAY_Y + obstacles[i].y * CELL + shake_y;
        LCD_Draw_Rect(px + 1, py + 1, CELL - 2,  CELL - 2,  C_BROWN, 1);
        LCD_Draw_Rect(px + 2, py + 2, CELL - 4,  CELL - 4,  C_DARK,  1);
        LCD_Draw_Rect(px + 4, py + 4, CELL - 8,  CELL - 8,  C_GREY,  1);
        uint8_t pulse = (anim_tick & 0x02) ? C_RED : C_YELLOW;
        LCD_Draw_Rect(px + 6, py + 6, CELL - 12, CELL - 12, pulse,   1);
        LCD_Draw_Rect(px + 1, py + 1, CELL - 2,  CELL - 2,  C_DARK,  0);
    }
}

static void draw_bonus(void) {
    if (!bonus_active) return;
    if (bonus_ttl < 15 && (anim_tick & 1)) return;
    int16_t px = PLAY_X + bonus.x * CELL + shake_x;
    int16_t py = PLAY_Y + bonus.y * CELL + shake_y;

    uint8_t frame, accent;
    switch (bonus_type) {
        case PU_HEAL:   frame = C_GREEN;  accent = C_LIME;   break;
        case PU_RAPID:  frame = C_YELLOW; accent = C_ORANGE; break;
        case PU_SHIELD: frame = C_BLUE;   accent = C_CYAN;   break;
        case PU_SPREAD: frame = C_PINK;   accent = C_RED;    break;
        case PU_AMMO:   frame = C_ORANGE; accent = C_YELLOW; break;
        default:        frame = C_WHITE;  accent = C_GREY;   break;
    }
    LCD_Draw_Rect(px + 1, py + 1, CELL - 2, CELL - 2, accent, 0);
    LCD_Draw_Rect(px + 2, py + 2, CELL - 4, CELL - 4, frame,  0);

    switch (bonus_type) {
        case PU_HEAL:
            LCD_Draw_Rect(px + CELL/2 - 1, py + 4,         3, CELL - 8, C_RED, 1);
            LCD_Draw_Rect(px + 4,          py + CELL/2-1, CELL - 8, 3,  C_RED, 1);
            LCD_Draw_Rect(px + CELL/2,     py + CELL/2,    1, 1,        C_WHITE, 1);
            break;
        case PU_RAPID:
            LCD_Draw_Rect(px + 8,  py + 3, 2, 4, C_WHITE,  1);
            LCD_Draw_Rect(px + 5,  py + 6, 4, 2, C_YELLOW, 1);
            LCD_Draw_Rect(px + 7,  py + 8, 2, 4, C_WHITE,  1);
            LCD_Draw_Rect(px + 9,  py + 9, 2, 3, C_ORANGE, 1);
            break;
        case PU_SHIELD:
            LCD_Draw_Rect(px + 4, py + 4, CELL - 8, CELL - 8, C_CYAN,  1);
            LCD_Draw_Rect(px + 5, py + 5, CELL - 10, CELL - 10, C_BLUE, 0);
            LCD_Draw_Rect(px + 7, py + 7, 2, 2, C_WHITE, 1);
            break;
        case PU_SPREAD:
            LCD_Draw_Rect(px + CELL/2 - 1, py + 4, 3, CELL - 8, C_WHITE, 1);
            LCD_Draw_Rect(px + 3,          py + 6, 3, 2,        C_PINK,  1);
            LCD_Draw_Rect(px + CELL - 6,   py + 6, 3, 2,        C_PINK,  1);
            LCD_Draw_Rect(px + 4,          py + 9, 2, 1,        C_RED, 1);
            LCD_Draw_Rect(px + CELL - 6,   py + 9, 2, 1,        C_RED, 1);
            break;
        case PU_AMMO:
            LCD_Draw_Rect(px + 3, py + 5, CELL - 6, CELL - 9, C_BROWN, 1);
            LCD_Draw_Rect(px + 3, py + 4, CELL - 6, 2,        C_YELLOW,1);
            LCD_Draw_Rect(px + 4,  py + 2, 2, 2, C_RED,    1);
            LCD_Draw_Rect(px + 7,  py + 2, 2, 2, C_RED,    1);
            LCD_Draw_Rect(px + 10, py + 2, 2, 2, C_RED,    1);
            LCD_Draw_Rect(px + 4,  py + 2, 2, 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 7,  py + 2, 2, 1, C_ORANGE, 1);
            LCD_Draw_Rect(px + 10, py + 2, 2, 1, C_ORANGE, 1);
            break;
        default: break;
    }
    LCD_Draw_Rect(px + 1, py + 1, CELL - 2, CELL - 2, C_DARK, 0);
}

// SKILL ICONS in HUD
static void draw_skill_icon(int16_t x, int16_t y, uint8_t s) {
    uint16_t energy = skill_energy[s];
    uint16_t mx     = skill_max[s];
    uint8_t ready   = (energy >= mx);
    uint8_t fill_h  = (uint8_t)(((uint32_t)energy * 12) / mx);
    if (fill_h > 12) fill_h = 12;
    uint8_t fill_col = (s == SKILL_EMP) ? C_CYAN : C_ORANGE;

    // base plate
    LCD_Draw_Rect(x,     y,     14, 14, C_DARK, 1);
    LCD_Draw_Rect(x + 1, y + 1, 12, 12, C_NAVY, 1);

    // energy fill from the bottom
    if (fill_h > 0) {
        LCD_Draw_Rect(x + 1, (int16_t)(y + 13 - fill_h),
                      12, fill_h, fill_col, 1);
        if (fill_h >= 2)
            LCD_Draw_Rect(x + 1, (int16_t)(y + 13 - fill_h),
                          12, 1, C_LIGHT, 1);
    }

    // icon glyph
    uint8_t glyph = ready ? C_WHITE : C_LIGHT;
    if (s == SKILL_EMP) {
        // 4-way burst / plus
        LCD_Draw_Rect(x + 2, y + 6, 10, 2, glyph, 1);
        LCD_Draw_Rect(x + 6, y + 2, 2, 10, glyph, 1);
        uint8_t spark = ready ? C_YELLOW : C_GREY;
        LCD_Draw_Rect(x + 3, y + 3, 2, 2, spark, 1);
        LCD_Draw_Rect(x + 9, y + 3, 2, 2, spark, 1);
        LCD_Draw_Rect(x + 3, y + 9, 2, 2, spark, 1);
        LCD_Draw_Rect(x + 9, y + 9, 2, 2, spark, 1);
    } else {
        // up-arrow / chevron => OVERDRIVE
        LCD_Draw_Rect(x + 6, y + 2,  2, 10, glyph, 1);
        LCD_Draw_Rect(x + 4, y + 4,  2, 2,  glyph, 1);
        LCD_Draw_Rect(x + 8, y + 4,  2, 2,  glyph, 1);
        LCD_Draw_Rect(x + 2, y + 6,  2, 2,  glyph, 1);
        LCD_Draw_Rect(x + 10, y + 6, 2, 2,  glyph, 1);
        if (ready) {
            LCD_Draw_Rect(x + 5, y + 9,  4, 1, C_YELLOW, 1);
            LCD_Draw_Rect(x + 5, y + 11, 4, 1, C_YELLOW, 1);
        }
    }

    // outer frame: pulse yellow when ready, else dark
    uint8_t fcol;
    if (ready)
        fcol = (anim_tick & 1) ? C_YELLOW : C_WHITE;
    else
        fcol = C_DARK;
    LCD_Draw_Rect(x, y, 14, 14, fcol, 0);

    // button hint underline (BTN1 / BTN2)
    uint8_t btn_col = ready ? C_LIME : C_GREY;
    LCD_Draw_Rect(x + 4, (int16_t)(y + 14), 6, 1, btn_col, 1);
}

static void draw_header(void) {
    char buf[32];
    // Famicom-style status bar
    LCD_Draw_Rect(0, 0,            SCREEN_W, HEADER_H, C_NAVY,  1);
    LCD_Draw_Rect(0, 0,            SCREEN_W, 1,        C_LIME,  1);
    LCD_Draw_Rect(0, HEADER_H - 1, SCREEN_W, 1,        C_LIME,  1);

    if (game_over) {
        LCD_printString(victory ? "FIREWALL HOLDS!" : "FIREWALL DOWN!",
                        4, 8, victory ? C_LIME : C_ORANGE, 1);
        return;
    }

    // row 1: stats
    sprintf(buf, "W%u",  level);    LCD_printString(buf,   4, 2, C_CYAN,   1);
    sprintf(buf, "%u",   score);    LCD_printString(buf,  28, 2, C_YELLOW, 1);
    if (combo_count > 1) {
        sprintf(buf, "x%u", combo_count);
        LCD_printString(buf, 88, 2, C_ORANGE, 1);
    }
    LCD_printString("HP", 116, 2, C_GREEN, 1);
    for (uint8_t i = 0; i < MAX_HP; i++) {
        uint16_t bx = 134 + i * 8;
        if (i < player_hp) {
            LCD_Draw_Rect(bx, 3, 6, 6, C_RED,  1);
            LCD_Draw_Rect(bx, 3, 6, 1, C_PINK, 1);
            LCD_Draw_Rect(bx, 3, 6, 6, C_DARK, 0);
        } else {
            LCD_Draw_Rect(bx, 3, 6, 6, C_DARK, 0);
        }
    }
    sprintf(buf, "L%u",  lives);        LCD_printString(buf, 174, 2, C_PINK,   1);
    sprintf(buf, "Ax%u", bullet_count); LCD_printString(buf, 198, 2, C_YELLOW, 1);

    // row 2: progress bar
    uint16_t bw = SCREEN_W - 8;
    uint16_t fill = packets_needed
        ? (uint16_t)((uint32_t)packets_collected * bw / packets_needed) : 0;
    if (fill > bw) fill = bw;
    LCD_Draw_Rect(4, 12, bw, 4, C_DARK, 1);
    LCD_Draw_Rect(4, 12, bw, 4, C_GREY, 0);
    if (fill > 0) {
        LCD_Draw_Rect(4, 12, fill, 4, C_CYAN,  1);
        LCD_Draw_Rect(4, 12, fill, 1, C_LIGHT, 1);
    }

    // row 3: skills + indicators + counter
    draw_skill_icon(4,  17, SKILL_EMP);
    draw_skill_icon(22, 17, SKILL_OVERDRIVE);

    uint16_t ex = 42;
    if (speed_timer > 0) { LCD_Draw_Rect(ex, 22, 5, 5, C_YELLOW, 1); ex += 7; }
    if (ghost_timer > 0) { LCD_Draw_Rect(ex, 22, 5, 5, C_BLUE,   1); ex += 7; }
    if (mult_timer  > 0) { LCD_Draw_Rect(ex, 22, 5, 5, C_PINK,   1); ex += 7; }

    sprintf(buf, "%u/%u", packets_collected, packets_needed);
    LCD_printString(buf, SCREEN_W - 50, 22, C_LIME, 1);
}

static void draw_border(void) {
    uint8_t c = C_BLUE;
    if (game_over)          c = C_RED;
    else if (juice_counter) c = C_YELLOW;
    LCD_Draw_Rect(0, PLAY_Y, SCREEN_W, SCREEN_H - PLAY_Y, c, 0);
}

static void draw_skill_flash(void) {
    if (skill_flash == 0) return;
    // Rapid white/cyan ring on EMP discharge
    uint8_t c = (skill_flash & 1) ? C_WHITE : C_CYAN;
    LCD_Draw_Rect(0,  PLAY_Y,      SCREEN_W,     SCREEN_H - PLAY_Y,     c, 0);
    LCD_Draw_Rect(1,  PLAY_Y + 1,  SCREEN_W - 2, SCREEN_H - PLAY_Y - 2, c, 0);
    LCD_Draw_Rect(2,  PLAY_Y + 2,  SCREEN_W - 4, SCREEN_H - PLAY_Y - 4, c, 0);
    if (skill_flash >= SKILL_FLASH_FRAMES - 1) {
        // full overlay on first frame
        LCD_Draw_Rect(4, PLAY_Y + 4,
                      SCREEN_W - 8, SCREEN_H - PLAY_Y - 8, c, 0);
    }
}

static void draw_frame(void) {
    if (juice_counter > JUICE_FRAMES - 2) {
        shake_x = (anim_tick & 1) ? 2 : -2;
        shake_y = (anim_tick & 2) ? 2 : -2;
    } else { shake_x = shake_y = 0; }

    LCD_Fill_Buffer(C_BG);
    draw_stars();
    draw_header();
    draw_border();
    draw_obstacles();
    draw_food();
    draw_bonus();
    draw_enemies();
    draw_bullets();
    draw_explosions();
    draw_player();
    draw_skill_flash();
    LCD_Refresh(&cfg0);
}

// systems
static void update_led(void) {
    if (led_flash_counter > 0) {
        led_flash_counter--;
        led_state = !led_state;
        led_set(led_state);
        if (led_flash_counter == 0) { led_state = 0; led_set(0); }
    }
}
static void update_effects(void) {
    if (speed_timer)    speed_timer--;
    if (ghost_timer)    ghost_timer--;
    if (mult_timer)     mult_timer--;
    if (player_invuln)  player_invuln--;
    if (fire_cooldown)  fire_cooldown--;
    if (portal_cooldown) portal_cooldown--;
    if (skill_flash)    skill_flash--;
    if (bonus_active && bonus_ttl) {
        bonus_ttl--;
        if (!bonus_ttl) { bonus_active = 0; bonus_type = PU_NONE; }
    }
    if (combo_timer) {
        combo_timer--;
        if (!combo_timer) combo_count = 0;
    }
    // skill-ready chime (one-shot per fill)
    for (uint8_t s = 0; s < SKILL_COUNT; s++) {
        if (skill_energy[s] >= skill_max[s]) {
            if (!skill_ready_seen[s]) {
                skill_ready_seen[s] = 1;
                led_flash_counter = 4;
                sfx(s == SKILL_EMP ? 1760 : 1480, 25);
            }
        }
    }
}

static void update_game(void) {
    update_stars();
    update_explosions();

    player_shoot();
    update_bullets();
    if (game_over) return;
    update_bullets();
    if (game_over) return;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) {
            enemy_step(i);
            if (game_over) return;
            enemy_try_shoot(i);
        }
    }
    try_spawn_enemy();

    if (packets_collected >= packets_needed) {
        uint8_t any_alive = 0;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].alive) { any_alive = 1; break; }
        if (!any_alive) level_complete = 1;
    }
    enemy_move_phase++;
}

// init
static void generate_obstacles(void) {
    obstacle_count = 0;
    if (level <= 1) return;
    uint8_t target = (level - 1) * 2 + 1;
    if (target > MAX_OBSTACLES) target = MAX_OBSTACLES;
    int8_t cx = COLS / 2, cy = ROWS / 2;
    uint16_t attempts = 0;
    while (obstacle_count < target && attempts < 250) {
        attempts++;
        int8_t x = rand() % COLS;
        int8_t y = rand() % ROWS;
        if (abs8((int8_t)(x - cx)) + abs8((int8_t)(y - cy)) < 3) continue;
        if (cell_blocked(x, y)) continue;
        uint8_t neighbours = 0;
        for (uint8_t k = 0; k < obstacle_count; k++)
            if (abs8((int8_t)(obstacles[k].x - x)) <= 1 &&
                abs8((int8_t)(obstacles[k].y - y)) <= 1) neighbours++;
        if (neighbours > 1) continue;
        obstacles[obstacle_count].x = x;
        obstacles[obstacle_count].y = y;
        obstacle_count++;
    }
}

static void level_init(void) {
    snake[0].x = COLS / 2;
    snake[0].y = ROWS / 2;
    snake_len = 1;
    dir = next_dir = E;

    for (int i = 0; i < MAX_ENEMIES; i++)   enemies[i].alive    = 0;
    for (int i = 0; i < MAX_BULLETS; i++)   bullets[i].active   = 0;
    for (int i = 0; i < EXPLOSION_MAX; i++) explosions[i].active= 0;
    enemy_spawn_timer = 30;
    enemy_move_phase  = 0;

    bonus_active = 0; bonus_ttl = 0; bonus_type = PU_NONE;

    speed_timer = ghost_timer = mult_timer = 0;
    player_invuln = 0;
    fire_cooldown = 0;
    shield_charges = 0;
    combo_count = 0; combo_timer = 0;
    portal_cooldown = 0; portals_active = 0;
    skill_flash = 0;

    juice_counter = 0;
    shake_x = shake_y = 0;
    game_over = victory = 0;
    level_complete = 0;

    obstacle_count = 0;
    generate_obstacles();

    packets_collected = 0;
    packets_needed    = 4 + level * 2;

    food.x = -1; food.y = -1;
    place_food();

    init_stars();

    led_flash_counter = 0;
    led_state = 0;
    led_set(0);

    prev_btn1 = prev_btn2 = 0;
    // note: skill_energy is preserved between waves on purpose
}

static void reset_skills_full(void) {
    for (uint8_t s = 0; s < SKILL_COUNT; s++) {
        skill_energy[s] = 0;
        skill_ready_seen[s] = 0;
    }
    skill_flash = 0;
}

static void game_init(void) { level_init(); }

static uint16_t current_tick_ms(void) {
    int16_t reduction = level * 4;
    int16_t base = TICK_MS_BASE - reduction;
    if (base < TICK_MS_MIN) base = TICK_MS_MIN;
    return (uint16_t)base;
}

// skip helpers
static uint8_t check_skip(void) {
    Input_Read();
    if (current_input.btn3_pressed) return 1;
    Joystick_Read(&joystick_cfg, &joystick_data);
    if (Joystick_GetInput(&joystick_data).direction != CENTRE) return 1;
    return 0;
}
static void wait_release(void) {
    for (uint16_t guard = 0; guard < 100; guard++) {
        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);
        if (!current_input.btn3_pressed &&
            Joystick_GetInput(&joystick_data).direction == CENTRE) return;
        HAL_Delay(20);
    }
}

// animations
static void show_intro(void) {
    LCD_Fill_Buffer(C_BG); LCD_Refresh(&cfg0);
    uint8_t scan_pal[4] = { C_RED, C_YELLOW, C_GREEN, C_BLUE };
    for (uint16_t y = 0; y < SCREEN_H; y += 6) {
        LCD_Draw_Rect(0, y, SCREEN_W, 2, scan_pal[(y / 6) & 3], 1);
        LCD_Refresh(&cfg0);
        if (check_skip()) { wait_release(); return; }
        LCD_Draw_Rect(0, y, SCREEN_W, 2, C_BG, 1);
    }
    sfx(2000, 20);

    const char *title = "FIREWALL";
    char buf[32] = {0};
    size_t tlen = strlen(title);
    for (size_t i = 0; i < tlen; i++) {
        buf[i] = title[i]; buf[i+1] = '\0';
        LCD_Fill_Buffer(C_BG);
        LCD_printString(buf, 60, 50, C_YELLOW, 2);
        LCD_Draw_Rect((uint16_t)(60 + (i+1) * 12), 50, 8, 16, C_CYAN, 1);
        LCD_Refresh(&cfg0);
        sfx(2200, 8);
        HAL_Delay(70);
        if (check_skip()) { wait_release(); return; }
    }
    LCD_printString("DEFENDER",     60, 75,  C_ORANGE, 2);
    LCD_printString("8-BIT v3.5",   78, 100, C_LIME,   1);
    LCD_Refresh(&cfg0);
    HAL_Delay(300);

    const char *logs[5] = {
        "> turret: online",
        "> radar: scanning",
        "> BTN1: EMP burst",
        "> BTN2: overdrive",
        "> defend the core",
    };
    uint8_t log_cols[5] = { C_GREEN, C_CYAN, C_YELLOW, C_ORANGE, C_PINK };
    for (uint8_t i = 0; i < 5; i++) {
        LCD_printString(logs[i], 8, (uint16_t)(125 + i * 14), log_cols[i], 1);
        LCD_Refresh(&cfg0);
        sfx(1500, 6);
        HAL_Delay(110);
        if (check_skip()) { wait_release(); return; }
    }

    const uint16_t bx = 20, by = 200, bw = 200, bh = 12;
    LCD_Draw_Rect(bx, by, bw, bh, C_BLUE, 0);
    for (uint16_t w = 0; w <= bw - 4; w += 8) {
        LCD_Draw_Rect((uint16_t)(bx + 2), (uint16_t)(by + 2), w, (uint16_t)(bh - 4),
                      C_LIME, 1);
        LCD_Refresh(&cfg0);
        HAL_Delay(20);
        if (check_skip()) { wait_release(); return; }
    }
    sfx(880, 80);
    HAL_Delay(180);

    uint8_t blink = 0;
    while (!check_skip()) {
        LCD_Draw_Rect(60, 222, 130, 12, C_BG, 1);
        if (blink & 1) LCD_printString(">> PRESS BTN3 <<", 60, 222, C_YELLOW, 1);
        LCD_Refresh(&cfg0);
        if (blink & 1) sfx(1320, 20);
        HAL_Delay(250);
        blink++;
    }
    wait_release();
}

static void show_level_intro(void) {
    char buf[32];
    uint8_t cols[2] = { C_YELLOW, C_CYAN };
    for (uint8_t k = 0; k < 6; k++) {
        LCD_Fill_Buffer(C_BG);
        sprintf(buf, "WAVE %u", level);
        LCD_printString(buf, 80, 80, cols[k & 1], 2);
        sprintf(buf, "Threats: %u", packets_needed);
        LCD_printString(buf, 70, 120, C_LIME, 1);

        const char *hint;
        switch (level) {
            case 1: hint = "BTN1=EMP  BTN2=OVERDRIVE"; break;
            case 2: hint = "Shooters firing back!";    break;
            case 3: hint = "Watch the firewalls";      break;
            case 4: hint = "Fast scouts incoming";     break;
            case 5: hint = "Combo for big bonuses";    break;
            case 6: hint = "Tank class detected";      break;
            case 7: hint = "Heavy assault wave";       break;
            default:hint = "FINAL WAVE";               break;
        }
        LCD_printString(hint, 8, 150, C_ORANGE, 1);
        LCD_Refresh(&cfg0);
        if (k == 0) sfx(SFX_LEVEL_FREQ, SFX_LEVEL_MS);
        HAL_Delay(180);
    }
    HAL_Delay(300);
}

static void show_level_clear(void) {
    sfx(SFX_LEVEL_FREQ, SFX_LEVEL_MS);
    HAL_Delay(60);
    sfx(SFX_LEVEL_FREQ + 300, SFX_LEVEL_MS);
    char buf[32];
    uint8_t flicker[2] = { C_LIME, C_YELLOW };
    for (uint8_t i = 0; i < 4; i++) {
        LCD_Fill_Buffer(C_BG);
        sprintf(buf, "WAVE %u CLEAR!", level);
        LCD_printString(buf, 50, 70, flicker[i & 1], 2);
        sprintf(buf, "Score: %u", score);
        LCD_printString(buf, 80, 110, C_CYAN, 1);
        sprintf(buf, "Bonus: +%u", level * 30);
        LCD_printString(buf, 75, 130, C_ORANGE, 1);
        sprintf(buf, "Next: WAVE %u", level + 1);
        LCD_printString(buf, 60, 160, C_PINK, 1);
        LCD_Refresh(&cfg0);
        HAL_Delay(200);
    }
    score += level * 30;
    HAL_Delay(400);
}

static void show_outro(void) {
    uint8_t alert_colour = victory ? C_LIME : C_RED;
    for (uint8_t i = 0; i < 3; i++) {
        LCD_Fill_Buffer(alert_colour); LCD_Refresh(&cfg0);
        sfx(victory ? 660 : 220, 60);
        HAL_Delay(40);
        draw_frame();
        HAL_Delay(60);
    }
    uint8_t streak_pal[4] = { C_RED, C_YELLOW, C_CYAN, C_PINK };
    for (uint8_t i = 0; i < 12; i++) {
        uint16_t y = (uint16_t)(HEADER_H + (rand() % (SCREEN_H - HEADER_H - 4)));
        uint8_t  c = streak_pal[i & 3];
        LCD_Draw_Rect(2, y, (uint16_t)(SCREEN_W - 4), 2, c, 1);
        LCD_Refresh(&cfg0);
        sfx((uint32_t)(120 + (rand() % 400)), 10);
        HAL_Delay(35);
    }
    LCD_Fill_Buffer(C_BG);
    if (victory) {
        LCD_printString("DEFENDED",     60, 100, C_LIME,   2);
        LCD_printString("ALL WAVES",    50, 130, C_CYAN,   2);
        LCD_printString("CORE SECURED", 55, 165, C_YELLOW, 1);
    } else {
        LCD_printString("FIREWALL", 70, 100, C_RED,    2);
        LCD_printString("BREACHED", 70, 130, C_ORANGE, 2);
    }
    LCD_Refresh(&cfg0);
    sfx(victory ? SFX_VICTORY_FREQ : 110,
        victory ? SFX_VICTORY_MS   : 400);
    HAL_Delay(700);
}

static void show_life_lost(void) {
    for (uint8_t i = 0; i < 4; i++) { led_set(1); HAL_Delay(80); led_set(0); HAL_Delay(80); }
    for (uint8_t i = 0; i < 2; i++) {
        LCD_Fill_Buffer(C_RED); LCD_Refresh(&cfg0); HAL_Delay(70);
        LCD_Fill_Buffer(C_BG);  LCD_Refresh(&cfg0); HAL_Delay(70);
    }
    LCD_Fill_Buffer(C_BG);
    LCD_printString("CORE HIT!", 70, 50, C_RED, 2);
    char buf[32];
    sprintf(buf, "Wave %u  -  Lives: %u", level, lives);
    LCD_printString(buf, 35, 90, C_YELLOW, 1);
    for (uint8_t i = 0; i < MAX_LIVES; i++) {
        uint16_t x = (uint16_t)(70 + i * 35);
        if (i < lives) {
            LCD_Draw_Rect(x, 120, 22, 22, C_RED,    1);
            LCD_Draw_Rect(x, 120, 22, 4,  C_PINK,   1);
            LCD_Draw_Rect(x + 8, 128, 6, 6, C_YELLOW, 1);
            LCD_Draw_Rect(x, 120, 22, 22, C_DARK,   0);
        } else {
            LCD_Draw_Rect(x, 120, 22, 22, C_DARK,   0);
        }
    }
    sprintf(buf, "Score: %u", score);
    LCD_printString(buf, 70, 165, C_CYAN, 1);
    LCD_printString("Rebooting...", 65, 200, C_LIME, 1);
    LCD_Refresh(&cfg0);
    HAL_Delay(1500);
}

static void show_game_over(void) {
    if (score > hi_score) hi_score = score;
    const uint16_t bw = 210, bh = 130;
    const uint16_t bx = (SCREEN_W - bw) / 2;
    const uint16_t by = (SCREEN_H - bh) / 2;
    for (int i = 0; i < 2; i++) {
        LCD_Draw_Rect(bx, by, bw, bh, victory ? C_LIME : C_RED, 1);
        LCD_Refresh(&cfg0); HAL_Delay(80);
        LCD_Draw_Rect(bx, by, bw, bh, C_BG, 1);
        LCD_Refresh(&cfg0); HAL_Delay(80);
    }
    LCD_Draw_Rect(bx, by, bw, bh, C_BG, 1);
    LCD_Draw_Rect(bx, by, bw, bh, victory ? C_LIME : C_RED, 0);
    LCD_Draw_Rect(bx + 1, by + 1, bw - 2, bh - 2, victory ? C_CYAN : C_ORANGE, 0);
    LCD_printString(victory ? "DEFENSE COMPLETE" : "FIREWALL DOWN",
                    (uint16_t)(bx + 12), (uint16_t)(by + 12),
                    victory ? C_LIME : C_RED, 2);
    char buf[24];
    sprintf(buf, "Final Score: %u", score);
    LCD_printString(buf, (uint16_t)(bx + 24), (uint16_t)(by + 46), C_YELLOW, 1);
    sprintf(buf, "High Score:  %u", hi_score);
    LCD_printString(buf, (uint16_t)(bx + 24), (uint16_t)(by + 60), C_CYAN, 1);
    sprintf(buf, "Reached: Wave %u", level);
    LCD_printString(buf, (uint16_t)(bx + 24), (uint16_t)(by + 74), C_PINK, 1);
    LCD_printString(victory ? "All Threats Eliminated" : "Core Compromised",
                    (uint16_t)(bx + 14), (uint16_t)(by + 96),
                    victory ? C_LIME : C_ORANGE, 1);
    LCD_printString("BTN3:Menu  Stick:Retry",
                    (uint16_t)(bx + 8),  (uint16_t)(by + 112), C_GREEN, 1);
    LCD_Refresh(&cfg0);
}

static uint8_t wait_for_restart(void) {
    HAL_Delay(600);
    for (;;) {
        Input_Read();
        if (current_input.btn3_pressed) return 1;
        Joystick_Read(&joystick_cfg, &joystick_data);
        if (Joystick_GetInput(&joystick_data).direction == CENTRE) break;
        HAL_Delay(20);
    }
    for (;;) {
        Input_Read();
        if (current_input.btn3_pressed) return 1;
        Joystick_Read(&joystick_cfg, &joystick_data);
        if (Joystick_GetInput(&joystick_data).direction != CENTRE) break;
        HAL_Delay(20);
    }
    return 0;
}

// entry
MenuState Game1_Run(void) {
    srand(HAL_GetTick());

    LCD_Set_Palette(PALETTE_VINTAGE);

    if (!intro_played) { show_intro(); intro_played = 1; }

    lives        = MAX_LIVES;
    score        = 0;
    level        = 1;
    player_hp    = MAX_HP;
    bullet_count = 1;
    reset_skills_full();

    show_level_intro();

    for (;;) {
        game_init();

        while (!game_over && !level_complete) {
            Input_Read();
            if (current_input.btn3_pressed) {
                led_set(0);
                buzzer_off(&buzzer_cfg);
                LCD_Fill_Buffer(C_BG); LCD_Refresh(&cfg0);
                LCD_Set_Palette(PALETTE_DEFAULT);
                return MENU_STATE_HOME;
            }

            // SKILL TRIGGERS (edge-detected)
            uint8_t b1 = current_input.btn2_pressed;
            

            if (b1 && !prev_btn1) {
                try_use_skill(SKILL_EMP);
                try_use_skill(SKILL_OVERDRIVE);
            }

            prev_btn1 = b1;

            handle_input();
            if (!game_over && !level_complete) update_game();
            update_effects();
            anim_tick++;
            if (juice_counter) juice_counter--;
            update_led();
            if (!game_over && !level_complete) draw_frame();
            HAL_Delay(current_tick_ms());
        }

        led_state = 0; led_set(0);

        if (level_complete) {
            show_level_clear();
            level++;
            if (level > MAX_LEVEL) {
                victory = 1; game_over = 1;
                show_outro();
                show_game_over();
                if (wait_for_restart()) {
                    led_set(0); buzzer_off(&buzzer_cfg);
                    LCD_Fill_Buffer(C_BG); LCD_Refresh(&cfg0);
                    LCD_Set_Palette(PALETTE_DEFAULT);
                    return MENU_STATE_HOME;
                }
                lives = MAX_LIVES; score = 0; level = 1;
                player_hp = MAX_HP; bullet_count = 1;
                reset_skills_full();
            } else {
                if (player_hp < MAX_HP) player_hp++;
                // keep skill energy across waves — feels rewarding
            }
            show_level_intro();
            continue;
        }

        sfx(SFX_DEATH_FREQ, SFX_DEATH_MS);
        if (lives > 0) lives--;

        if (lives == 0) {
            show_outro();
            show_game_over();
            if (wait_for_restart()) {
                led_set(0); buzzer_off(&buzzer_cfg);
                LCD_Fill_Buffer(C_BG); LCD_Refresh(&cfg0);
                LCD_Set_Palette(PALETTE_DEFAULT);
                return MENU_STATE_HOME;
            }
            lives = MAX_LIVES; score = 0; level = 1;
            player_hp = MAX_HP; bullet_count = 1;
            reset_skills_full();
            show_level_intro();
        } else {
            player_hp = MAX_HP;
            // lose half skill energy on death — skill is precious
            for (uint8_t s = 0; s < SKILL_COUNT; s++) {
                skill_energy[s] /= 2;
                skill_ready_seen[s] = 0;
            }
            show_life_lost();
        }
    }
}