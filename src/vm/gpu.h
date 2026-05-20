#pragma once
#include "../gfx/gfx.h"
#include "vm.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
struct vm;

struct gpu {
  pixel_t *fb;
  uint16_t screen_height;
  uint16_t screen_width;
  uint8_t refresh_rate;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  atomic_bool ready;
};
typedef struct gpu gpu_t;

bool gpu_init(struct vm *vm);
void gpu_swap(gpu_t *gpu);
void gpu_destroy(gpu_t *gpu);
