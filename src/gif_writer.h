// Grey-scale animated GIF writer with the newsreel look (grain, vignette, flicker, scratches). No game dependency, so
// tools/gif_writer_test.cpp can build and check it offline.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <random>
#include <string>
#include <vector>

struct GifFrames {
  int w = 0, h = 0;
  std::vector<std::vector<uint8_t>> frames;                         // w*h grey bytes each, top row first
  std::vector<int> delays;                                            // per frame, in 1/100 s
};

static const int kGifBits = 5;                                        // 32 grey levels: grain stays cheap for LZW

// One frame's pixels (palette indices 0..31) as GIF LZW sub-blocks.
static void GifLzw(std::vector<uint8_t>& out, const std::vector<uint8_t>& px) {
  const int minBits = kGifBits, clear = 1 << minBits, eoi = clear + 1, nsym = 1 << minBits;
  std::vector<int16_t> next(4096 * nsym, -1);                         // next[code*nsym+sym] = child code
  int bits = minBits + 1, nextCode = eoi + 1;
  uint32_t acc = 0; int accBits = 0;
  std::vector<uint8_t> bytes;
  auto put = [&](int code) { acc |= (uint32_t)code << accBits; accBits += bits; while (accBits >= 8) { bytes.push_back((uint8_t)acc); acc >>= 8; accBits -= 8; } };
  put(clear);
  int cur = px.empty() ? 0 : px[0];
  for (size_t i = 1; i < px.size(); i++) {
    int s = px[i], n = next[cur * nsym + s];
    if (n >= 0) { cur = n; continue; }
    put(cur);
    if (nextCode < 4096) { next[cur * nsym + s] = (int16_t)nextCode; if (nextCode == (1 << bits) && bits < 12) bits++; nextCode++; }
    if (nextCode == 4096) { put(clear); std::fill(next.begin(), next.end(), (int16_t)-1); bits = minBits + 1; nextCode = eoi + 1; }
    cur = s;
  }
  put(cur); put(eoi);
  if (accBits > 0) bytes.push_back((uint8_t)acc);
  out.push_back((uint8_t)minBits);
  for (size_t i = 0; i < bytes.size(); i += 255) {
    size_t n = (std::min)((size_t)255, bytes.size() - i);
    out.push_back((uint8_t)n); out.insert(out.end(), bytes.begin() + i, bytes.begin() + i + n);
  }
  out.push_back(0);
}

// The newsreel look on one grey frame -> palette indices. rng is shared so grain differs per frame.
static std::vector<uint8_t> GifStyle(const std::vector<uint8_t>& g, int w, int h, std::mt19937& rng, const std::vector<float>& vig) {
  std::uniform_int_distribution<int> grain(-14, 14), coin(0, 99), col(0, w - 1);
  float flicker = 0.94f + (rng() % 1000) / 1000.0f * 0.1f;
  const int gs = (std::max)(1, w / 320);                              // grain and scratch size scale with the picture (same look, smaller file)
  int scratch = coin(rng) < 25 ? col(rng) / gs * gs : -1;
  std::vector<int> row((size_t)(w + gs - 1) / gs);
  std::vector<uint8_t> out(g.size());
  for (int y = 0; y < h; y++) {
    if (y % gs == 0) for (int& n : row) n = grain(rng);
    for (int x = 0; x < w; x++) {
      size_t i = (size_t)y * w + x;
      float v = (g[i] - 128) * 1.15f + 128;                          // a touch more contrast
      v = v * flicker * vig[i] + row[x / gs];
      if (scratch >= 0 && x >= scratch && x < scratch + gs) v += 60;
      int q = (int)(v / 256.0f * 32); out[i] = (uint8_t)(q < 0 ? 0 : q > 31 ? 31 : q);
    }
  }
  return out;
}

// Writes the frames to path. Returns the file size, or -1 when it cannot be written (a partial file is removed).
// Streamed: o holds about one frame and is flushed after each, since the whole film in memory adds 80-160 MB on top
// of the frames. An exception (bad_alloc) also removes the file, then rethrows.
static long GifWrite(const std::string& path, const GifFrames& f) {
  if (f.frames.empty() || f.w <= 0 || f.h <= 0) return -1;
  FILE* fp = fopen(path.c_str(), "wb"); if (!fp) return -1;
  long total = 0; bool ok = true;
  std::vector<uint8_t> o;
  auto flush = [&]() { if (ok && !o.empty()) { ok = fwrite(o.data(), 1, o.size(), fp) == o.size(); total += (long)o.size(); } o.clear(); };
  try {
  auto u16 = [&](int v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); };
  const char* hdr = "GIF89a"; o.insert(o.end(), hdr, hdr + 6);
  u16(f.w); u16(f.h); o.push_back(0xF0 | (kGifBits - 1)); o.push_back(0); o.push_back(0);   // global colour table of 2^kGifBits greys
  for (int i = 0; i < (1 << kGifBits); i++) { uint8_t c = (uint8_t)(i * 255 / ((1 << kGifBits) - 1)); o.push_back(c); o.push_back(c); o.push_back(c); }
  const uint8_t loop[] = { 0x21, 0xFF, 0x0B, 'N','E','T','S','C','A','P','E','2','.','0', 3, 1, 0, 0, 0 };   // NETSCAPE2.0: loop forever
  o.insert(o.end(), loop, loop + sizeof(loop));
  flush();
  std::vector<float> vig((size_t)f.w * f.h);
  for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) {
    float dx = (x - f.w / 2.0f) / (f.w / 2.0f), dy = (y - f.h / 2.0f) / (f.h / 2.0f);
    vig[(size_t)y * f.w + x] = 1.0f - 0.45f * (std::min)(1.0f, (dx * dx + dy * dy) * 0.6f);
  }
  std::mt19937 rng(1942);
  for (size_t k = 0; k < f.frames.size(); k++) {
    int d = k < f.delays.size() ? f.delays[k] : 10;
    const uint8_t gce[] = { 0x21, 0xF9, 4, 0, (uint8_t)d, (uint8_t)(d >> 8), 0, 0 };   // graphic control extension: the delay
    o.insert(o.end(), gce, gce + sizeof(gce));
    o.push_back(0x2C); u16(0); u16(0); u16(f.w); u16(f.h); o.push_back(0);
    GifLzw(o, GifStyle(f.frames[k], f.w, f.h, rng, vig));
    flush(); if (!ok) break;                                          // disk full: stop, the file is removed below
  }
  o.push_back(0x3B); flush();
  } catch (...) { fclose(fp); remove(path.c_str()); throw; }
  if (fclose(fp) != 0) ok = false;
  if (!ok) { remove(path.c_str()); return -1; }
  return total;
}
