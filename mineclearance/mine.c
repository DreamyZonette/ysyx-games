#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <am.h>
#include <amdev.h>
#include <klib-macros.h>

#define GRID_SIZE 16  // 网格大小（16x16）
#define NUM_MINES 40  // 地雷数量
#define TILE_W 10     // 每个单元格的像素大小
#define LETTER_HIGHT 30 //胜利或者失败时候字母显示的高度

#define COLOR_BLUE        0x000000ff // 1：蓝色
#define COLOR_GREEN       0x0000ff00 // 2：绿色
#define COLOR_YELLOW      0x00ffff00 // 3：黄色
#define COLOR_PURP_RED    0x00ff00ff // 4：品红（紫红）
#define COLOR_ORANGE      0x00ffa500 // 5：橙色
#define COLOR_OLIVE_GREEN 0x00808000 // 6：橄榄绿
#define COLOR_PURP        0x00800080 // 7：紫色
#define COLOR_CYAN        0x00008080 // 8：青色

extern const int number_patterns[9][5][5];
extern const int flag_pattern[5][5];
extern const int mine_pattern[5][5];
extern const int letter_patterns[7][5][5];

// 游戏状态枚举
typedef enum {
  PLAYING,
  WON,
  LOST
} game_state_t;

// 单元格结构体
typedef struct {
  bool has_mine;       // 是否有地雷
  bool revealed;       // 是否已揭开
  bool flagged;        // 是否标记为地雷
  int adjacent_mines;  // 周围地雷数量
} cell_t;

// 二维数组表示扫雷网格
static cell_t minefield[GRID_SIZE][GRID_SIZE];

static int cursor_x, cursor_y;  // 光标位置
static int revealed_count;      // 已揭开的非地雷单元格数量
static int flagged_count;       // 已标记的地雷数量
static int grid_x, grid_y;      // 网格在屏幕上的起始位置
static game_state_t game_state; // 游戏状态

// 刷新屏幕
static void refresh() {
  io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
}

// 绘制单元格（支持边框、数字、旗帜和地雷）
static void draw_tile(int y, int x, uint32_t color, uint32_t border_color, int number, bool is_flag, bool is_mine) {
  static uint32_t buf[TILE_W * TILE_W];
  for (int i = 0; i < TILE_W * TILE_W; i++) {
    buf[i] = color;
  }
  if (border_color != 0) {
    for (int i = 0; i < TILE_W; i++) {
      buf[i] = border_color;                   // 上边框
      buf[i * TILE_W] = border_color;          // 左边框
      buf[i * TILE_W + TILE_W - 1] = border_color; // 右边框
      buf[(TILE_W - 1) * TILE_W + i] = border_color; // 下边框
    }
  }
  int start_y = (TILE_W - 5) / 2;
  int start_x = (TILE_W - 5) / 2;
  if (number >= 0 && number <= 8) {
    int pattern[5][5];
    memcpy(pattern, number_patterns[number], sizeof(pattern));
    for (int dy = 0; dy < 5; dy++) {
      for (int dx = 0; dx < 5; dx++) {
        if (pattern[dy][dx]) {
          int pixel_y = start_y + dy;
          int pixel_x = start_x + dx;
          buf[pixel_y * TILE_W + pixel_x] = 0x00000000; // 黑色
        }
      }
    }
  } else if (is_flag) {
    for (int dy = 0; dy < 5; dy++) {
      for (int dx = 0; dx < 5; dx++) {
        if (flag_pattern[dy][dx]) {
          int pixel_y = start_y + dy;
          int pixel_x = start_x + dx;
          buf[pixel_y * TILE_W + pixel_x] = 0x00000000; // 黑色
        }
      }
    }
  } else if (is_mine) {
    for (int dy = 0; dy < 5; dy++) {
      for (int dx = 0; dx < 5; dx++) {
        if (mine_pattern[dy][dx]) {
          int pixel_y = start_y + dy;
          int pixel_x = start_x + dx;
          buf[pixel_y * TILE_W + pixel_x] = 0x00000000; // 黑色
        }
      }
    }
  }
  io_write(AM_GPU_FBDRAW, x * TILE_W, y * TILE_W, buf, TILE_W, TILE_W, false);
}

