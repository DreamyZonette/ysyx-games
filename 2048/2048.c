#include <am.h>
#include <klib.h>
#include <klib-macros.h>

#define BLOCK_WIDTH   4 // 方块x轴个数
#define BLOCK_HEIGHT  4 // 方块y轴个数
#define BLOCK_SIZE   50 // 方块大小 (边长)
#define BLOCK_MARGIN 10 // 边框
#define BUFFER_SIZE   8 // 输入缓冲区大小
#define FPS 30
#define CPS 5

// 颜色定义
#define COL_BG 0xbbada0
#define COL_TEXT 0x776e65
#define COL_EMPTY 0xccc0b3
#define COL_GRID_LINE 0xbbada0

// 数字块颜色 (0=空白, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048)
static uint32_t block_colors[] = {
    COL_EMPTY,      // 0
    0xeee4da,      // 2
    0xede0c8,      // 4
    0xf2b179,      // 8
    0xf59563,      // 16
    0xf67c5f,      // 32
    0xf65e3b,      // 64
    0xedcf72,      // 128
    0xedcc61,      // 256
    0xedc850,      // 512
    0xedc53f,      // 1024
    0xedc22e       // 2048
};

static int blocks[BLOCK_WIDTH][BLOCK_HEIGHT] = {0};
static int screen_w, screen_h;
static int score = 0;

typedef struct key_info {
    int key_code;
    int valid;
} key_info_t;

static key_info_t key_info[BUFFER_SIZE];
static int read_index = 0, write_index = 0;
static int free_blocks = BLOCK_HEIGHT * BLOCK_WIDTH;
static int up = 0, down = 0, left = 0, right = 0;

// 字体绘制
void draw_char(int x, int y, char ch, uint32_t color) {
    extern uint8_t font[];
    static uint32_t char_buf[8 * 16];
    int base; 
    if (ch >= '0' && ch <= '9'){
      base = (uint8_t)(ch - '0') * 16 * 8;
    }
    else if (ch == 'S'){
      base = 10 * 16 * 8; 
    }
    else if (ch == 'c'){
      base = 11 * 16 * 8; 
    }
    else if (ch == 'o'){
      base = 12 * 16 * 8; 
    }
    else if (ch == 'r'){
      base = 13 * 16 * 8; 
    }
    else if (ch == 'e'){
      base = 14 * 16 * 8; 
    }
    else if (ch == ':'){
      base = 15 * 16 * 8; 
    }
    else {
      base = 0; // 默认是0
    }
    
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            char_buf[i * 8 + j] = (font[base + j + i * 8] == 0 )? color : COL_TEXT;
        }
    }
    
    io_write(AM_GPU_FBDRAW, x, y, char_buf, 8, 16, false);
}

// 绘制数字 (居中显示)
void draw_number(int x, int y, int num, int block_w, int block_h) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", num);
    int len = strlen(buf);
    
    int char_width = 8 * len;
    int char_height = 16;
    
    int start_x = x + (block_w - char_width) / 2;
    int start_y = y + (block_h - char_height) / 2;
    
    // 获取文字颜色 
    int color_idx;// = (num <= 4) ? num : 3;
    switch (num) {
        case 2:    color_idx =  1; break;
        case 4:    color_idx =  2; break;
        case 8:    color_idx =  3; break;
        case 16:   color_idx =  4; break;
        case 32:   color_idx =  5; break;
        case 64:   color_idx =  6; break;
        case 128:  color_idx =  7; break;
        case 256:  color_idx =  8; break;
        case 512:  color_idx =  9; break;
        case 1024: color_idx = 10; break;
        case 2048: color_idx = 11; break;
        default:   color_idx =  0; break;
    }
    uint32_t text_color = block_colors[color_idx];
    
    // 绘制每个字符
    for (int i = 0; i < len; i++) {
        draw_char(start_x + i * 8, start_y, buf[i], text_color);
    }
}

// 绘制一个纯色的方块
void draw_block(int x, int y, int w, int h, uint32_t color) {
    static uint32_t *buf = NULL;
    static int buf_size = 0;
    
    // 第一次使用时初始化缓冲区
    if (!buf || buf_size < w * h) {
        if (buf) free(buf);
        buf = malloc(w * h * sizeof(uint32_t));
        buf_size = w * h;
        panic_on(!buf, "Memory allocation failed for block buffer");
    }
    
    // 填充缓冲区为指定颜色
    for (int i = 0; i < w * h; i++) {
        buf[i] = color;
    }
    
    // 绘制方块
    io_write(AM_GPU_FBDRAW, x, y, buf, w, h, false);
}

