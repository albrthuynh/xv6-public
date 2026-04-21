#include "types.h"
#include "user.h"
#include "gui.h"

// A compressed 3x5 font array using bitmasks (bits 0-14). 
// To keep it short, we define a few letters and numbers. You can expand this later!
short font_data[128] = {0};

void init_font() {
  font_data['A'] = 0b101111101101010; // A
  font_data['B'] = 0b110101110101110; // B
  font_data['O'] = 0b010101101101010; // O
  font_data['U'] = 0b111101101101101; // U
  font_data['T'] = 0b010010010010111; // T
  font_data['4'] = 0b001001111101101; // 4
  font_data['6'] = 0b111101111100111; // 6
  font_data['1'] = 0b111010010110010; // 1
}

void draw_char(int x, int y, char c, int color) {
  if (font_data['A'] == 0) init_font(); // Lazy load the font
  
  short glyph = font_data[(int)c];
  uchar buffer[15]; // 3x5 grid = 15 pixels

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 3; j++) {
      // Extract the correct bit from our 15-bit glyph
      int bit_index = i * 3 + (2-j);
      if ((glyph >> bit_index) & 1) {
        buffer[i * 3 + j] = color;
      } else {
        buffer[i * 3 + j] = 255; // 255 is our transparent mask
      }
    }
  }
  // One blazing fast system call per character
  draw_bitmap(x, y, 3, 5, buffer); 
}

void draw_string(int x, int y, char *str, int color) {
  int cursor_x = x;
  while (*str) {
    if (*str != ' ') draw_char(cursor_x, y, *str, color);
    cursor_x += 4; // Move cursor right by 4 pixels (3 for char + 1 for spacing)
    str++;
  }
}
