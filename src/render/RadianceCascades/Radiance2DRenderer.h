#ifndef RADIANCE_2D_RENDERER_H
#define RADIANCE_2D_RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <math.h>       
#include "../BaseRenderer.h"
#include "../../debug/OPProfiler.h"
#include "../../common/Colors.h"

// THE RENDERER SHOULD NOT CARE ABOUT THESE CALLBACKS, SET THEM ELSEWHERE

// Make a window class wrapper to handle this instead of making it global
//see: glfwSetWindowUserPointer

//An input handler must be passed to all renderers and customized inside each one
bool mouseJustClicked = false;
bool mouseClicked = false;
bool mouseJustReleased = false;
double mouseClickXPos, mouseClickYPos;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    //Forward the mouse input to the imgui window
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, action);
    if (io.WantCaptureMouse) {return;}

    
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) 
    {
        mouseJustClicked = true;
        mouseClicked = true;

        glfwGetCursorPos(window, &mouseClickXPos, &mouseClickYPos);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) 
    {
        mouseJustReleased = true;
        mouseClicked = false;

        glfwGetCursorPos(window, &mouseClickXPos, &mouseClickYPos);
    }
}



class Radiance2DRenderer : public BaseRenderer
{
    public:
        static constexpr unsigned int MAX_CASCADES = 10;

        glm::vec4 brushColor = glm::vec4(1.0f,0.0f,0.0f,1.0f);
        

        Radiance2DRenderer(unsigned int vpWidth, unsigned int vpHeight)
        {
            //size of the rendered area
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;

            raymarchRegionSize = std::max(vpWidth, vpHeight);   //size of the area filled by cascades
                                                                //this will be a square area that always fits the rendered area inside. 
                                                                //it to simply the process of cascade creation but generates some wasted area in memory
            
            c0Angular  = 4; // angular resolution or initial rays per probe in cascade 0. (keep power of 4 for simplicity)
            c0Interval = 4; // radiance interval or raymarch distance in cascade 0.           
            c0Spacing  = 2; // Initial probe spacing in cascade 0.                        (keep power of 2 for simplicity)
            cascadeStorageSize =  floor(raymarchRegionSize / c0Spacing) * sqrt(c0Angular);

            // Maximum cascade count.
            cascadeCount = std::max((unsigned int)ceil(log2(cascadeStorageSize / sqrt(c0Angular))), MAX_CASCADES);
            
            // Desired cascade count.
            float diagonal = sqrt(2) * raymarchRegionSize;
            cascadeCount = std::min((unsigned int)floor(log2(4.0 * diagonal)/2.0f) - 1, cascadeCount);

            std::cout << "Cascade Region Size: " << raymarchRegionSize << "\n";
            std::cout << "Cascade Storage Size: " << cascadeStorageSize << "\n";
            std::cout << "Cascade Count: " << cascadeCount << "\n";

        }

        void RecreateResources(Scene &scene, Camera &camera, GLFWwindow *window)
        {
            //////////////////////////////////////////////////////////////////////////////////////
            // Add this to a custom window event manager 
            glfwSetMouseButtonCallback(window, mouse_button_callback);
            //////////////////////////////////////////////////////////////////////////////////////

            

            screenQuad = Mesh::QuadMesh();

            shaderMemoryPool.Clear();
            shaderMemoryPool.AddUniformBuffer(sizeof(MouseData), "MouseData");


            
            glGenFramebuffers(2, SdfFBOs);
            glGenTextures(2, sdfBufferTextures);
            glClearColor(1e20f, 0.0, 0.0, 0.0);

            glBindFramebuffer(GL_FRAMEBUFFER, SdfFBOs[0]);

            glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sdfBufferTextures[0], 0);

