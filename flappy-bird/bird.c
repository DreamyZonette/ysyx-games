#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <am.h>
#include <amdev.h>
#include <klib-macros.h>
#include <time.h>

// 为了避免浮点数运算，使用整数缩放因子
#define SCALE 100  // 缩放比例，相当于小数点后两位

// 游戏常量定义
#define FPS 90             // 帧率
#define FRAME_DELAY (1000000 / FPS)  // 每帧延迟(微秒)

// 游戏元素尺寸（比例定义，使用整数计算）
#define BIRD_SIZE_RATIO 7    // 小鸟大小占屏幕高度比例 * 100
#define PIPE_WIDTH_RATIO 12  // 管道宽度占屏幕宽度比例 * 100
#define PIPE_GAP_RATIO 35    // 管道间隙占屏幕高度比例 * 100
#define PIPE_SPEED_RATIO 5   // 管道速度占屏幕宽度比例 * 1000
#define GRAVITY 50           // 重力加速度 * SCALE (原0.5)
#define JUMP_FORCE -800      // 跳跃力度 * SCALE 
#define GROUND_HEIGHT_RATIO 5 // 地面高度占屏幕高度比例 * 100
#define COIN_SIZE_RATIO 3    // 金币大小占屏幕高度比例 * 100

// 颜色定义
#define COLOR_BG      0x0087CEEB  // 背景天空蓝
#define COLOR_BIRD    0x00FFD700  // 小鸟金色
#define COLOR_PIPE    0x00228B22  // 管道深绿
#define COLOR_GROUND  0x00CD853F  // 地面秘鲁棕
#define COLOR_TEXT    0x00FFFFFF  // 文字白色
#define COLOR_RED     0x00FF0000  // 红色（游戏结束）
#define COLOR_PIPE_TOP 0x00228B22 // 管道顶部
#define COLOR_COIN    0x00FFD700  // 金币金色
#define COLOR_COIN_BORDER 0x00FFA500 // 金币边框橙色
#define COLOR_SCORE   0x00FFD700  // 分数黄色

// 游戏状态枚举
typedef enum {
    GAME_RUNNING,
    GAME_OVER
} GameState;

// 管道结构体
typedef struct {
    int x;          // 管道x坐标
    int gap_y;      // 管道间隙y坐标（中心）
    bool passed;    // 是否已穿过（用于计分）
    bool active;    // 管道是否处于活动状态
} Pipe;

// 金币结构体
typedef struct {
    int x;          // 金币x坐标
    int y;          // 金币y坐标
    int size;       // 金币大小
    bool collected; // 是否已收集
    bool active;    // 是否活动
} Coin;

// 游戏全局状态
static struct {
    // 屏幕尺寸（动态获取）
    int screen_width;
    int screen_height;
    // 元素实际尺寸（根据屏幕计算）
    int bird_size;
    int pipe_width;
    int pipe_gap;
    int pipe_speed;
    int ground_height;
    int coin_size;  // 金币大小
    // 小鸟状态（使用整数模拟浮点运算）
    int bird_y;     // 小鸟y坐标 * SCALE
    int velocity;   // 小鸟竖直速度 * SCALE
    // 管道状态
    Pipe pipes[5];  // 最多同时存在5个管道
    int active_pipes; // 当前活动管道数量
    // 金币状态
    Coin coins[10]; // 最多同时存在10个金币
    int active_coins; // 当前活动金币数量
    // 游戏信息
    int score;      // 分数
    int coin_score; // 金币额外分数
    GameState state;// 游戏状态
    // 地面滚动
    int ground_offset;
    // 缓冲区
    uint32_t *frame_buf; // 全屏缓冲区
} game;

// 预分配字符缓冲区（更大的字符尺寸，16x16像素，使字体更圆滑）
static uint32_t *char_buf = NULL;   // 字符绘制缓冲区

// 微秒级延迟函数
static void delay_us(unsigned int us) {
    uint64_t start = io_read(AM_TIMER_UPTIME).us;
    while (io_read(AM_TIMER_UPTIME).us - start < us) {
        // 空循环等待
    }
}

// 整数乘法（带缩放）
static int mul_scale(int a, int b, int scale) {
    return (a * b) / scale;
}

