#pragma once
#include <gfx_positioning.hpp>
#include <stdint.h>
#ifdef SCREEN_SIZE
constexpr static const gfx::size16 screen_size = SCREEN_SIZE;
#else
constexpr static const gfx::size16 screen_size = {320,240};
#endif
uint32_t millis();
void setup();
void loop();
void flush_bitmap(const gfx::rect16& bounds, const void* bmp );
bool read_mouse(gfx::point16* out_location);