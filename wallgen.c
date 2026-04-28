#include <stdio.h>
#include <stdlib.h>

#define W 320
#define H 200

static void
write_file(const char *name, unsigned char pixels[H][W])
{
  FILE *f;

  f = fopen(name, "wb");
  if(f == 0) {
    perror(name);
    exit(1);
  }
  if(fwrite(pixels, 1, W * H, f) != W * H) {
    perror(name);
    fclose(f);
    exit(1);
  }
  fclose(f);
}

static void
make_grid(unsigned char pixels[H][W])
{
  int x;
  int y;

  for(y = 0; y < H; y++) {
    for(x = 0; x < W; x++) {
      if((x / 10 + y / 10) % 2 == 0)
        pixels[y][x] = 11;
      else
        pixels[y][x] = 9;
      if((x % 40) == 0 || (y % 40) == 0)
        pixels[y][x] = 15;
    }
  }
}

static void
make_party(unsigned char pixels[H][W])
{
  int x;
  int y;

  for(y = 0; y < H; y++) {
    for(x = 0; x < W; x++) {
      int stripe = (x + y) / 14;
      int sparkle = ((x * 17 + y * 29) % 97) == 0;

      pixels[y][x] = 1 + (stripe % 14);
      if(sparkle)
        pixels[y][x] = 15;
    }
  }
}

static void
make_night(unsigned char pixels[H][W])
{
  int x;
  int y;

  for(y = 0; y < H; y++) {
    for(x = 0; x < W; x++) {
      pixels[y][x] = (y < 70) ? 1 : (y < 135 ? 9 : 2);
      if(((x * 13 + y * 7) % 211) == 0 && y < 120)
        pixels[y][x] = 15;
      if(y > 138 && ((x + y) % 11) < 3)
        pixels[y][x] = 10;
    }
  }
}

int
main(void)
{
  static unsigned char pixels[H][W];

  make_grid(pixels);
  write_file("wallpaper_grid", pixels);

  make_party(pixels);
  write_file("wallpaper_party", pixels);

  make_night(pixels);
  write_file("wallpaper_night", pixels);

  return 0;
}