// 初始化缓冲区
static void init_buffers() {
    // 初始化全屏缓冲区
    game.frame_buf = (uint32_t*)malloc(game.screen_width * game.screen_height * sizeof(uint32_t));
    // 字符缓冲区（16x16像素，更大尺寸使字体更圆滑）
    char_buf = (uint32_t*)malloc(16 * 16 * sizeof(uint32_t));
}

// 释放缓冲区
static void free_buffers() {
    if (game.frame_buf) free(game.frame_buf);
    if (char_buf) free(char_buf);
}

// 初始化游戏状态
static void init_game() {
    // 获取屏幕尺寸
    AM_GPU_CONFIG_T gpu_cfg = io_read(AM_GPU_CONFIG);
    game.screen_width = gpu_cfg.width;
    game.screen_height = gpu_cfg.height;
    
    // 根据屏幕尺寸计算元素实际大小（使用整数运算）
    game.bird_size = mul_scale(game.screen_height, BIRD_SIZE_RATIO, 100);
    game.pipe_width = mul_scale(game.screen_width, PIPE_WIDTH_RATIO, 100);
    game.pipe_gap = mul_scale(game.screen_height, PIPE_GAP_RATIO, 100);
    game.pipe_speed = mul_scale(game.screen_width, PIPE_SPEED_RATIO, 1000);
    game.ground_height = mul_scale(game.screen_height, GROUND_HEIGHT_RATIO, 100);
    game.coin_size = mul_scale(game.screen_height, COIN_SIZE_RATIO, 100);
    
    // 确保最小尺寸
    if (game.pipe_speed < 1) game.pipe_speed = 1;
    if (game.bird_size < 8) game.bird_size = 8;
    if (game.pipe_width < 20) game.pipe_width = 20;
    if (game.coin_size < 6) game.coin_size = 6;  // 金币最小尺寸
    
    // 初始化小鸟位置（屏幕左侧中间）
    game.bird_y = game.screen_height * SCALE / 2;  // 中间位置
    game.velocity = 0;

    // 初始化管道
    memset(game.pipes, 0, sizeof(game.pipes));
    for (int i = 0; i < 5; i++) {
        game.pipes[i].x = -game.pipe_width;  // 初始位置在左侧屏幕外
        game.pipes[i].passed = false;
        game.pipes[i].active = false;
    }
    game.active_pipes = 0;

    // 初始化金币
    memset(game.coins, 0, sizeof(game.coins));
    for (int i = 0; i < 10; i++) {
        game.coins[i].x = -game.coin_size;  // 初始位置在左侧屏幕外
        game.coins[i].collected = false;
        game.coins[i].active = false;
    }
    game.active_coins = 0;

    // 初始化分数
    game.score = 0;
    game.coin_score = 0;

    // 初始化游戏状态
    game.state = GAME_RUNNING;
    game.ground_offset = 0;

    // 初始化缓冲区
    init_buffers();

    // 初始化随机数种子
    srand((unsigned int)io_read(AM_TIMER_UPTIME).us);
}

// 向缓冲区绘制像素
static void frame_buf_set_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < game.screen_width && y >= 0 && y < game.screen_height) {
        game.frame_buf[y * game.screen_width + x] = color;
    }
}

// 绘制实心矩形到缓冲区
static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    // 检查绘制区域是否在屏幕内
    if (x >= game.screen_width || y >= game.screen_height ||
        x + w <= 0 || y + h <= 0) return;

    // 调整绘制区域（裁剪超出屏幕的部分）
    int draw_x = x < 0 ? 0 : x;
    int draw_y = y < 0 ? 0 : y;
    int draw_w = (x + w > game.screen_width) ? (game.screen_width - x) : w;
    int draw_h = (y + h > game.screen_height) ? (game.screen_height - y) : h;
    if (draw_w <= 0 || draw_h <= 0) return;

    // 填充到缓冲区
    for (int i = 0; i < draw_h; i++) {
        int row = (draw_y + i) * game.screen_width + draw_x;
        for (int j = 0; j < draw_w; j++) {
            game.frame_buf[row + j] = color;
        }
    }
}