// 读取键盘输入
static int read_key() {
  while (1) {
    AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
    if (ev.keydown || ev.keycode == AM_KEY_NONE) return ev.keycode;
  }
}

// 计算每个单元格周围的地雷数量
static void calculate_adjacent_mines() {
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      if (!minefield[y][x].has_mine) {
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ny = y + dy;
            int nx = x + dx;
            if (ny >= 0 && ny < GRID_SIZE && nx >= 0 && nx < GRID_SIZE && minefield[ny][nx].has_mine) {
              count++;
            }
          }
        }
        minefield[y][x].adjacent_mines = count;
      }
    }
  }
}

// 绘制单个单元格
static void draw_cell(int y, int x) {
  cell_t *cell = &minefield[y][x];
  uint32_t color = 0;  // 初始化 color 为 0
  int number = -1;     // -1 表示不绘制数字
  bool is_flag = false;
  bool is_mine = false;

  if (cell->revealed) {
    if (cell->has_mine) {
      color = 0x00ff0000; // 地雷：红色
      is_mine = true;
    } else if (cell->adjacent_mines > 0) {
      switch (cell->adjacent_mines) {
        case 1: color = COLOR_BLUE; break;
        case 2: color = COLOR_GREEN; break;
        case 3: color = COLOR_YELLOW; break;
        case 4: color = COLOR_PURP_RED; break;
        case 5: color = COLOR_ORANGE; break;
        case 6: color = COLOR_OLIVE_GREEN; break;
        case 7: color = COLOR_PURP; break;
        case 8: color = COLOR_CYAN; break;
      }
      number = cell->adjacent_mines;
    } else {
      color = 0x00d3d3d3; // 空单元格：浅灰色
    }
  } else {
    if (cell->flagged) {
      color = 0x00a9a9a9; // 标记：暗灰色
      is_flag = true;
    } else {
      color = 0x00404040; // 未揭开：深灰色
    }
  }

  uint32_t border_color = (y == cursor_y && x == cursor_x) ? 0x00ffffff : 0; // 光标：白色边框
  draw_tile(grid_y + y, grid_x + x, color, border_color, number, is_flag, is_mine);
}

// 绘制整个扫雷网格
static void draw_minefield() {
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      draw_cell(y, x);
    }
  }
}

// DFS算法递归揭开空白区域
static void reveal_cell_dfs(int y, int x) {
  if (y < 0 || y >= GRID_SIZE || x < 0 || x >= GRID_SIZE || minefield[y][x].revealed) {
    return;
  }

  minefield[y][x].revealed = true;
  if (!minefield[y][x].has_mine) {
    revealed_count++;
  }

  if (minefield[y][x].adjacent_mines == 0) {
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx != 0 || dy != 0) {
          reveal_cell_dfs(y + dy, x + dx);
        }
      }
    }
  }
}

// 揭开所有地雷（失败时调用）
static void reveal_all_mines() {
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      if (minefield[y][x].has_mine) {
        minefield[y][x].revealed = true;
      }
    }
  }
}

// 检查是否正确标记所有地雷
static bool check_all_mines_flagged() {
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      if (minefield[y][x].has_mine && !minefield[y][x].flagged) {
        return false; // 存在未标记的地雷
      }
      if (!minefield[y][x].has_mine && minefield[y][x].flagged) {
        return false; // 错误标记了非地雷格子
      }
    }
  }
  return true;
}

