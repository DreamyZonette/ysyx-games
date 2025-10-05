#include <stdlib.h>
#include <stdio.h>
#include <am.h>
#include <amdev.h>
#include <klib-macros.h>

#define TILE_W 10
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define NEXT_PIECE_SIZE 4

// 方块形状定义 (I, O, T, L, J, S, Z)
static const int shapes[7][4][4][4] = {
    // I
    {
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}},
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}}
    },
    // O
    {
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}}
    },
    // T
    {
        {{0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}}
    },
    // L
    {
        {{0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0}},
        {{1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,0,0,0}, {1,0,0,0}, {0,0,0,0}},
        {{1,1,1,0}, {0,0,1,0}, {0,0,0,0}, {0,0,0,0}}
    },
    // J
    {
        {{1,0,0,0}, {1,0,0,0}, {1,1,0,0}, {0,0,0,0}},
        {{1,1,1,0}, {1,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}}
    },
    // S
    {
        {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    // Z
    {
        {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0}}
    }
};

// 方块颜色
static const uint32_t colors[8] = {
    0x00000000,   // 空白
    0x0000ffff,   // I - 青色
    0x00ffff00,   // O - 黄色
    0x008000ff,   // T - 紫色
    0x00ff8000,   // L - 橙色
    0x000000ff,   // J - 蓝色
    0x0000ff00,   // S - 绿色
    0x00ff0000    // Z - 红色
};

typedef struct {
    int x, y;
    int shape;
    int rotation;
} piece_t;

typedef struct {
    int board[BOARD_HEIGHT][BOARD_WIDTH];
    piece_t current;
    piece_t next;
    int score;
    int game_over;
    piece_t last_next;                    
} game_t;

static void refresh() {
    io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
}

static void draw_tile(int y, int x, uint32_t color) {
    static uint32_t buf[TILE_W * TILE_W];
    for (int i = 0; i < LENGTH(buf); i++) {
        buf[i] = color;
    }
    io_write(AM_GPU_FBDRAW, x * TILE_W, y * TILE_W, buf, TILE_W, TILE_W, false);
}

static int read_key() {
    AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
    return ev.keycode;
}

static void init_game(game_t *game) {
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            game->board[y][x] = 0;
        }
    }
    
    srand(io_read(AM_TIMER_UPTIME).us);
    game->score = 0;
    game->game_over = 0;
    game->current.shape = rand() % 7;
    game->current.rotation = 0;
    game->current.x = BOARD_WIDTH / 2 - 2;
    game->current.y = 0;
    
    game->next.shape = rand() % 7;
    game->next.rotation = 0;
    game->next.x = 0;
    game->next.y = 0;
    game->last_next.shape = -1;
}

static int check_collision(game_t *game, piece_t *piece) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (shapes[piece->shape][piece->rotation][y][x]) {
                int new_x = piece->x + x;
                int new_y = piece->y + y;
                if (new_x < 0 || new_x >= BOARD_WIDTH || 
                    new_y >= BOARD_HEIGHT || new_y < 0) {
                    return 1;
                }
                if (new_y >= 0 && game->board[new_y][new_x]) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void clear_next_piece(piece_t *piece, int offset_x, int offset_y) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (piece->shape != -1 && shapes[piece->shape][piece->rotation][y][x]) {
                int draw_x = offset_x + x;
                int draw_y = offset_y + y;
                draw_tile(draw_y, draw_x, colors[0]); 
            }
        }
    }
}

static void lock_piece(game_t *game) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (shapes[game->current.shape][game->current.rotation][y][x]) {
                int new_x = game->current.x + x;
                int new_y = game->current.y + y;
                
                if (new_y >= 0) {
                    game->board[new_y][new_x] = game->current.shape + 1;
                } else {
                    game->game_over = 1;
                    return;
                }
            }
        }
    }

    int lines_cleared = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (game->board[y][x] == 0) {
                full = 0;
                break;
            }
        }
        
        if (full) {
            lines_cleared++;
            for (int yy = y; yy > 0; yy--) {
                for (int x = 0; x < BOARD_WIDTH; x++) {
                    game->board[yy][x] = game->board[yy - 1][x];
                }
            }
            for (int x = 0; x < BOARD_WIDTH; x++) {
                game->board[0][x] = 0;
            }
            y++;
        }
    }
    

    if (lines_cleared == 1) game->score += 100;
    else if (lines_cleared == 2) game->score += 300;
    else if (lines_cleared == 3) game->score += 500;
    else if (lines_cleared >= 4) game->score += 800;
    game->last_next = game->next;
    
    game->current = game->next;
    game->current.x = BOARD_WIDTH / 2 - 2;
    game->current.y = 0;
    game->current.rotation = 0;
    
    game->next.shape = rand() % 7;
    game->next.rotation = 0;
    if (check_collision(game, &game->current)) {
        game->game_over = 1;
    }
}