// 绘制小鸟（圆形）
static void draw_bird() {
    int bird_x = game.screen_width / 4;  // 小鸟固定x坐标
    int bird_y = game.bird_y / SCALE;    // 转换为实际坐标
    
    // 绘制圆形小鸟
    for (int dy = -game.bird_size/2; dy < game.bird_size/2; dy++) {
        for (int dx = -game.bird_size/2; dx < game.bird_size/2; dx++) {
            // 圆形公式：x² + y² <= r²
            if (dx*dx + dy*dy <= (game.bird_size/2)*(game.bird_size/2)) {
                int x = bird_x + dx;
                int y = bird_y + dy;
                frame_buf_set_pixel(x, y, COLOR_BIRD);
            }
        }
    }
}

// 生成新管道（右侧）
static void spawn_pipe() {
    if (game.active_pipes >= 5) return;  // 达到最大管道数

    // 找到空管道位置
    for (int i = 0; i < 5; i++) {
        if (!game.pipes[i].active) {  // 只使用非活动管道
            // 生成管道位置（右侧屏幕外）
            game.pipes[i].x = game.screen_width;
            // 随机生成间隙位置
            int min_gap = game.ground_height + game.pipe_gap/2 + 20;
            int max_gap = game.screen_height - game.ground_height - game.pipe_gap/2 - 20;
            game.pipes[i].gap_y = min_gap + (rand() % (max_gap - min_gap + 1));
            game.pipes[i].passed = false;
            game.pipes[i].active = true;  // 标记为活动
            game.active_pipes++;
            
            // 有30%概率在管道间隙生成金币
            if (rand() % 10 < 3) {
                // 在管道间隙附近随机位置生成金币
                int coin_x = game.screen_width + game.pipe_width/2;
                int coin_y = game.pipes[i].gap_y - game.pipe_gap/4 + 
                            (rand() % (game.pipe_gap/2));
                // 尝试生成金币
                for (int c = 0; c < 10; c++) {
                    if (!game.coins[c].active) {
                        game.coins[c].x = coin_x;
                        game.coins[c].y = coin_y;
                        game.coins[c].size = game.coin_size;
                        game.coins[c].collected = false;
                        game.coins[c].active = true;
                        game.active_coins++;
                        break;
                    }
                }
            }
            break;
        }
    }
}

// 绘制管道
static void draw_pipes() {
    for (int i = 0; i < 5; i++) {
        Pipe *p = &game.pipes[i];
        // 只绘制活动且在屏幕范围内的管道
        if (!p->active) continue;
        if (p->x > game.screen_width) continue;    // 完全在右侧不绘制
        if (p->x + game.pipe_width < 0) continue;  // 完全在左侧不绘制

        // 绘制上管道
        draw_rect(p->x, 0, game.pipe_width, p->gap_y - game.pipe_gap/2, COLOR_PIPE);
        // 绘制上管道顶部
        draw_rect(p->x - 2, p->gap_y - game.pipe_gap/2, game.pipe_width + 4, 6, COLOR_PIPE_TOP);
        
        // 绘制下管道
        int lower_pipe_height = game.screen_height - (p->gap_y + game.pipe_gap/2) - game.ground_height;
        draw_rect(p->x, p->gap_y + game.pipe_gap/2, game.pipe_width, lower_pipe_height, COLOR_PIPE);
        // 绘制下管道顶部
        draw_rect(p->x - 2, p->gap_y + game.pipe_gap/2 - 6, game.pipe_width + 4, 6, COLOR_PIPE_TOP);
    }
}

