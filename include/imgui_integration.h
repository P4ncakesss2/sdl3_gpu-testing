#ifndef IMGUI_INTEGRATION_H
#define IMGUI_INTEGRATION_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#ifdef __cplusplus
extern "C" {
#endif

bool imgui_integration_init(SDL_Window *window, SDL_GPUDevice *device);
bool imgui_integration_process_event(const SDL_Event *event);
void imgui_integration_new_frame(void);
void imgui_integration_prepare_draw_data(SDL_GPUCommandBuffer *command_buffer);
void imgui_integration_render_draw_data(SDL_GPUCommandBuffer *command_buffer,
                                        SDL_GPURenderPass *render_pass);
void imgui_integration_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
