#ifndef BASE_RENDERER_H
#define BASE_RENDERER_H

#include <memory>
#include "../scene/Scene.h"
#include "../scene/Camera.h"
#include "../scene/lights.h"

//excpecting glfwWindow to be included in the main.cpp
class GLFWwindow;

class BaseRenderer
{
    public:
        BaseRenderer(){}
        virtual ~BaseRenderer() {}
        virtual void RecreateSceneResources(Scene *scene){}
        //virtual void RecreateSwapchainResources(vk::Extent2D viewportExtent, size_t inFlightFramesCount){}

        //virtual void RenderFrame(const legit::InFlightQueue::FrameInfo &frameInfo, const Camera &camera, const Camera &light, Scene *scene, GLFWwindow *window){}
        virtual void RenderFrame(const Camera &camera, Scene *scene, GLFWwindow *window){}
        virtual void ReloadShaders(){}
        virtual void ChangeView(){}
};

#endif