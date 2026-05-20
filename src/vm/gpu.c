#include "gpu.h"


gpu_t mgpu = {0};

bool gpu_init(vm_t *vm) {

  atomic_init(&mgpu.ready, false);
   
  if(!vm) return false;
  gpu_t *gpu = &mgpu;
  vm->gpu = &mgpu;

  gpu->fb = calloc(FB_WIDTH*FB_HEIGHT, sizeof(pixel_t));
  if(!gpu->fb) return false;

  if(!SDL_Init(SDL_INIT_VIDEO)) {
      fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return false;
  }
  
  gpu->refresh_rate=60;
  gpu->screen_width=FB_WIDTH;
  gpu->screen_height=FB_HEIGHT;
 
  gpu->window = SDL_CreateWindow("CGVM", gpu->screen_width, gpu->screen_height, 0);
  gpu->renderer = SDL_CreateRenderer(gpu->window, NULL);
  gpu->refresh_rate=60;
  gpu->screen_width=FB_WIDTH;
  gpu->screen_height=FB_HEIGHT;
  gpu->texture = SDL_CreateTexture(
    gpu->renderer,
    SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STREAMING,
    gpu->screen_width,
    gpu->screen_height                                  
  );
  atomic_store(&gpu->ready, true);
  return true;
}

void gpu_swap(gpu_t *gpu) {
    SDL_UpdateTexture(gpu->texture, nullptr, gpu->fb, gpu->screen_width*3);
    SDL_RenderClear(gpu->renderer);
    SDL_RenderTexture(gpu->renderer,gpu->texture,nullptr,nullptr);
    SDL_RenderPresent(gpu->renderer);
}

void gpu_destroy(gpu_t *gpu) {
  SDL_DestroyTexture(gpu->texture);
  SDL_DestroyRenderer(gpu->renderer);
  SDL_DestroyWindow(gpu->window);
  SDL_Quit();
}