            // no depth buffer needed
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }
        
            glClear(GL_COLOR_BUFFER_BIT);


            glBindFramebuffer(GL_FRAMEBUFFER, SdfFBOs[1]);

            glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sdfBufferTextures[1], 0);

            // no depth buffer needed
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }

            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.0, 0.0, 0.0, 0.0);



            glGenFramebuffers(cascadeCount + 1, cascadeIntervalFBOs);
            glGenTextures(cascadeCount + 1, cascadeBuffers);

            for (size_t i = 0; i < cascadeCount + 1; i++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, cascadeIntervalFBOs[i]);

                glBindTexture(GL_TEXTURE_2D, cascadeBuffers[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, cascadeStorageSize, cascadeStorageSize, 0, GL_RGBA, GL_FLOAT, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cascadeBuffers[i], 0);

                // no depth buffer needed
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
                }
            }
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            

        }

        void ViewportUpdate(int vpWidth, int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
            
            glClearColor(1e20f, 0.0, 0.0, 0.0);
            currentBuffer = 0;

            glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            glBindFramebuffer(GL_FRAMEBUFFER, SdfFBOs[0]);
            glClear(GL_COLOR_BUFFER_BIT);

            glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            glBindFramebuffer(GL_FRAMEBUFFER, SdfFBOs[1]);
            glClear(GL_COLOR_BUFFER_BIT);

            glClearColor(0.0, 0.0, 0.0, 0.0);

            // RESIZE CASCADES
            raymarchRegionSize = std::max(viewportWidth, viewportHeight);
            cascadeStorageSize =  floor(raymarchRegionSize / c0Spacing) * sqrt(c0Angular);
            cascadeCount = std::max((unsigned int)ceil(log2(cascadeStorageSize / sqrt(c0Angular))), MAX_CASCADES);
            float diagonal = sqrt(2) * raymarchRegionSize;
            cascadeCount = std::min((unsigned int)floor(log2(4.0 * diagonal)/2.0f) - 1, cascadeCount);

            for (size_t i = 0; i < cascadeCount + 1; i++)
            {
                glBindTexture(GL_TEXTURE_2D, cascadeBuffers[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, cascadeStorageSize, cascadeStorageSize, 0, GL_RGBA, GL_FLOAT, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

        }


        void RenderFrame(Camera &camera, Scene *scene, GLFWwindow *window, OPProfiler::OPProfiler *profiler)
        {

            // Lazy mouse brush, based on: https://lazybrush.dulnan.net/
            if (mouseClicked)
            {
                if (!drawing)
                {
                    drawing = true;

                    mouseA.x = mouseClickXPos/(float)viewportWidth;
                    mouseA.y = 1.0f - mouseClickYPos/(float)viewportHeight;

                    mouseB.x = mouseClickXPos/(float)viewportWidth;
                    mouseB.y = 1.0f - mouseClickYPos/(float)viewportHeight;

                    mouseC.x = mouseClickXPos/(float)viewportWidth;
                    mouseC.y = 1.0f - mouseClickYPos/(float)viewportHeight;
                }
                else
                {
                    //start rotating the points. 
                    mouseA = mouseB;
                    mouseB = mouseC;

                    double x, y;
                    glfwGetCursorPos(window, &x, &y);
                    mouseC.x = x/(float)viewportWidth;
                    mouseC.y = 1.0f - y/(float)viewportHeight; 
                    
                    float dist = sqrt( (mouseC.x - mouseB.x) * (mouseC.x - mouseB.x) + (mouseC.y - mouseB.y) * (mouseC.y - mouseB.y) );
                    
                    if (dist>0)
                    {
                        glm::vec4 dir = (mouseC - mouseB) / dist;

                        float len = std::max((dist - brushRadius), 0.0f);
                        float ease = 1.0 - pow(friction, 1.0f/60.0f * 10.0); //use deltatime instead
                        mouseC = mouseB + dir * len * ease;
                    }
                    

                    
                }
            }
            else if (drawing)
            {
                drawing = false;
            }
            

            auto captureMouseTask = profiler->AddTask("Capture Mouse", Colors::carrot);
            captureMouseTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, SdfFBOs[currentBuffer]);
            //glClearColor(1e20f, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            
            auto mouseDataBuffer = shaderMemoryPool.GetUniformBuffer("MouseData");
            MouseData *mouseData = mouseDataBuffer->BeginSetData<MouseData>();
            {
                mouseData->mouseA = mouseA;
                mouseData->mouseB = mouseB;
                mouseData->mouseC = mouseC;
                
            }
            mouseDataBuffer->EndSetData();
            
            genSDFShader.UseProgram();
            genSDFShader.SetFloat("brushRadius", brushRadius);
            genSDFShader.SetVec4("brushColor", brushColor);
            screenQuad->BindBuffers();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[1 - currentBuffer]); 
            if(drawing) 
            {
                glDrawArrays(GL_TRIANGLES, 0, 6); 
                currentBuffer = 1 - currentBuffer;
            } 
            
            captureMouseTask->End();



            GLint origViewportSize[4];
            glGetIntegerv(GL_VIEWPORT, origViewportSize);
            glViewport(0, 0, cascadeStorageSize, cascadeStorageSize);

            auto marchCascadesTask = profiler->AddTask("March Cascades", Colors::belizeHole);
            marchCascadesTask->Start();

            marchCascadeShader.UseProgram();
            marchCascadeShader.SetVec2("sceneDimensions", glm::vec2(viewportWidth,viewportHeight));
            marchCascadeShader.SetFloat("raymarchRegionSize", raymarchRegionSize);
            marchCascadeShader.SetFloat("cascadeStorageSize", cascadeStorageSize);
            marchCascadeShader.SetFloat("c0Spacing", c0Spacing);
            marchCascadeShader.SetFloat("c0Interval", c0Interval);
            marchCascadeShader.SetFloat("c0Angular", c0Angular);

            for (size_t i = 0; i < cascadeCount; i++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, cascadeIntervalFBOs[i]);
                glClear(GL_COLOR_BUFFER_BIT);

                marchCascadeShader.SetInt("cascadeIndex", i);
                screenQuad->BindBuffers();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[1 - currentBuffer]); 
                glDrawArrays(GL_TRIANGLES, 0, 6); 
            }

            marchCascadesTask->End();



            auto mergeCascades = profiler->AddTask("Merge Cascades", Colors::peterRiver);
            mergeCascades->Start();
            
            mergeCascadeShader.UseProgram();
            mergeCascadeShader.SetFloat("cascadeStorageSize", cascadeStorageSize);
            mergeCascadeShader.SetFloat("c0Angular", c0Angular);
            mergeCascadeShader.SetFloat("cascadeCount", cascadeCount);

            for (size_t i = cascadeCount - 1; i >= 1; i--)
            {
                if (cascadeCount <= 1) {break;}

                glBindFramebuffer(GL_FRAMEBUFFER, cascadeIntervalFBOs[i + 1]); //temp buffer
                glClear(GL_COLOR_BUFFER_BIT);

                mergeCascadeShader.SetInt("cascadeIndex", i - 1 ); // upper cascade index
                screenQuad->BindBuffers();

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cascadeBuffers[i]); // upper cascade // no longer used. swap

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, cascadeBuffers[i-1]); // current cascade

                GLuint tFBO = cascadeIntervalFBOs[i - 1];
                GLuint tTex = cascadeBuffers[i - 1];

                cascadeIntervalFBOs[i - 1] = cascadeIntervalFBOs[i + 1];
                cascadeBuffers[i - 1] = cascadeBuffers[i + 1];

                cascadeIntervalFBOs[i + 1] = tFBO;
                cascadeBuffers[i + 1] = tTex;
                

                glDrawArrays(GL_TRIANGLES, 0, 6); 
            }
            
            mergeCascades->End();



            auto drawSDFTask = profiler->AddTask("Integrate Radiance", Colors::alizarin);

            drawSDFTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(origViewportSize[0], origViewportSize[1], origViewportSize[2], origViewportSize[3]);
            glClear(GL_COLOR_BUFFER_BIT);

            RenderRadianceShader.UseProgram();
            RenderRadianceShader.SetVec2("sceneDimensions", glm::vec2(viewportWidth,viewportHeight));
            RenderRadianceShader.SetFloat("raymarchRegionSize", raymarchRegionSize);
            RenderRadianceShader.SetFloat("cascadeStorageSize", cascadeStorageSize);
            RenderRadianceShader.SetFloat("c0Spacing", c0Spacing);
            RenderRadianceShader.SetFloat("c0Interval", c0Interval);
            RenderRadianceShader.SetFloat("c0Angular", c0Angular);
            screenQuad->BindBuffers();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cascadeBuffers[0]); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

            drawSDFTask->End();  

            // For rendering just the input sdf:
            /*
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            drawSDFShader.UseProgram();
            screenQuad->BindBuffers();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sdfBufferTextures[1 - currentBuffer]); 
            glDrawArrays(GL_TRIANGLES, 0, 6);*/
        }

        void ReloadShaders()
        {   
            auto bufferBindings = shaderMemoryPool.GetNamedBindings();

            genSDFShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/2D/LineDraw/genLineSDF.frag");
            genSDFShader.BuildProgram();
            genSDFShader.BindUniformBlocks(bufferBindings);
            genSDFShader.UseProgram();
            genSDFShader.SetSamplerBinding("prev", 0);


            drawSDFShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/2D/LineDraw/drawLine.frag");
            drawSDFShader.BuildProgram();
            drawSDFShader.UseProgram();
            drawSDFShader.SetSamplerBinding("sdf", 0);


            marchCascadeShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/RadianceCascades/2D/MarchCascade.frag");
            marchCascadeShader.BuildProgram();
            marchCascadeShader.UseProgram();
            marchCascadeShader.SetSamplerBinding("sdf", 0);


            mergeCascadeShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/RadianceCascades/2D/MergeCascades.frag");
            mergeCascadeShader.BuildProgram();
            mergeCascadeShader.UseProgram();
            mergeCascadeShader.SetSamplerBinding("upperCascade", 0);
            mergeCascadeShader.SetSamplerBinding("currentCascade", 1);


            RenderRadianceShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/RadianceCascades/2D/RenderRadiance.frag");
            RenderRadianceShader.BuildProgram();
            RenderRadianceShader.UseProgram();
            RenderRadianceShader.SetSamplerBinding("mergedCascades", 0);
        }

        void RenderGUI()
        {
            ImGui::Begin("Radiance 2D");

            ImGui::ColorPicker4("Brush Color", (float*)&brushColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoDragDrop);
            ImGui::End();
        }

        
    private:
        unsigned int currentBuffer = 0;
        GLuint SdfFBOs[2];
        GLuint sdfBufferTextures[2];

        unsigned int viewportWidth;
        unsigned int viewportHeight;
        unsigned int raymarchRegionSize;
        unsigned int cascadeStorageSize;


        unsigned int cascadeCount;
        unsigned int c0Interval;
        unsigned int c0Spacing;
        unsigned int c0Angular;
        GLuint cascadeBuffers[MAX_CASCADES + 1];      //max cascades + 1 temporary buffer cascade for merging
        GLuint cascadeIntervalFBOs[MAX_CASCADES + 1]; //max cascades + 1 temporary buffer cascade for merging

        bool drawing = false;
        float brushRadius = 0.015f;
        float friction = 0.05f;
        
        glm::vec4 mouseA, mouseB, mouseC = glm::vec4(0.0f);
        std::unique_ptr<Mesh> screenQuad;
        std::vector<std::string> preprocessorDefines;


        #pragma pack(push, 1)
        struct MouseData 
        {
            glm::vec4 mouseA; 
            glm::vec4 mouseB; 
            glm::vec4 mouseC;      
        };
        #pragma pack(pop)

        

        

        StandardShader drawSDFShader;
        StandardShader genSDFShader;
        StandardShader marchCascadeShader;
        StandardShader mergeCascadeShader;
        StandardShader RenderRadianceShader;


        
        
        

};




#endif