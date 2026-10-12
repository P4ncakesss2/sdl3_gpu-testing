#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cimgui.h>
#include <cglm/cglm.h>
#include <cglm/clipspace/persp_lh_zo.h>
#include <cglm/clipspace/view_lh_zo.h>

#include "imgui_integration.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct CubeVertex {
    float position[3];
    float uv[2];
} CubeVertex;

static const CubeVertex cube_vertices[] = {
    /* Front */
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}}, {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}}, {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}},
    /* Back */
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}}, {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}}, {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}},
    /* Right */
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}}, {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}}, {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}},
    /* Left */
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}}, {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}}, {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}},
    /* Top */
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}}, {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}}, {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}},
    /* Bottom */
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}}, {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}}, {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}}
};

static const Uint16 cube_indices[] = {
    0, 1, 2, 2, 3, 0,   4, 5, 6, 6, 7, 4,
    8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
};

static SDL_GPUBuffer *create_uploaded_buffer(SDL_GPUDevice *device,
                                              SDL_GPUCopyPass *copy_pass,
                                              SDL_GPUBufferUsageFlags usage,
                                              const void *data, size_t size)
{
    SDL_GPUBufferCreateInfo buffer_info = {
        .usage = usage,
        .size = (Uint32)size
    };
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (buffer == NULL) {
        SDL_Log("Could not create mesh buffer: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)size
    };
    SDL_GPUTransferBuffer *transfer_buffer =
        SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer_buffer == NULL) {
        SDL_Log("Could not create mesh upload buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    void *mapped_data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (mapped_data == NULL) {
        SDL_Log("Could not map mesh upload buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped_data, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUTransferBufferLocation source = {
        .transfer_buffer = transfer_buffer,
        .offset = 0
    };
    SDL_GPUBufferRegion destination = {
        .buffer = buffer,
        .offset = 0,
        .size = (Uint32)size
    };
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
    return buffer;
}

static SDL_GPUShader *load_shader(SDL_GPUDevice *device, const char *shader_name)
{
    SDL_GPUShaderStage stage;
    SDL_GPUShaderFormat format;
    const char *extension;
    const char *driver = SDL_GetGPUDeviceDriver(device);
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    size_t name_length = SDL_strlen(shader_name);
    const char *base_path = NULL;
    char *path = NULL;
    size_t code_size = 0;
    Uint8 *code;
    SDL_GPUShader *shader;

    if (name_length >= SDL_strlen(".vert.hlsl") &&
        SDL_strcmp(shader_name + name_length - SDL_strlen(".vert.hlsl"), ".vert.hlsl") == 0) {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else if (name_length >= SDL_strlen(".frag.hlsl") &&
               SDL_strcmp(shader_name + name_length - SDL_strlen(".frag.hlsl"), ".frag.hlsl") == 0) {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    } else {
        SDL_Log("Unsupported shader name %s", shader_name);
        return NULL;
    }

    if (SDL_strcmp(driver, "metal") == 0 && (formats & SDL_GPU_SHADERFORMAT_MSL)) {
        format = SDL_GPU_SHADERFORMAT_MSL;
        extension = "msl";
    } else if (SDL_strcmp(driver, "direct3d12") == 0 && (formats & SDL_GPU_SHADERFORMAT_DXIL)) {
        format = SDL_GPU_SHADERFORMAT_DXIL;
        extension = "dxil";
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        extension = "spirv";
    } else {
        SDL_Log("No precompiled shader format available for GPU backend %s", driver);
        return NULL;
    }

    base_path = SDL_GetBasePath();
    if (base_path == NULL ||
        SDL_asprintf(&path, "%sshaders/%.*s.%s", base_path,
                     (int)(name_length - SDL_strlen(".hlsl")),
                     shader_name, extension) < 0) {
        SDL_Log("Could not construct path for shader %s: %s", shader_name, SDL_GetError());
        return NULL;
    }
    code = SDL_LoadFile(path, &code_size);
    if (code == NULL) {
        SDL_Log("Could not read shader %s: %s", path, SDL_GetError());
        SDL_free(path);
        return NULL;
    }

    SDL_GPUShaderCreateInfo create_info = {
        .code_size = code_size,
        .code = code,
        /* Metal reserves `main`, so shadercross emits the MSL entry point as `main0`. */
        .entrypoint = format == SDL_GPU_SHADERFORMAT_MSL ? "main0" : "main",
        .format = format,
        .stage = stage,
        .num_samplers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1 : 0,
        .num_uniform_buffers = stage == SDL_GPU_SHADERSTAGE_VERTEX ? 1 : 0
    };
    shader = SDL_CreateGPUShader(device, &create_info);
    SDL_free(code);
    if (shader == NULL) {
        SDL_Log("Could not create GPU shader for %s: %s", path, SDL_GetError());
    }
    SDL_free(path);
    return shader;
}

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *name,
                                    SDL_GPUSampler **sampler_out)
{
    const char *base_path = SDL_GetBasePath();
    char *path = NULL;
    int width = 0;
    int height = 0;
    int channels = 0;
    Uint32 mip_levels = 1;
    size_t pixel_bytes = 0;
    stbi_uc *pixels = NULL;
    SDL_GPUTexture *texture = NULL;
    SDL_GPUTransferBuffer *transfer_buffer = NULL;
    SDL_GPUCommandBuffer *command_buffer = NULL;
    SDL_GPUSampler *sampler = NULL;

    *sampler_out = NULL;
    if (base_path == NULL ||
        SDL_asprintf(&path, "%sassets/%s", base_path, name) < 0) {
        SDL_Log("Could not construct texture path: %s", SDL_GetError());
        goto cleanup;
    }
    pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == NULL) {
        SDL_Log("Could not load texture %s: %s", path, stbi_failure_reason());
        goto cleanup;
    }
    pixel_bytes = (size_t)width * (size_t)height * 4;
    if (width <= 0 || height <= 0 || pixel_bytes > UINT32_MAX) {
        SDL_Log("Texture dimensions are invalid or too large: %s", path);
        goto cleanup;
    }
    for (Uint32 largest_dimension = (Uint32)SDL_max(width, height);
         largest_dimension > 1;
         largest_dimension >>= 1) {
        ++mip_levels;
    }

    SDL_GPUTextureCreateInfo texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                 SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = (Uint32)width,
        .height = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels = mip_levels,
        .sample_count = SDL_GPU_SAMPLECOUNT_1
    };
    texture = SDL_CreateGPUTexture(device, &texture_info);
    if (texture == NULL) {
        SDL_Log("Could not create texture %s: %s", path, SDL_GetError());
        goto cleanup;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)pixel_bytes
    };
    transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer_buffer == NULL) {
        SDL_Log("Could not create texture upload buffer: %s", SDL_GetError());
        goto cleanup;
    }
    void *mapped_pixels = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (mapped_pixels == NULL) {
        SDL_Log("Could not map texture upload buffer: %s", SDL_GetError());
        goto cleanup;
    }
    SDL_memcpy(mapped_pixels, pixels, pixel_bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (command_buffer == NULL) {
        SDL_Log("Could not acquire texture upload command buffer: %s", SDL_GetError());
        goto cleanup;
    }
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (copy_pass == NULL) {
        SDL_Log("Could not begin texture upload: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        command_buffer = NULL;
        goto cleanup;
    }
    SDL_GPUTextureTransferInfo source = {
        .transfer_buffer = transfer_buffer
    };
    SDL_GPUTextureRegion destination = {
        .texture = texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = (Uint32)width,
        .h = (Uint32)height,
        .d = 1
    };
    SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_GenerateMipmapsForGPUTexture(command_buffer, texture);
    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
        command_buffer = NULL;
        SDL_Log("Could not submit texture upload: %s", SDL_GetError());
        goto cleanup;
    }
    command_buffer = NULL;

    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .max_anisotropy = 16.0f,
        .max_lod = (float)(mip_levels - 1),
        .enable_anisotropy = true
    };
    sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (sampler == NULL) {
        SDL_Log("Could not create texture sampler: %s", SDL_GetError());
        goto cleanup;
    }
    *sampler_out = sampler;