// 绘制金币（带边框的圆形）
static void draw_coins() {
    for (int i = 0; i < 10; i++) {
        Coin *c = &game.coins[i];
        if (!c->active || c->collected) continue;  // 只绘制活动且未收集的金币
        if (c->x > game.screen_width) continue;    // 完全在右侧不绘制
        if (c->x + c->size < 0) continue;          // 完全在左侧不绘制

        // 绘制金币边框（橙色）
        for (int dy = -c->size/2; dy <= c->size/2; dy++) {
            for (int dx = -c->size/2; dx <= c->size/2; dx++) {
                int dist_sq = dx*dx + dy*dy;
                // 边框范围：(r-1)^2 <= x²+y² <= r²
                if (dist_sq <= (c->size/2)*(c->size/2) && 
                    dist_sq >= ((c->size/2 - 2)*(c->size/2 - 2))) {
                    int x = c->x + dx;
                    int y = c->y + dy;
                    frame_buf_set_pixel(x, y, COLOR_COIN_BORDER);
                }
            }
        }

        // 绘制金币内部（金色）
        for (int dy = -(c->size/2 - 2); dy <= (c->size/2 - 2); dy++) {
            for (int dx = -(c->size/2 - 2); dx <= (c->size/2 - 2); dx++) {
                if (dx*dx + dy*dy <= ((c->size/2 - 2)*(c->size/2 - 2))) {
                    int x = c->x + dx;
                    int y = c->y + dy;
                    frame_buf_set_pixel(x, y, COLOR_COIN);
                }
            }
        }
    }
}

static void update_pipes_and_coins() {
    for (int i = 0; i < 5; i++) {
        Pipe *p = &game.pipes[i];
        if (!p->active) continue;

        p->x -= game.pipe_speed;

        if (p->x + game.pipe_width < 0) {
            p->active = false;
            game.active_pipes--;
        }
    }

    for (int i = 0; i < 10; i++) {
        Coin *c = &game.coins[i];
        if (!c->active || c->collected) continue;

        c->x -= game.pipe_speed;

        if (c->x + c->size < 0) {
            c->active = false;
            game.active_coins--;
        }
    }

    static int spawn_timer = 0;
    int spawn_interval = (game.screen_width / game.pipe_speed) / 2;
    if (spawn_interval < 60) spawn_interval = 60;

    if (++spawn_timer >= spawn_interval) {
        spawn_pipe();
        spawn_timer = 0;
    }
}


// 绘制地面
static void draw_ground() {
    // 绘制地面矩形
    draw_rect(0, game.screen_height - game.ground_height, 
              game.screen_width, game.ground_height, COLOR_GROUND);
    
    // 绘制地面纹理
    for (int i = 0; i < game.screen_width; i += game.screen_width / 20) {
        int x = (i + game.ground_offset) % game.screen_width;
        draw_rect(x, game.screen_height - game.ground_height + 5, 
                  game.screen_width / 40, 2, COLOR_TEXT);
    }
    
    // 更新地面滚动偏移
    game.ground_offset = (game.ground_offset - game.pipe_speed + game.screen_width) % game.screen_width;
}

static void draw_char(int x, int y, char c, uint32_t color) {
    //const int size = 16;
    static const uint8_t font[11][16] = {
        // 0
        {0b00111100, 0b01111110, 0b11000011, 0b11000011,
         0b11000011, 0b11000011, 0b01111110, 0b00111100},
        // 1
        {0b00011000, 0b00111000, 0b01111000, 0b00011000,
         0b00011000, 0b00011000, 0b00111100, 0b00111100},
        // 2
        {0b01111110, 0b11000110, 0b00000110, 0b00001100,
         0b00011000, 0b00110000, 0b11111111, 0b11111111},
        // 3
        {0b01111110, 0b11000110, 0b00000110, 0b00111100,
         0b00000110, 0b11000110, 0b01111110, 0b00000000},
        // 4
        {0b00001100, 0b00011100, 0b00101100, 0b01001100,
         0b11111111, 0b11111111, 0b00001100, 0b00000000},
        // 5
        {0b11111111, 0b11111111, 0b11000000, 0b11111100,
         0b00000110, 0b00000110, 0b11111110, 0b11000000},
        // 6
        {0b00111100, 0b01111110, 0b11000000, 0b11111100,
         0b11000110, 0b11000110, 0b01111110, 0b00000000},
        // 7
        {0b11111111, 0b11111111, 0b00000110, 0b00001100,
         0b00011000, 0b00110000, 0b00110000, 0b00110000},
        // 8
        {0b01111110, 0b11000110, 0b01111110, 0b11000110,
         0b11000110, 0b11000110, 0b01111110, 0b00000000},
        // 9
        {0b01111110, 0b11000110, 0b11000110, 0b01111110,
         0b00000110, 0b00000110, 0b01111110, 0b00000000},
        // + (10)
        {0b00000000, 0b00011000, 0b00011000, 0b11111111,
         0b11111111, 0b00011000, 0b00011000, 0b00000000}
    };

    int index = -1;
    if (c >= '0' && c <= '9') index = c - '0';
    else if (c == '+') index = 10;

    if (index < 0 || index > 10) return;

    for (int row = 0; row < 8; row++) {
        uint8_t line = font[index][row];
        for (int col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                int px = x + col * 2;
                int py = y + row * 2;
                draw_rect(px, py, 2, 2, color);
            }
        }
    }
}