static void draw_game(game_t *game, int offset_x, int offset_y) {
    uint32_t border_color = 0x00ffffff;
    for (int x = -1; x <= BOARD_WIDTH; x++) {
        draw_tile(offset_y - 1, offset_x + x, border_color);
        draw_tile(offset_y + BOARD_HEIGHT, offset_x + x, border_color);
    }
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        draw_tile(offset_y + y, offset_x - 1, border_color);
        draw_tile(offset_y + y, offset_x + BOARD_WIDTH, border_color);
    }
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int color_idx = game->board[y][x];
            draw_tile(offset_y + y, offset_x + x, colors[color_idx]);
        }
    }
    
    piece_t *p = &game->current;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (shapes[p->shape][p->rotation][y][x]) {
                int draw_x = offset_x + p->x + x;
                int draw_y = offset_y + p->y + y;
                if (p->y + y >= 0) {
                    draw_tile(draw_y, draw_x, colors[p->shape + 1]);
                }
            }
        }
    }
    
    int next_offset_x = offset_x + BOARD_WIDTH + 3;
    int next_offset_y = offset_y + 5;
    for (int x = -1; x <= NEXT_PIECE_SIZE; x++) {
        draw_tile(next_offset_y - 2, next_offset_x + x, border_color);
        draw_tile(next_offset_y + NEXT_PIECE_SIZE + 1, next_offset_x + x, border_color);
    }
    for (int y = -1; y <= NEXT_PIECE_SIZE + 1; y++) {
        draw_tile(next_offset_y + y, next_offset_x - 1, border_color);
        draw_tile(next_offset_y + y, next_offset_x + NEXT_PIECE_SIZE, border_color);
    }
    draw_tile(next_offset_y - 3, next_offset_x, 0x00ffff00);
    draw_tile(next_offset_y - 3, next_offset_x + 1, 0x00ffff00);
    draw_tile(next_offset_y - 3, next_offset_x + 2, 0x00ffff00);
    draw_tile(next_offset_y - 3, next_offset_x + 3, 0x00ffff00);
    
    clear_next_piece(&game->last_next, next_offset_x, next_offset_y);
    p = &game->next;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (shapes[p->shape][p->rotation][y][x]) {
                int draw_x = next_offset_x + x;
                int draw_y = next_offset_y + y;
                draw_tile(draw_y, draw_x, colors[p->shape + 1]);
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        draw_tile(next_offset_y + NEXT_PIECE_SIZE + 5, next_offset_x + i, colors[0]);
    }
    
    int score_offset_y = next_offset_y + NEXT_PIECE_SIZE + 3;
    draw_tile(score_offset_y, next_offset_x, 0x00ff0000);
    draw_tile(score_offset_y, next_offset_x + 1, 0x00ff0000);
    draw_tile(score_offset_y, next_offset_x + 2, 0x00ff0000);
    draw_tile(score_offset_y, next_offset_x + 3, 0x00ff0000);
    
    int score = game->score;
    for (int i = 0; i < 4; i++) {
        int digit = score % 10;
        score /= 10;
        if (digit > 0) {
            draw_tile(score_offset_y + 2, next_offset_x + 3 - i, 0x0000ffff);
        }
    }
}

int main() {
    game_t game;
    int key;
    int fall_delay = 1000000;
    uint64_t last_fall = 0;
    
    ioe_init();
    const int screen_width = 400;
    const int screen_height = 300;
    
    const int max_tiles_x = screen_width / TILE_W; 
    const int max_tiles_y = screen_height / TILE_W;
    
    int offset_x = (max_tiles_x - BOARD_WIDTH - 8) / 2;  
    int offset_y = (max_tiles_y - BOARD_HEIGHT) / 2;     
    
    init_game(&game);
    
    while (!game.game_over) {
        draw_game(&game, offset_x, offset_y);
        refresh();
        
        key = read_key();
        piece_t temp = game.current;
        
        switch (key) {
            case AM_KEY_LEFT:
                temp.x--;
                if (!check_collision(&game, &temp)) {
                    game.current.x--;
                }
                break;
            case AM_KEY_RIGHT:
                temp.x++;
                if (!check_collision(&game, &temp)) {
                    game.current.x++;
                }
                break;
            case AM_KEY_DOWN:
                temp.y++;
                if (!check_collision(&game, &temp)) {
                    game.current.y++;
                    last_fall = io_read(AM_TIMER_UPTIME).us;
                } else {
                    lock_piece(&game);
                }
                break;
            case AM_KEY_UP:
                temp.rotation = (temp.rotation + 1) % 4;
                if (!check_collision(&game, &temp)) {
                    game.current.rotation = temp.rotation;
                }
                break;
            case AM_KEY_Q:
                game.game_over = 1;
                break;
        }
        
        uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
        if (current_time - last_fall > fall_delay) {
            temp = game.current;
            temp.y++;
            if (!check_collision(&game, &temp)) {
                game.current.y++;
            } else {
                lock_piece(&game);
            }
            last_fall = current_time;
        }
        
        fall_delay = 1000000 - (game.score / 1000) * 100000;
        if (fall_delay < 100000) fall_delay = 100000;
    }
    printf("GAME OVER! Score: %d\nPress Q to Exit\n", game.score);
    while (read_key() != AM_KEY_Q);
    
    return 0;
}