// 绘制文本
static void draw_text(const char *text, int start_y, int start_x, uint32_t color) {
  int x_offset = 0;
  for (int i = 0; text[i] != '\0'; i++) {
    int letter_idx;
    switch (text[i]) {
      case 'W': letter_idx = 0; break;
      case 'I': letter_idx = 1; break;
      case 'N': letter_idx = 2; break;
      case 'L': letter_idx = 3; break;
      case 'O': letter_idx = 4; break;
      case 'S': letter_idx = 5; break;
      case 'E': letter_idx = 6; break;
      default: continue;
    }
    for (int y = 0; y < 5; y++) {
      for (int x = 0; x < 5; x++) {
        if (letter_patterns[letter_idx][y][x]) {
          draw_tile(start_y + y, start_x + x_offset + x, color, 0, -1, false, false);
        }
      }
    }
    x_offset += 6; // 每个字母占用5格宽度，加1格间距
  }
}

int main() {
  ioe_init();

  int screen_height = io_read(AM_GPU_CONFIG).height / TILE_W;
  int screen_width = io_read(AM_GPU_CONFIG).width / TILE_W;

  grid_x = (screen_width - GRID_SIZE) / 2;
  grid_y = (screen_height - GRID_SIZE) / 2;

  memset(minefield, 0, sizeof(minefield)); 
  
  printf("\n按下空格键--选择   按下'F'--插上旗帜\n");
  printf("\n蓝色--1  绿色--2\n黄色--3  紫红--4\n橙色--5  橄榄绿--6\n紫色--7  青色--8\n");
	
  srand(io_read(AM_TIMER_UPTIME).us);
  int placed = 0;
  while (placed < NUM_MINES) {
    int x = rand() % GRID_SIZE;
    int y = rand() % GRID_SIZE;
    if (!minefield[y][x].has_mine) {
      minefield[y][x].has_mine = true;
      placed++;
    }
  }

  calculate_adjacent_mines();

  cursor_x = GRID_SIZE / 2;
  cursor_y = GRID_SIZE / 2;
  revealed_count = 0;
  flagged_count = 0;
  game_state = PLAYING;

  while (game_state == PLAYING) {
    draw_minefield();
    refresh();

    int key = read_key();
    switch (key) {
      case AM_KEY_LEFT:  if (cursor_x > 0) cursor_x--; break;
      case AM_KEY_RIGHT: if (cursor_x < GRID_SIZE - 1) cursor_x++; break;
      case AM_KEY_UP:    if (cursor_y > 0) cursor_y--; break;
      case AM_KEY_DOWN:  if (cursor_y < GRID_SIZE - 1) cursor_y++; break;
      case AM_KEY_SPACE:
        if (!minefield[cursor_y][cursor_x].revealed && !minefield[cursor_y][cursor_x].flagged) {
          reveal_cell_dfs(cursor_y, cursor_x);
          if (minefield[cursor_y][cursor_x].has_mine) {
            game_state = LOST;
          } else if (revealed_count == GRID_SIZE * GRID_SIZE - NUM_MINES) {
            game_state = WON;
          }
        }
        break;
      case AM_KEY_F:
        if (!minefield[cursor_y][cursor_x].revealed) {
          minefield[cursor_y][cursor_x].flagged = !minefield[cursor_y][cursor_x].flagged;
          flagged_count += minefield[cursor_y][cursor_x].flagged ? 1 : -1;
          if (flagged_count == NUM_MINES && check_all_mines_flagged()) {
            game_state = WON;
          }
        }
        break;
    }
  }

  int text_y = (screen_height - LETTER_HIGHT) / 2; 
  int text_x = (screen_width - (3 * 6)) / 2; //"WIN"或"LOSE"宽度（3个字母x(5+1)）

  if (game_state == LOST) {
    reveal_all_mines();
    draw_minefield();
    draw_text("LOSE", text_y, text_x, COLOR_PURP_RED); 
    refresh();
    printf("\n\n游戏结束\n按 Q 退出\n");
  } else if (game_state == WON) {
    draw_minefield();
    draw_text("WIN", text_y, text_x, COLOR_GREEN); 
    refresh();
    printf("\n\n你赢了\n按 Q 退出\n");
  }

  while (read_key() != AM_KEY_Q);
  return 0;
}