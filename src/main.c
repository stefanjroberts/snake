#include "core/event.h"
#include "core/logger.h"
#include "core/memory.h"
#include "core/input.h"
#include "renderer/vulkan.h"
#include <GLFW/glfw3.h>

int main()
{
    platform_memory_init();
    initialise_event_handler();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan Test", NULL, NULL);
    if (window == NULL)
    {
        PFATAL("Failed to create glfw window");
    }

    init_vulkan(window);

    struct InputState* input_state  = initialise_input(window);

    

    f64 angle = 0;

    while (!glfwWindowShouldClose(window))
    {
        vulkan_test(angle);
        angle += 0.00004f;
    }

    return 0;
}