// 绘制分数（优化后更圆滑的字体）
static void draw_score() {
    // 主分数
    char score_str[10];
    snprintf(score_str, sizeof(score_str), "%d", game.score + game.coin_score);
    int len = strlen(score_str);
    int char_size = 16;  // 匹配增大的字符尺寸

    // 绘制在屏幕顶部中央
    int start_x = game.screen_width/2 - (len * char_size) / 2;
    for (int i = 0; i < len; i++) {
        draw_char(start_x + i * char_size, 10, score_str[i], COLOR_SCORE);
    }

    // 如果有金币分数，显示金币图标和额外分数
    if (game.coin_score > 0) {
        char coin_str[5];
        snprintf(coin_str, sizeof(coin_str), "+%d", game.coin_score);
        int coin_len = strlen(coin_str);
        
        // 绘制金币小图标
        int coin_icon_x = start_x - char_size;
        int coin_icon_y = 10;
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                if (dx*dx + dy*dy <= 16) {  // 4x4的小圆
                    frame_buf_set_pixel(coin_icon_x + 8 + dx, coin_icon_y + 8 + dy, COLOR_COIN);
                }
            }
        }
        
        // 绘制额外分数
        for (int i = 0; i < coin_len; i++) {
            draw_char(coin_icon_x - coin_len * char_size + i * char_size, 10, coin_str[i], COLOR_COIN);
        }
    }
}

// 检测金币碰撞（小鸟是否吃到金币）
static void check_coin_collision() {
    int bird_x = game.screen_width / 4;
    int bird_y = game.bird_y / SCALE;  // 转换为实际坐标
    
    for (int i = 0; i < 10; i++) {
        Coin *c = &game.coins[i];
        if (!c->active || c->collected) continue;
        
        // 计算小鸟和金币的距离
        int dx = c->x - bird_x;
        int dy = c->y - bird_y;
        int min_dist = (game.bird_size + c->size) / 3;  // 碰撞距离阈值
        
        if (dx*dx + dy*dy <= min_dist*min_dist) {
            // 吃到金币，加分并标记为已收集
            c->collected = true;
            game.coin_score += 1;  // 每个金币加1分
        }
    }
}

static void draw_game_over() {
    // 半透明遮罩
    for (int y = 0; y < game.screen_height; y++) {
        for (int x = 0; x < game.screen_width; x++) {
            uint32_t original = game.frame_buf[y * game.screen_width + x];
            uint32_t r = (original >> 16) & 0xFF;
            uint32_t g = (original >> 8) & 0xFF;
            uint32_t b = original & 0xFF;
            r = (r * 3) / 10;
            g = (g * 3) / 10;
            b = (b * 3) / 10;
            frame_buf_set_pixel(x, y, (0xFF << 24) | (r << 16) | (g << 8) | b);
        }
    }

    int char_width = 16; // 每个字符宽度（放大后）

    // 绘制“GAME OVER”
    char *msg = "GAME OVER";
    int msg_len = strlen(msg);
    int msg_x = (game.screen_width - msg_len * char_width) / 2;
    int msg_y = game.screen_height / 2 - 30;
    for (int i = 0; i < msg_len; i++) {
        draw_char(msg_x + i * char_width, msg_y, msg[i], COLOR_RED);
    }

    // 绘制“SCORE: X”
    char score_str[20];
    snprintf(score_str, sizeof(score_str), "SCORE: %d", game.score + game.coin_score);
    int score_len = strlen(score_str);
    int score_x = (game.screen_width - score_len * char_width) / 2;
    int score_y = game.screen_height / 2 + 20;
    for (int i = 0; i < score_len; i++) {
        draw_char(score_x + i * char_width, score_y, score_str[i], COLOR_SCORE);
    }

    // 绘制“PRESS R”
    char *hint = "PRESS R";
    int hint_len = strlen(hint);
    int hint_x = (game.screen_width - hint_len * char_width) / 2;
    int hint_y = game.screen_height / 2 + 60;
    for (int i = 0; i < hint_len; i++) {
        draw_char(hint_x + i * char_width, hint_y, hint[i], COLOR_TEXT);
    }
}


