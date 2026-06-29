#!/bin/bash

../slang/bin/slangc src/shader.slang -target spirv -profile spirv_1_6 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertex_main -entry fragment_main -o data/shaders/shader.spv
