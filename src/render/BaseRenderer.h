#ifndef BASE_RENDERER_H
#define BASE_RENDERER_H

#include <memory>
#include "../scene/Scene.h"
#include "../scene/Camera.h"
#include "../scene/lights.h"
#include "../common/MathUtils.h"
#include "../common/ShaderMemoryPool.h"
#include "../debug/OPProfiler.h"
#include "../gl/Texture.h"



//excpecting glfwWindow to be included in the main.cpp
class GLFWwindow;

class BaseRenderer
{
    public:
        BaseRenderer(){}
        virtual ~BaseRenderer() {}
        virtual void RecreateResources(Scene &scene, Camera &camera, GLFWwindow *window){}
        virtual void RenderFrame(Camera &camera, Scene *scene, GLFWwindow *window, OPProfiler::OPProfiler *profiler){}
        virtual void RenderGUI(){}
        virtual void ReloadShaders(){}
        virtual void ChangeView(){}

        //callback used when there are viewport resizes
        virtual void ViewportUpdate(int vpWidth, int vpHeight){}
        
        class RendererException: public std::exception
        {
            std::string message;
            public:
                RendererException(const std::string &message)
                {
                    this->message = message;
                }
                virtual const char* what() const throw()
                {
                    return message.c_str();
                }
        };
        
        struct FrameResources
        {
            unsigned int viewportWidth;
            unsigned int viewportHeight;

            Scene *scene;
            Camera *camera;
            GlobalLightData *lightData;

            glm::mat4 projectionMatrix;
            glm::mat4 viewMatrix;
            glm::mat4 inverseViewMatrix;

            ShaderMemoryPool *shaderMemoryPool;
        }; 
        

    protected:
        ShaderMemoryPool shaderMemoryPool;
        
};




#endif