cleanup:
    if (command_buffer != NULL) SDL_CancelGPUCommandBuffer(command_buffer);
    if (transfer_buffer != NULL) SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
    if (pixels != NULL) stbi_image_free(pixels);
    SDL_free(path);
    if (texture != NULL && sampler == NULL) {
        SDL_ReleaseGPUTexture(device, texture);
        texture = NULL;
    }
    return texture;
}

int main(int argc, char **argv)
{
    int result = EXIT_FAILURE;
    SDL_Window *window = NULL;
    SDL_GPUDevice *device = NULL;
    bool imgui_initialized = false;
    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;
    SDL_GPUTexture *triangle_texture = NULL;
    SDL_GPUSampler *triangle_sampler = NULL;
    SDL_GPUBuffer *cube_vertex_buffer = NULL;
    SDL_GPUBuffer *cube_index_buffer = NULL;
    SDL_GPUGraphicsPipeline *pipeline = NULL;
    SDL_GPUTexture *msaa_target = NULL;
    SDL_GPUTexture *depth_target = NULL;
    Uint32 msaa_target_width = 0;
    Uint32 msaa_target_height = 0;
    Uint32 depth_target_width = 0;
    Uint32 depth_target_height = 0;
    bool depth_target_msaa = false;
    bool vsync_enabled = false;
    const char *shader_name = argc > 1 ? argv[1] : "triangle";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        goto cleanup;
    }

    window = SDL_CreateWindow("SDL3 GPU Textured Cube", 800, 600,
                              SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        goto cleanup;
    }

    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                 SDL_GPU_SHADERFORMAT_DXIL |
                                 SDL_GPU_SHADERFORMAT_MSL, false, NULL);
    if (device == NULL) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        goto cleanup;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Could not claim window for GPU: %s", SDL_GetError());
        goto cleanup;
    }
    if (SDL_WindowSupportsGPUPresentMode(device, window,
                                         SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        if (!SDL_SetGPUSwapchainParameters(device, window,
                                           SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                           SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            SDL_Log("Could not disable GPU presentation vsync: %s", SDL_GetError());
            goto cleanup;
        }
    } else {
        vsync_enabled = true;
        SDL_Log("Immediate presentation is unsupported; VSync remains enabled");
    }

    {
        char *vertex_name = NULL;
        char *fragment_name = NULL;
        if (SDL_asprintf(&vertex_name, "%s.vert.hlsl", shader_name) < 0 ||
            SDL_asprintf(&fragment_name, "%s.frag.hlsl", shader_name) < 0) {
            SDL_Log("Could not construct shader names: %s", SDL_GetError());
            SDL_free(vertex_name);
            SDL_free(fragment_name);
            goto cleanup;
        }
        vertex_shader = load_shader(device, vertex_name);
        fragment_shader = load_shader(device, fragment_name);
        SDL_free(vertex_name);
        SDL_free(fragment_name);
    }
    if (vertex_shader == NULL || fragment_shader == NULL) {
        goto cleanup;
    }
    triangle_texture = load_texture(device, "images.jpg", &triangle_sampler);
    if (triangle_texture == NULL || triangle_sampler == NULL) {
        goto cleanup;
    }

    {
        SDL_GPUCommandBuffer *upload_command_buffer = SDL_AcquireGPUCommandBuffer(device);
        if (upload_command_buffer == NULL) {
            SDL_Log("Could not acquire mesh upload command buffer: %s", SDL_GetError());
            goto cleanup;
        }
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);
        if (copy_pass == NULL) {
            SDL_Log("Could not begin mesh upload: %s", SDL_GetError());
            SDL_CancelGPUCommandBuffer(upload_command_buffer);
            goto cleanup;
        }
        cube_vertex_buffer = create_uploaded_buffer(
            device, copy_pass, SDL_GPU_BUFFERUSAGE_VERTEX,
            cube_vertices, sizeof(cube_vertices));
        cube_index_buffer = create_uploaded_buffer(
            device, copy_pass, SDL_GPU_BUFFERUSAGE_INDEX,
            cube_indices, sizeof(cube_indices));
        SDL_EndGPUCopyPass(copy_pass);
        if (cube_vertex_buffer == NULL || cube_index_buffer == NULL) {
            SDL_CancelGPUCommandBuffer(upload_command_buffer);
            goto cleanup;
        }
        if (!SDL_SubmitGPUCommandBuffer(upload_command_buffer)) {
            SDL_Log("Could not submit mesh upload: %s", SDL_GetError());
            goto cleanup;
        }
    }

    SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GetGPUSwapchainTextureFormat(device, window),
        .blend_state = {
            .enable_blend = false,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .color_write_mask = SDL_GPU_COLORCOMPONENT_R |
                                SDL_GPU_COLORCOMPONENT_G |
                                SDL_GPU_COLORCOMPONENT_B |
                                SDL_GPU_COLORCOMPONENT_A
        }
    };
    SDL_GPUVertexBufferDescription vertex_buffer_description = {
        .slot = 0,
        .pitch = sizeof(CubeVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };
    SDL_GPUVertexAttribute vertex_attributes[] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(CubeVertex, position)
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(CubeVertex, uv)
        }
    };
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vertex_buffer_description,
            .num_vertex_buffers = 1,
            .vertex_attributes = vertex_attributes,
            .num_vertex_attributes = SDL_arraysize(vertex_attributes)
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
        },
        .multisample_state = { .sample_count = SDL_GPU_SAMPLECOUNT_1 },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_LESS,
            .enable_depth_test = true,
            .enable_depth_write = true
        },
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .has_depth_stencil_target = true
        }
    };
    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    if (pipeline == NULL) {
        SDL_Log("Could not create graphics pipeline: %s", SDL_GetError());
        goto cleanup;
    }

    if (!imgui_integration_init(window, device)) {
        SDL_Log("Could not initialize Dear ImGui: %s", SDL_GetError());
        goto cleanup;
    }
    imgui_initialized = true;

    result = EXIT_SUCCESS;
    bool running = true;
    bool show_demo_window = true;
    bool msaa_enabled = false;
    Uint64 fps_last_counter = SDL_GetPerformanceCounter();
    Uint64 fps_frame_count = 0;
    double fps = 0.0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imgui_integration_process_event(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        imgui_integration_new_frame();
        ++fps_frame_count;
        Uint64 fps_now = SDL_GetPerformanceCounter();
        double fps_elapsed = (double)(fps_now - fps_last_counter) /
                             (double)SDL_GetPerformanceFrequency();
        if (fps_elapsed >= 0.5) {
            fps = (double)fps_frame_count / fps_elapsed;
            fps_frame_count = 0;
            fps_last_counter = fps_now;
        }
        int window_width = 0;
        int window_height = 0;
        SDL_GetWindowSizeInPixels(window, &window_width, &window_height);
        mat4 model;
        mat4 view;
        mat4 projection;
        mat4 view_model;
        mat4 mvp;
        vec3 eye = { 0.0f, 0.0f, -3.0f };
        vec3 center = { 0.0f, 0.0f, 0.0f };
        vec3 up = { 0.0f, 1.0f, 0.0f };
        vec3 x_axis = { 1.0f, 0.0f, 0.0f };
        vec3 y_axis = { 0.0f, 1.0f, 0.0f };
        float aspect = window_width > 0 && window_height > 0
                           ? (float)window_width / (float)window_height
                           : 1.0f;
        float angle = (float)SDL_GetTicks() * 0.0005f;
        glm_mat4_identity(model);
        glm_rotate(model, angle, y_axis);
        glm_rotate(model, angle * 0.7f, x_axis);
        glm_lookat_lh_zo(eye, center, up, view);
        glm_perspective_lh_zo(glm_rad(60.0f), aspect, 0.1f, 100.0f, projection);
        glm_mat4_mul(view, model, view_model);
        glm_mat4_mul(projection, view_model, mvp);
        if (show_demo_window) {
            igShowDemoWindow(&show_demo_window);
        }
        
        igBegin("SDL3 GPU textured cube", NULL, 0);
        igText("FPS: %.1f", fps);
        igText("GPU backend: %s", SDL_GetGPUDeviceDriver(device));
        igText("Indexed textured cube");
        if (igCheckbox("VSync", &vsync_enabled)) {
            SDL_GPUPresentMode mode = vsync_enabled
                                          ? SDL_GPU_PRESENTMODE_VSYNC
                                          : SDL_GPU_PRESENTMODE_IMMEDIATE;
            if (!SDL_SetGPUSwapchainParameters(device, window,
                                               SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                               mode)) {
                SDL_Log("Could not change VSync: %s", SDL_GetError());
                vsync_enabled = !vsync_enabled;
            }
        }
        if (igCheckbox("MSAA (4x)", &msaa_enabled)) {
            if (msaa_enabled &&
                !SDL_GPUTextureSupportsSampleCount(device,
                                                   SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                                   SDL_GPU_SAMPLECOUNT_4)) {
                SDL_Log("4x MSAA is unsupported for the depth format");
                msaa_enabled = false;
            } else {
                SDL_GPUGraphicsPipelineCreateInfo updated_pipeline_info = pipeline_info;
                updated_pipeline_info.multisample_state.sample_count =
                    msaa_enabled ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
                SDL_GPUGraphicsPipeline *updated_pipeline =
                    SDL_CreateGPUGraphicsPipeline(device, &updated_pipeline_info);
                if (updated_pipeline == NULL) {
                    SDL_Log("Could not change MSAA: %s", SDL_GetError());
                    msaa_enabled = !msaa_enabled;
                } else {
                    SDL_WaitForGPUIdle(device);
                    SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
                    pipeline = updated_pipeline;
                }
            }
        }
        igEnd();
        igRender();

        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
        if (command_buffer == NULL) {
            SDL_Log("Could not acquire GPU command buffer: %s", SDL_GetError());
            result = EXIT_FAILURE;
            break;
        }
        SDL_PushGPUVertexUniformData(command_buffer, 0, mvp, sizeof(mvp));

        SDL_GPUTexture *swapchain_texture = NULL;
        Uint32 swapchain_width = 0;
        Uint32 swapchain_height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                                  &swapchain_texture,
                                                  &swapchain_width,
                                                  &swapchain_height)) {
            SDL_Log("Could not acquire swapchain texture: %s", SDL_GetError());
            SDL_CancelGPUCommandBuffer(command_buffer);
            result = EXIT_FAILURE;
            break;
        }
        if (swapchain_texture != NULL) {
            if (depth_target == NULL || depth_target_width != swapchain_width ||
                depth_target_height != swapchain_height ||
                depth_target_msaa != msaa_enabled) {
                SDL_WaitForGPUIdle(device);
                if (depth_target != NULL) {
                    SDL_ReleaseGPUTexture(device, depth_target);
                    depth_target = NULL;
                }
                SDL_GPUTextureCreateInfo depth_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                    .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                    .width = swapchain_width,
                    .height = swapchain_height,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = msaa_enabled ? SDL_GPU_SAMPLECOUNT_4
                                                 : SDL_GPU_SAMPLECOUNT_1
                };
                depth_target = SDL_CreateGPUTexture(device, &depth_info);
                if (depth_target == NULL) {
                    SDL_Log("Could not create depth target: %s", SDL_GetError());
                    SDL_CancelGPUCommandBuffer(command_buffer);
                    result = EXIT_FAILURE;
                    break;
                }
                depth_target_width = swapchain_width;
                depth_target_height = swapchain_height;
                depth_target_msaa = msaa_enabled;
            }
            if (msaa_enabled &&
                (msaa_target == NULL || msaa_target_width != swapchain_width ||
                 msaa_target_height != swapchain_height)) {
                SDL_WaitForGPUIdle(device);
                if (msaa_target != NULL) {
                    SDL_ReleaseGPUTexture(device, msaa_target);
                    msaa_target = NULL;
                }
                SDL_GPUTextureCreateInfo texture_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                    .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
                    .width = swapchain_width,
                    .height = swapchain_height,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_4
                };
                msaa_target = SDL_CreateGPUTexture(device, &texture_info);
                if (msaa_target == NULL) {
                    SDL_Log("Could not create MSAA render target: %s", SDL_GetError());
                    SDL_CancelGPUCommandBuffer(command_buffer);
                    result = EXIT_FAILURE;
                    break;
                }
                msaa_target_width = swapchain_width;
                msaa_target_height = swapchain_height;
            }
            imgui_integration_prepare_draw_data(command_buffer);
            SDL_GPUColorTargetInfo target = {
                .texture = msaa_enabled ? msaa_target : swapchain_texture,
                .clear_color = { 0.035f, 0.045f, 0.075f, 1.0f },
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = msaa_enabled ? SDL_GPU_STOREOP_RESOLVE
                                         : SDL_GPU_STOREOP_STORE,
                .resolve_texture = msaa_enabled ? swapchain_texture : NULL,
                .cycle = msaa_enabled,
                .cycle_resolve_texture = msaa_enabled
            };
            SDL_GPUDepthStencilTargetInfo depth_info = {
                .texture = depth_target,
                .clear_depth = 1.0f,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_DONT_CARE,
                .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
                .cycle = true
            };
            SDL_GPURenderPass *render_pass =
                SDL_BeginGPURenderPass(command_buffer, &target, 1, &depth_info);
            SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
            SDL_GPUTextureSamplerBinding texture_binding = {
                .texture = triangle_texture,
                .sampler = triangle_sampler
            };
            SDL_GPUBufferBinding vertex_binding = {
                .buffer = cube_vertex_buffer,
                .offset = 0
            };
            SDL_GPUBufferBinding index_binding = {
                .buffer = cube_index_buffer,
                .offset = 0
            };
            SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
            SDL_BindGPUIndexBuffer(render_pass, &index_binding,
                                   SDL_GPU_INDEXELEMENTSIZE_16BIT);
            SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_binding, 1);
            SDL_DrawGPUIndexedPrimitives(render_pass,
                                         SDL_arraysize(cube_indices), 1, 0, 0, 0);
            SDL_EndGPURenderPass(render_pass);
            if (msaa_enabled) {
                SDL_GPUColorTargetInfo imgui_target = {
                    .texture = swapchain_texture,
                    .load_op = SDL_GPU_LOADOP_LOAD,
                    .store_op = SDL_GPU_STOREOP_STORE
                };
                SDL_GPURenderPass *imgui_pass =
                    SDL_BeginGPURenderPass(command_buffer, &imgui_target, 1, NULL);
                imgui_integration_render_draw_data(command_buffer, imgui_pass);
                SDL_EndGPURenderPass(imgui_pass);
            } else {
                SDL_GPUColorTargetInfo ui_target = {
                    .texture = swapchain_texture,
                    .load_op = SDL_GPU_LOADOP_LOAD,
                    .store_op = SDL_GPU_STOREOP_STORE
                };
                SDL_GPURenderPass *ui_pass =
                    SDL_BeginGPURenderPass(command_buffer, &ui_target, 1, NULL);
                imgui_integration_render_draw_data(command_buffer, ui_pass);
                SDL_EndGPURenderPass(ui_pass);
            }
        }
        if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
            SDL_Log("Could not submit GPU command buffer: %s", SDL_GetError());
            result = EXIT_FAILURE;
            break;
        }
    }

cleanup:
    if (imgui_initialized) {
        SDL_WaitForGPUIdle(device);
        imgui_integration_shutdown();
    }
    if (device != NULL) {
        if (pipeline != NULL) SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        if (triangle_sampler != NULL) SDL_ReleaseGPUSampler(device, triangle_sampler);
        if (triangle_texture != NULL) SDL_ReleaseGPUTexture(device, triangle_texture);
        if (cube_vertex_buffer != NULL) SDL_ReleaseGPUBuffer(device, cube_vertex_buffer);
        if (cube_index_buffer != NULL) SDL_ReleaseGPUBuffer(device, cube_index_buffer);
        if (msaa_target != NULL) SDL_ReleaseGPUTexture(device, msaa_target);
        if (depth_target != NULL) SDL_ReleaseGPUTexture(device, depth_target);
        if (vertex_shader != NULL) SDL_ReleaseGPUShader(device, vertex_shader);
        if (fragment_shader != NULL) SDL_ReleaseGPUShader(device, fragment_shader);
        if (window != NULL) SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
    }
    if (window != NULL) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
