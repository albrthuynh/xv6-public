#include "types.h"
#include "user.h"
#include "gui.h"

// A compressed 3x5 font array using bitmasks (bits 0-14). 
// To keep it short, we define a few letters and numbers. You can expand this later!
short font_data[128] = {0};

void init_font() {
  // Uppercase
  font_data['A'] = 0b101101111101010;
  font_data['B'] = 0b111101110101110;
  font_data['C'] = 0b011100100100011;
  font_data['D'] = 0b110101101101110;
  font_data['E'] = 0b111100111100111;
  font_data['F'] = 0b100100111100111;
  font_data['G'] = 0b011101101100011;
  font_data['H'] = 0b101101111101101;
  font_data['I'] = 0b111010010010111;
  font_data['J'] = 0b010101001001001;
  font_data['K'] = 0b101101110101101;
  font_data['L'] = 0b111100100100100;
  font_data['M'] = 0b101101101111101;
  font_data['N'] = 0b101101101101111;
  font_data['O'] = 0b010101101101010;
  font_data['P'] = 0b100100110101110;
  font_data['Q'] = 0b001010101101010;
  font_data['R'] = 0b101101110101110;
  font_data['S'] = 0b110001010100011;
  font_data['T'] = 0b010010010010111;
  font_data['U'] = 0b111101101101101;
  font_data['V'] = 0b010010101101101;
  font_data['W'] = 0b101111101101101;
  font_data['X'] = 0b101101010101101;
  font_data['Y'] = 0b010010010101101;
  font_data['Z'] = 0b111100010001111;

  //Numbers
  font_data['0'] = 0b010101101101010;
  font_data['1'] = 0b111010010110010;
  font_data['2'] = 0b111100010001110;
  font_data['3'] = 0b111001011001111;
  font_data['4'] = 0b001001111101101;
  font_data['5'] = 0b111001111100111;
  font_data['6'] = 0b010101110100011;
  font_data['7'] = 0b001001001001111;
  font_data['8'] = 0b010101010101010;
  font_data['9'] = 0b010001011101010;

  //Special
  font_data['.'] = 0b010000000000000;
  font_data[':'] = 0b000010000010000;
  font_data['/'] = 0b100100010001001;
  font_data['-'] = 0b000000111000000;
  font_data['_'] = 0b111000000000000;
  font_data['>'] = 0b100010001010100;
  font_data['~'] = 0b000000000101010;
  font_data['@'] = 0b010100111101010;
  font_data['#'] = 0b010111010111010;

  //Lower case
  for(int i = 'a'; i <= 'z'; i++) {
    font_data[i] = font_data[i - 32];
  }
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