// 检测碰撞
static bool check_collision() {
    int bird_x = game.screen_width / 4;
    int bird_y = game.bird_y / SCALE;  // 转换为实际坐标
    
    // 检测地面碰撞
    if (bird_y + game.bird_size/2 >= game.screen_height - game.ground_height) return true;
    // 检测顶部碰撞
    if (bird_y - game.bird_size/2 <= 0) return true;

    // 检测管道碰撞
    for (int i = 0; i < 5; i++) {
        Pipe *p = &game.pipes[i];
        if (!p->active) continue;
        
        // 管道与小鸟x范围重叠
        if (p->x <= bird_x + game.bird_size/2 && p->x + game.pipe_width >= bird_x - game.bird_size/2) {
            // 检测上管道碰撞
            if (bird_y - game.bird_size/2 <= p->gap_y - game.pipe_gap/2) return true;
            // 检测下管道碰撞
            if (bird_y + game.bird_size/2 >= p->gap_y + game.pipe_gap/2) return true;
        }
    }
    return false;
}

// 更新分数
static void update_score() {
    int bird_x = game.screen_width / 4;
    for (int i = 0; i < 5; i++) {
        Pipe *p = &game.pipes[i];
        // 只对活动且未计分的管道进行判断
        if (!p->active || p->passed) continue;
        
        // 小鸟完全穿过管道
        if (p->x + game.pipe_width < bird_x - game.bird_size/2) {
            game.score++;
            p->passed = true;
        }
    }
}

// 处理用户输入
static void handle_input() {
    AM_INPUT_KEYBRD_T key = io_read(AM_INPUT_KEYBRD);
    if (!key.keydown) return;

    if (game.state == GAME_RUNNING) {
        // 空格或上方向键跳跃
        if (key.keycode == AM_KEY_SPACE || key.keycode == AM_KEY_UP) {
            game.velocity = JUMP_FORCE;
        }
    } else {
        // 游戏结束时按R重启
        if (key.keycode == AM_KEY_R) {
            free_buffers();
            init_game();
        }
    }
}

// 更新游戏状态
static void update_game() {
    if (game.state != GAME_RUNNING) return;

    // 更新小鸟位置（使用整数运算模拟浮点）
    game.velocity += GRAVITY;
    game.bird_y += game.velocity;

    // 更新管道和金币
    update_pipes_and_coins();

    // 更新分数
    update_score();

    // 检测金币碰撞
    check_coin_collision();

    // 检测管道/边界碰撞
    if (check_collision()) {
        game.state = GAME_OVER;
    }
}

// 绘制游戏画面
static void draw_game() {
    // 清空缓冲区（绘制背景）
    draw_rect(0, 0, game.screen_width, game.screen_height, COLOR_BG);

    // 绘制游戏元素（按层次绘制）
    draw_pipes();
    draw_coins();
    draw_ground();
    draw_bird();
    draw_score();

    // 游戏结束时绘制遮罩
    if (game.state == GAME_OVER) {
        draw_game_over();
    }

    // 一次性刷新到屏幕
    io_write(AM_GPU_FBDRAW, 0, 0, game.frame_buf, game.screen_width, game.screen_height, true);
}

// 主游戏循环
static void game_loop() {
    while (1) {
        handle_input();
        update_game();
        draw_game();
        delay_us(FRAME_DELAY);
    }
}

int main() {
    ioe_init();
    init_game();

    printf("Flappy Bird\n");
    printf("按空格键或上方向键跳跃\n");
    printf("避开管道，收集金币加分\n");
    printf("游戏结束后按R键重启\n");

    game_loop();
    free_buffers();
    return 0;
}
    