// 初始化游戏
void game_init() {
    free_blocks = BLOCK_WIDTH * BLOCK_HEIGHT;
    score = 0;
    memset(blocks, 0, sizeof(blocks));
    
    // 添加两个初始方块
    blocks[rand() % BLOCK_WIDTH][rand() % BLOCK_HEIGHT] = 2;
    free_blocks--;
    int x, y;
    do {
        x = rand() % BLOCK_WIDTH;
        y = rand() % BLOCK_HEIGHT;
    } while (blocks[x][y] != 0);
    blocks[x][y] = 2;
    free_blocks--;
}

void block_update() {
    for(int i = 0; i < BUFFER_SIZE; i++) {
        if(key_info[read_index].valid != 1) continue;
        
        int key_code = key_info[read_index].key_code;
        switch (key_code) {
        case AM_KEY_UP:
            up = 1;
            break;
        case AM_KEY_DOWN:
            down = 1;
            break;
        case AM_KEY_LEFT:
            left = 1;
            break;
        case AM_KEY_RIGHT:
            right = 1;
            break;
        default:
            break;
        }
        
        key_info[read_index].valid = 0;
        read_index = (read_index + 1) % BUFFER_SIZE;
        free_blocks++;
        
        if(up || down || left || right) return;
    }
}

// 上移操作
void up_update() {
    for (int col = 0; col < BLOCK_WIDTH; col++) {
        for (int row = 1; row < BLOCK_HEIGHT; row++) {
            if (blocks[col][row] == 0) continue;
            
            int new_row = row;
            while (new_row > 0 && blocks[col][new_row - 1] == 0) {
                blocks[col][new_row - 1] = blocks[col][new_row];
                blocks[col][new_row] = 0;
                new_row--;
            }
            
            if (new_row > 0 && blocks[col][new_row - 1] == blocks[col][new_row]) {
                blocks[col][new_row - 1] *= 2;
                score += blocks[col][new_row - 1];
                blocks[col][new_row] = 0;
                free_blocks++;
            }
        }
    }
}

// 下移操作
void down_update() {
    for (int col = 0; col < BLOCK_WIDTH; col++) {
        for (int row = BLOCK_HEIGHT - 2; row >= 0; row--) {
            if (blocks[col][row] == 0) continue;
            
            int new_row = row;
            while (new_row < BLOCK_HEIGHT - 1 && blocks[col][new_row + 1] == 0) {
                blocks[col][new_row + 1] = blocks[col][new_row];
                blocks[col][new_row] = 0;
                new_row++;
            }
            
            if (new_row < BLOCK_HEIGHT - 1 && blocks[col][new_row + 1] == blocks[col][new_row]) {
                blocks[col][new_row + 1] *= 2;
                score += blocks[col][new_row + 1];
                blocks[col][new_row] = 0;
                free_blocks++;
            }
        }
    }
}

// 左移操作
void left_update() {
    for (int row = 0; row < BLOCK_HEIGHT; row++) {
        for (int col = 1; col < BLOCK_WIDTH; col++) {
            if (blocks[col][row] == 0) continue;
            
            int new_col = col;
            while (new_col > 0 && blocks[new_col - 1][row] == 0) {
                blocks[new_col - 1][row] = blocks[new_col][row];
                blocks[new_col][row] = 0;
                new_col--;
            }
            
            if (new_col > 0 && blocks[new_col - 1][row] == blocks[new_col][row]) {
                blocks[new_col - 1][row] *= 2;
                score += blocks[new_col - 1][row];
                blocks[new_col][row] = 0;
                free_blocks++;
            }
        }
    }
}

// 右移操作
void right_update() {
    for (int row = 0; row < BLOCK_HEIGHT; row++) {
        for (int col = BLOCK_WIDTH - 2; col >= 0; col--) {
            if (blocks[col][row] == 0) continue;
            
            int new_col = col;
            while (new_col < BLOCK_WIDTH - 1 && blocks[new_col + 1][row] == 0) {
                blocks[new_col + 1][row] = blocks[new_col][row];
                blocks[new_col][row] = 0;
                new_col++;
            }
            
            if (new_col < BLOCK_WIDTH - 1 && blocks[new_col + 1][row] == blocks[new_col][row]) {
                blocks[new_col + 1][row] *= 2;
                score += blocks[new_col + 1][row];
                blocks[new_col][row] = 0;
                free_blocks++;
            }
        }
    }
}

void new_block() {
    if (free_blocks == 0) return;
    
    int rand_pos = rand() % free_blocks;
    int counter = 0;
    int new_value = (rand() % 10 == 0) ? 4 : 2; // 10%几率生成4
    
    for (int i = 0; i < BLOCK_WIDTH; i++) {
        for (int j = 0; j < BLOCK_HEIGHT; j++) {
            if (blocks[i][j] == 0) {
                if (counter == rand_pos) {
                    blocks[i][j] = new_value;
                    free_blocks--;
                    return;
                }
                counter++;
            }
        }
    }
}

