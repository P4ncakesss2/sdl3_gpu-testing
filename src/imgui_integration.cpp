#include "imgui_integration.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

extern "C" bool imgui_integration_init(SDL_Window *window, SDL_GPUDevice *device)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
        ImGui::DestroyContext();
        return false;
    }

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    return true;
}

extern "C" bool imgui_integration_process_event(const SDL_Event *event)
{
    return ImGui_ImplSDL3_ProcessEvent(event);
}

extern "C" void imgui_integration_new_frame(void)
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

extern "C" void imgui_integration_prepare_draw_data(SDL_GPUCommandBuffer *command_buffer)
{
    ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), command_buffer);
}

extern "C" void imgui_integration_render_draw_data(SDL_GPUCommandBuffer *command_buffer,
                                                    SDL_GPURenderPass *render_pass)
{
    ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), command_buffer, render_pass, NULL);
}

extern "C" void imgui_integration_shutdown(void)
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
