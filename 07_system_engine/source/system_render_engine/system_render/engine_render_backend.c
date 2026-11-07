#include "engine_globals.h"

bool create_render_backend(render_backend_type type, render_backend* out_renderer_backend) 
{
    phoenix_trace("BACKEND SYSTEM", "TRACING THE PROGRAM 11: STARTING THE RENDER BACKEND SYSTEM");
    if (type == render_backend_type_vulkan) 
    {
        out_renderer_backend->start_render   = start_vulkan_render_backend;
        out_renderer_backend->shutdown_render= shutdown_vulkan_render_backend;
        out_renderer_backend->begin_frame     = begin_vulkan_render_backend_frame;
        out_renderer_backend->end_frame       = end_vulkan_render_backend_frame;
        out_renderer_backend->resized         = vulkan_render_backend_on_resized;
        
        return TRUE; 
    }

    phoenix_trace("BACKEND SYSTEM", "TRACING THE PROGRAM 11: LEAVING THE RENDER BACKEND SYSTEM");
    return FALSE;
}

void destroy_render_backend(render_backend* renderer_backend) 
{
    renderer_backend->start_render = 0;
    renderer_backend->shutdown_render = 0;
    renderer_backend->begin_frame = 0;
    renderer_backend->end_frame = 0;
    renderer_backend->resized = 0;
}
