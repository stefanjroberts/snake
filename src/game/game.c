#include "game.h"
#include "../core/logger.h"

enum SnakeDirection
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    STATIC
};

struct GameState
{
    struct InputState *input;
    enum SnakeDirection snake_direction;
};

static struct GameState game_state;

void game_init(struct InputState *input_state)
{
    game_state.input = input_state;
    game_state.snake_direction = STATIC;
}

void game_update()
{
    // PROCESS INPUT
    switch (game_state.snake_direction)
    {
    case STATIC:
        if (game_state.input->up)
        {
            game_state.snake_direction = UP;
        }
        if (game_state.input->down)
        {
            game_state.snake_direction = DOWN;
        }
        if (game_state.input->left)
        {
            game_state.snake_direction = LEFT;
        }
        if (game_state.input->right)
        {
            game_state.snake_direction = RIGHT;
        }
        break;
    case UP:
        if (game_state.input->left)
        {
            game_state.snake_direction = LEFT;
        }
        if (game_state.input->right)
        {
            game_state.snake_direction = RIGHT;
        }
        break;
    case DOWN:
        if (game_state.input->left)
        {
            game_state.snake_direction = LEFT;
        }
        if (game_state.input->right)
        {
            game_state.snake_direction = RIGHT;
        }
        break;
    case LEFT:
        if (game_state.input->up)
        {
            game_state.snake_direction = UP;
        }
        if (game_state.input->down)
        {
            game_state.snake_direction = DOWN;
        }
        break;
    case RIGHT:
        if (game_state.input->up)
        {
            game_state.snake_direction = UP;
        }
        if (game_state.input->down)
        {
            game_state.snake_direction = DOWN;
        }
        break;
    }
    // TEST INPUT
    PDEBUG("Snake facing direction %d", game_state.snake_direction);
}