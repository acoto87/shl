#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include <glew.h>

#define GLFW_DLL
#include <GLFW/glfw3.h>

#include "cglm.h"

#include "nanovg.c"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"

static void glfwErrorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %d, %s\n", error, description);
}

int main(int argc, char *argv[]) 
{
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit())
    {
        printf("Error initializing GLFW!");
        return -1;
    }

    uint32_t windowWidth = 600;
    uint32_t windowHeight = 400;
    uint32_t fbWidth, fbHeight;
    char *windowTitle = "Canvas 2d Test";

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "GLFW window could not be created!");
        glfwTerminate();
        return -1;
    }

    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK)
    {
        fprintf(stderr, "Error initializing GLEW! %s\n", glewGetErrorString(glewError));
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll consume it here.
	glGetError();

    NVGcontext *vg = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!vg) {
		fprintf(stderr, "Could not init nanovg.\n");
		return -1;
	}

    glViewport(0, 0, fbWidth, fbHeight);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    float time = 0;
    float deltaTime = 0;
    uint32_t fps = 0;

    while (!glfwWindowShouldClose(window))
    {
        sprintf(windowTitle, "War 1: %.2f at %d fps", time, fps);
        glfwSetWindowTitle(window, windowTitle);

        // input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
            continue;
        }

        // Update and render
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        float pxRatio = (float)fbWidth / (float)windowWidth;

	    glViewport(0, 0, fbWidth, fbHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, windowWidth, windowHeight, pxRatio);

        nvgBeginPath(vg);
        nvgRect(vg, 100, 100, 200, 200);
        // nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
        // nvgFill(vg);

        nvgStrokeWidth(vg, 2.0f);
        nvgStrokeColor(vg, nvgRGBA(255, 0, 0, 255));
        nvgStroke(vg);

        nvgEndFrame(vg);

        // swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

        float currentTime = glfwGetTime();
        deltaTime = (currentTime - time);

        // force to 60 fps
        while (deltaTime <= (1.0f/60.0f))
        {
            currentTime = (float)glfwGetTime();
            deltaTime = (currentTime - time);
        }

        time = currentTime;
        fps = (uint32_t)(1.0f / deltaTime);
    }

    nvgDeleteGL3(vg);
    glfwDestroyWindow(window);
    glfwTerminate();
	return 0;
}