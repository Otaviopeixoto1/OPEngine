
#ifndef RENDER_FEATURE_H
#define RENDER_FEATURE_H

////////////////////////////////////////////////////
//DEPRECATED
//render features no longer inherit from this class
////////////////////////////////////////////////////

#include <memory>
#include "../../scene/Scene.h"
#include "../../scene/Camera.h"
#include "../../scene/lights.h"
#include "../../common/MathUtils.h"
#include "../BaseRenderer.h"

//excpecting glfwWindow to be included in the main.cpp
class GLFWwindow;

class RenderFeature 
{
    public:
        RenderFeature(){}
        virtual ~RenderFeature() {}
        virtual void RecreateResources(){}

        //Executes the Feature and returns a set of output textures
        virtual std::vector<unsigned int> Render(const BaseRenderer::FrameResources& frameResources){}


        //callback used when there are viewport resizes
        virtual void ViewportUpdate(int vpWidth, int vpHeight){}
};

#endif