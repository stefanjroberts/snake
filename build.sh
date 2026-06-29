#! /bin/bash

LDFLAGS=" -lglfw -lvulkan -ldl -lpthread -lm"

gcc -o bin/main -g src/main.c src/renderer/vulkan.c src/core/logger.c src/core/event.c src/core/memory.c src/core/clock.c src/core/input.c src/game/game.c $LDFLAGS