void game_logic_update(int frame) {
    if (frame % (FPS / CPS) == 0) block_update();
    
    if (up) {
        up_update();
        new_block();
        up = 0;
    }
    if (down) {
        down_update();
        new_block();
        down = 0;
    }
    if (left) {
        left_update();
        new_block();
        left = 0;
    }
    if (right) {
        right_update();
        new_block();
        right = 0;
    }
}

void render() {
    // 计算游戏区域的总宽度和高度
    int grid_width = BLOCK_WIDTH * (BLOCK_SIZE + BLOCK_MARGIN) + BLOCK_MARGIN;
    int grid_height = BLOCK_HEIGHT * (BLOCK_SIZE + BLOCK_MARGIN) + BLOCK_MARGIN;
    
    // 计算左上角位置居中显示
    int start_x = (screen_w - grid_width) / 2;
    int start_y = (screen_h - grid_height) / 3;
    
    // 绘制背景
    draw_block(0, 0, screen_w, screen_h, COL_BG);
    
    // 绘制网格
    draw_block(start_x, start_y, grid_width, grid_height, COL_GRID_LINE);
    
    // 绘制方块
    for (int i = 0; i < BLOCK_WIDTH; i++) {
        for (int j = 0; j < BLOCK_HEIGHT; j++) {
            int block_x = start_x + BLOCK_MARGIN + i * (BLOCK_SIZE + BLOCK_MARGIN);
            int block_y = start_y + BLOCK_MARGIN + j * (BLOCK_SIZE + BLOCK_MARGIN);
            
            int val_idx;
            switch (blocks[i][j]) {
                case 2:    val_idx =  1; break;
                case 4:    val_idx =  2; break;
                case 8:    val_idx =  3; break;
                case 16:   val_idx =  4; break;
                case 32:   val_idx =  5; break;
                case 64:   val_idx =  6; break;
                case 128:  val_idx =  7; break;
                case 256:  val_idx =  8; break;
                case 512:  val_idx =  9; break;
                case 1024: val_idx = 10; break;
                case 2048: val_idx = 11; break;
                default:   val_idx =  0; break;
            }
            
            // 绘制方块
            draw_block(block_x, block_y, BLOCK_SIZE, BLOCK_SIZE, block_colors[val_idx]);
            
            // 如果方块不为0，绘制数字
            if (blocks[i][j] != 0) {
                draw_number(block_x, block_y, blocks[i][j], BLOCK_SIZE, BLOCK_SIZE);
            }
        }
    }
    
    // 绘制分数
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "Score: %d", score);
    for (int i = 0; score_str[i]; i++) {
        draw_char(10 + i * 8, screen_h - 30, score_str[i], COL_BG);
    }
    
    // 刷新屏幕
    io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
}

void video_init() {
    AM_GPU_CONFIG_T config = io_read(AM_GPU_CONFIG);
    screen_w = config.width;
    screen_h = config.height;
    
    printf("Screen resolution: %dx%d\n", screen_w, screen_h);
}

int main() {
    ioe_init();
    video_init();
    
    panic_on(!io_read(AM_TIMER_CONFIG).present, "requires timer");
    panic_on(!io_read(AM_INPUT_CONFIG).present, "requires keyboard");
    
    srand(io_read(AM_TIMER_UPTIME).us);
    game_init();
    
    int current = 0, rendered = 0;
    uint64_t t0 = io_read(AM_TIMER_UPTIME).us;
    
    printf("2048 Game - Use arrow keys to play\n");
    
    while (1) {
        int frames = (io_read(AM_TIMER_UPTIME).us - t0) / (1000000 / FPS);
        
        for (; current < frames; current++) {
            game_logic_update(current);
        }
        
        while (1) {
            AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
            if (ev.keycode == AM_KEY_NONE) break;
            
            if (ev.keydown && ev.keycode == AM_KEY_ESCAPE) halt(0);
            
            // 只处理方向键
            if (ev.keydown && free_blocks > 0) {
                if (ev.keycode == AM_KEY_UP || ev.keycode == AM_KEY_DOWN ||
                    ev.keycode == AM_KEY_LEFT || ev.keycode == AM_KEY_RIGHT) {
                    
                    // 保存按键到环形缓冲区
                    key_info[write_index].key_code = ev.keycode;
                    key_info[write_index].valid = 1;
                    write_index = (write_index + 1) % BUFFER_SIZE;
                    free_blocks--;
                }
            }
        }
        
        if (current > rendered) {
            render();
            rendered = current;
        }
    }
}
