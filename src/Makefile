../bin/main: main.c ../res/shaders/bin/basic.vert.spv ../res/shaders/bin/basic.frag.spv
	clang -std=c99 -Wall -Wextra -Werror -g -o ../bin/main main.c -lglfw -lvulkan

run: ../bin/main
	../bin/main

../res/shaders/bin/basic.vert.spv: ../res/shaders/basic.vert.glsl
	glslangValidator -V ../res/shaders/basic.vert.glsl -o ../res/shaders/bin/basic.vert.spv

../res/shaders/bin/basic.frag.spv: ../res/shaders/basic.frag.glsl
	glslangValidator -V ../res/shaders/basic.frag.glsl -o ../res/shaders/bin/basic.frag.spv
