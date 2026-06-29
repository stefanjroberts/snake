#include "input.h"
#include <GLFW/glfw3.h>

static struct InputState input_state;

static GLFWwindow *window;

// TODO: Is this thread safe?
//TODO: Turn this into an action queue, mailbox style.

void keypress_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    switch (key)
    {
    case GLFW_KEY_W:
    case GLFW_KEY_UP:
        input_state.up = 1;
        input_state.down = 0;
        break;
    case GLFW_KEY_S:
    case GLFW_KEY_DOWN:
        input_state.down = 1;
        input_state.up = 0;
        break;
    case GLFW_KEY_A:
    case GLFW_KEY_LEFT:
        input_state.left = 1;
        input_state.right = 0;
        break;
    case GLFW_KEY_D:
    case GLFW_KEY_RIGHT:
        input_state.right = 1;
        input_state.left = 0;
        break;
    }
    return;
}

struct InputState* initialise_input(GLFWwindow *in_window)
{
    window = in_window;

    glfwSetKeyCallback(window, keypress_callback);
    return &input_state;
}

void clear_input()
{
    input_state.down = 0;
    input_state.up = 0;
    input_state.left = 0;
    input_state.right = 0;

    return;
}
