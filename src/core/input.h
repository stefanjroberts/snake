#pragma once
#include <GLFW/glfw3.h>

struct InputState {
    char up;
    char down;
    char left;
    char right;
};

struct InputState* initialise_input(GLFWwindow *in_window);
void clear_input();