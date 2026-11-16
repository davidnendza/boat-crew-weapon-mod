// Offline check of src/gif_writer.h: writes a moving gradient; tools/gif_writer_check.py decodes it with Pillow.
// cl /nologo /EHsc /O2 tools\gif_writer_test.cpp /Fe:out\gif_writer_test.exe && out\gif_writer_test.exe out\test.gif
#include <algorithm>
#include "../src/gif_writer.h"
int main(int argc, char** argv) {
  GifFrames f; f.w = 320; f.h = 180;
  for (int k = 0; k < 30; k++) {
    std::vector<uint8_t> g((size_t)f.w * f.h);
    for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) g[(size_t)y * f.w + x] = (uint8_t)((x + k * 8) & 255);
    f.frames.push_back(g); f.delays.push_back(10);
  }
  long n = GifWrite(argc > 1 ? argv[1] : "test.gif", f);
  printf("wrote %ld bytes\n", n);
  return n > 0 ? 0 : 1;
}
