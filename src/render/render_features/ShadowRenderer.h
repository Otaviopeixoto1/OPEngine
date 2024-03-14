#ifndef SHADOW_RENDER_FEATURE_H
#define SHADOW_RENDER_FEATURE_H


#include <memory>
#include "../../scene/Scene.h"
#include "../../scene/Camera.h"
#include "../../scene/lights.h"
#include "../../common/MathUtils.h"
#include "../BaseRenderer.h"

/*
struct PCFShadowsInput
{
    GLuint shadowUBOBinding;
};*/

//The same output for all shadow renderers
struct ShadowsOutput
{
    // Currently only one of the lights can generate an output target
    GLuint shadowMap0;
    GLenum texType0;
};

// Cascade partitioning scheme was Based on: https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
//Add enum containing all shadow mapping techniques to select from

class PCFShadowRenderer
{
    public:
        float seamCorrection = 0.4f;
        
        // This parameter multiplies the size of each frustrum in the CSM
        float zMult = 4.0f;

        PCFShadowRenderer(){}

        PCFShadowRenderer(unsigned int shadowBufferBinding, unsigned int cascadeCount, unsigned int ShadowWidth, unsigned int ShadowHeight)
        {
            this->GLOBAL_SHADOWS_BINDING = shadowBufferBinding;
            
            // MAX CASCADE COUNT = 4 
            this->SHADOW_CASCADE_COUNT = std::min(cascadeCount, 4u);
            this->SHADOW_WIDTH = ShadowWidth;
            this->SHADOW_HEIGHT = ShadowHeight;
        }
        
        void RecreateResources()
        {
            
            glGenFramebuffers(1, &shadowMapFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
            glGenTextures(1, &shadowMapBuffer0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffer0);

            glTexImage3D(
                GL_TEXTURE_2D_ARRAY,
                0,
                GL_DEPTH_COMPONENT32, // GL_DEPTH_COMPONENT32F
                SHADOW_WIDTH,
                SHADOW_HEIGHT,
                SHADOW_CASCADE_COUNT,
                0,
                GL_DEPTH_COMPONENT,
                GL_FLOAT,
                nullptr
            );
            
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            //Settings for PCF comparissons
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);    

            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMapBuffer0, 0);

            glDrawBuffer(GL_NONE); //we wont draw to any color buffer
            glReadBuffer(GL_NONE); //we wont read from any color buffer

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw BaseRenderer::RendererException("ERROR::FRAMEBUFFER:: shadowMap Framebuffer is incomplete");
            }


            glGenBuffers(1, &ShadowsUBO);

            // Shadows buffer setup
            // --------------------
            /* Shadows Uniform buffer structure:
             *layout(std140) uniform Shadows
             *{
             *    float shadowBias;
             *    float shadowSamples;
             *    float numShadowCasters;
             *    float spad3;
             *    float frustrumDistances[SHADOW_CASCADE_COUNT + 1];
             *    mat4 lightSpaceMatrices[SHADOW_CASCADE_COUNT];
             *    
             *}; 
             */
            ShadowBufferSize = 0;
            {
                ShadowBufferSize += 4 * sizeof(float);
                ShadowBufferSize += SHADOW_CASCADE_COUNT * sizeof(glm::mat4);
                ShadowBufferSize += sizeof(frustumCuts);
            }

            glBindBuffer(GL_UNIFORM_BUFFER, ShadowsUBO);
            glBufferData(GL_UNIFORM_BUFFER, ShadowBufferSize, NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            glBindBufferRange(GL_UNIFORM_BUFFER, GLOBAL_SHADOWS_BINDING, ShadowsUBO, 0, ShadowBufferSize);

            //Shadow map generation shader:
            shadowDepthPass = StandardShader(BASE_DIR"/data/shaders/shadows/shadow_mapping/layeredVert.vert", BASE_DIR"/data/shaders/nullFrag.frag");
            {
                std::string s = "SHADOW_CASCADE_COUNT " + std::to_string(SHADOW_CASCADE_COUNT);
                shadowDepthPass.AddPreProcessorDefines(&s,1);
            }
            shadowDepthPass.AddShaderStage(BASE_DIR"/data/shaders/shadows/shadow_mapping/layeredGeom.geom",GL_GEOMETRY_SHADER);
            shadowDepthPass.BuildProgram();
            shadowDepthPass.BindUniformBlock("Shadows", GLOBAL_SHADOWS_BINDING);
        }

        void SetupFrustumCuts(float camNear, float camFar)
        {
            for (size_t i = 0; i <= 4; i++)
            {
                
                if (i < SHADOW_CASCADE_COUNT + 1)
                {
                    frustumCuts[i] = (1-seamCorrection)*camNear * pow(camFar/camNear, i/(float)SHADOW_CASCADE_COUNT)
                                        + seamCorrection*(camNear + (i/(float)SHADOW_CASCADE_COUNT) * (camFar - camNear));
                }
                else 
                {
                    // if we have less than 4 cascades, the other frustrum cuts should be outsizde the range of
                    // the camera (this is for shader calculations)
                    frustumCuts[i] = camFar * 1.1f;
                }
            }
        }
        
        // Add a frameResources as as struct for input!!. ADD the scene reference. viewport reference. Matrices reference
        //Returns a set of output textures
        ShadowsOutput Render(const BaseRenderer::FrameResources& frameResources)
        {
            ShadowsOutput out = {0, GL_TEXTURE_2D};

            if (frameResources.lightData->numDirLights == 0)
            {
                return out;
            }

            SetupFrustumCuts(frameResources.camera->Near,frameResources.camera->Far);

            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);

            auto mainLight = frameResources.lightData->directionalLights[0];
            glm::vec3 lightDir = glm::normalize(glm::vec3(frameResources.inverseViewMatrix * mainLight.lightDirection));

            // Setting Shadow propeties:
            glBindBuffer(GL_UNIFORM_BUFFER, ShadowsUBO); 

            // Shadow parameters:
            // 0 -> float shadowBias;
            // 1 -> float shadowSamples;
            // 2 -> float numShadowCasters;
            // 3 -> float spad3;
            float shadowParams[4] = {0.1,1.0,2.0,12.0};

            //these dont have to be set every frame
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 4 * sizeof(float), &shadowParams);
            

            int offset = 0;
            offset += 4 * sizeof(float);
            
            for (size_t i = 0; i < SHADOW_CASCADE_COUNT; i++)
            {
                auto frustrumCorners = frameResources.camera->GetFrustumCornersWorldSpace(frustumCuts[i], frustumCuts[i+1]);

                glm::vec3 center = glm::vec3(0, 0, 0);
                for (size_t j = 0; j < frustrumCorners.size(); j++)
                {
                    center += glm::vec3(frustrumCorners[j]);
                }
                center /= frustrumCorners.size();
                auto camPos = frameResources.camera->GetPosition();
                
                auto v0 = glm::vec3(frustrumCorners[0]);
                auto v1 = glm::vec3(frustrumCorners[7]);
                
                float boundingRadius = glm::length(v0 - v1)/1.41421f;

                
                //we must transform the center the light viewport coordinates in order to snap the movements to the closest texels:
                float texelsPerUnit = ((float)SHADOW_WIDTH)/(boundingRadius * 2.0f);//assuming width = height

                

                glm::mat4 lightViewportMatrix = glm::mat4(1);
                lightViewportMatrix = glm::scale(lightViewportMatrix, glm::vec3(texelsPerUnit,texelsPerUnit,texelsPerUnit));
                lightViewportMatrix = glm::lookAt(glm::vec3(0.0f), lightDir, glm::vec3(0.0f, 1.0f, 0.0f)) * lightViewportMatrix;
                glm::mat4 invLightViewportMatrix = glm::inverse(lightViewportMatrix);

                center = lightViewportMatrix * glm::vec4(center,1.0f);
                center.x = (float)floor(center.x);   //snap to texel
                center.y = (float)floor(center.y);   //snap to texel
                center = invLightViewportMatrix * glm::vec4(center,1.0f);
                
                
                const auto lightView = glm::lookAt(
                    center + lightDir * 2.0f * boundingRadius, //changing the origin for the z buffer values
                    center,
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );

                // Simple bounding box:
                /*
                float minX = std::numeric_limits<float>::max();
                float maxX = std::numeric_limits<float>::lowest();
                float minY = std::numeric_limits<float>::max();
                float maxY = std::numeric_limits<float>::lowest();
                float minZ = std::numeric_limits<float>::max();
                float maxZ = std::numeric_limits<float>::lowest();

                for (const auto& v : frustrumCorners)
                {
                    const auto trf = lightView * v;
                    minX = std::min(minX, trf.x);
                    maxX = std::max(maxX, trf.x);
                    minY = std::min(minY, trf.y);
                    maxY = std::max(maxY, trf.y);
                    minZ = std::min(minZ, trf.z);
                    maxZ = std::max(maxZ, trf.z);
                }


                if (minZ < 0)
                {
                    minZ *= zMult;
                }
                else
                {
                    minZ /= zMult;
                }
                if (maxZ < 0)
                {
                    maxZ /= zMult;
                }
                else
                {
                    maxZ *= zMult;
                }
                */
                
                /*
                glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);*/
                glm::mat4 lightProjection = glm::ortho(-boundingRadius, boundingRadius, -boundingRadius, boundingRadius, -boundingRadius*zMult, boundingRadius * zMult);


                // multiply by the inverse projection matrix
                glm::mat4 lightMatrix = lightProjection * lightView;

                //std::cout << "render shadows set parameter: " << glGetError() << std::endl; 
                glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(glm::mat4), glm::value_ptr(lightMatrix));
                offset += sizeof(glm::mat4);
            }

            glBufferSubData(GL_UNIFORM_BUFFER, offset, 4 * sizeof(float), &frustumCuts[1]);
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 
            

            // Rendering objects to shadow map:
            
            //glCullFace(GL_FRONT); //(Front face culling avoids self shadowing/Acne)

            shadowDepthPass.UseProgram();
            
            frameResources.scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {
                if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    return;
                }

                shadowDepthPass.SetMat4("modelMatrix", objectToWorld);

                //bind VAO
                mesh->BindBuffers();

                //Indexed drawing
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });    

            glViewport(0, 0, frameResources.viewportWidth, frameResources.viewportHeight);
            //glCullFace(GL_BACK);
            out.shadowMap0 = shadowMapBuffer0;
            out.texType0 = GL_TEXTURE_2D_ARRAY;
            return out;
        }

        

    private:
        float cameraNear;
        float cameraFar;

        unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
        unsigned int SHADOW_CASCADE_COUNT;
        GLuint shadowMapFBO;
        GLuint shadowMapBuffer0;


        GLuint GLOBAL_SHADOWS_BINDING = 4;
        GLuint ShadowsUBO;
        unsigned int ShadowBufferSize;
        float frustumCuts[5];
        StandardShader shadowDepthPass;
};




/*Todo: 
    use MSAA, mipmaps and anisotropic filtering  for the shadowmap rendering
    test shadow map array blurring in: 1) one pass for each layer (more draw calls) or 2) one pass for all layers (more texture samples)
 */

class VarianceShadowRenderer 
{
    public:
        float seamCorrection = 0.4f;
        unsigned int gaussianBlurKernelSize = 5;
        
        // This parameter multiplies the size of each frustrum in the CSM
        float zMult = 4.0f;

        VarianceShadowRenderer(){}

        VarianceShadowRenderer(unsigned int shadowBufferBinding, unsigned int cascadeCount, unsigned int ShadowWidth, unsigned int ShadowHeight)
        {
            this->GLOBAL_SHADOWS_BINDING = shadowBufferBinding;
            
            // MAX CASCADE COUNT = 4 
            this->SHADOW_CASCADE_COUNT = std::min(cascadeCount, 4u);
            this->SHADOW_WIDTH = ShadowWidth;
            this->SHADOW_HEIGHT = ShadowHeight;
        }
        
        void RecreateResources()
        {
            glGenFramebuffers(1, &shadowMapFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
            glGenTextures(1, &shadowMapBuffer0); //buffer texture for the actual VSM
            glGenTextures(1, &depthBuffer);      //buffer for depth testing during the shadow pass

            // VSM texture:
            //glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffer0);
            glBindTexture(GL_TEXTURE_2D, shadowMapBuffer0);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RG, GL_FLOAT, 0);
            
            /*
            glTexImage3D(
                GL_TEXTURE_2D_ARRAY,
                0,
                GL_RG32F, 
                SHADOW_WIDTH,
                SHADOW_HEIGHT,
                SHADOW_CASCADE_COUNT,
                0,
                GL_RG,
                GL_FLOAT,
                nullptr
            );*/

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            /*
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);*/

            // Settings for PCF comparissons:
            //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            /////////////////////////////////////////////////////////////////////////////////////////

            /*
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);   */ 

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 




            // Depth buffer:
            //glBindTexture(GL_TEXTURE_2D_ARRAY, depthBuffer);
            glBindTexture(GL_TEXTURE_2D, depthBuffer);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
            /*
            glTexImage3D(
                GL_TEXTURE_2D_ARRAY,
                0,
                GL_DEPTH_COMPONENT32, // GL_DEPTH_COMPONENT32F
                SHADOW_WIDTH,
                SHADOW_HEIGHT,
                SHADOW_CASCADE_COUNT,
                0,
                GL_DEPTH_COMPONENT,
                GL_FLOAT,
                nullptr
            );*/
            
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, shadowMapBuffer0, 0);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthBuffer, 0);


            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw BaseRenderer::RendererException("ERROR::FRAMEBUFFER:: vsm Framebuffer is incomplete");
            }


            
            // Gaussian Blur pass
            glGenFramebuffers(2, blurPassFBOs);
            glGenTextures(2, blurredDepthTex);

            for (size_t i = 0; i < 2; i++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, blurPassFBOs[i]);            
                //THIS NEEDS TO BE REPLACED BY ANOTHER ARRAY TEXTURE, but for now we will only use one single cascade
                glBindTexture(GL_TEXTURE_2D, blurredDepthTex[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RG, GL_FLOAT, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, blurredDepthTex[i], 0);
                

                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    throw BaseRenderer::RendererException("ERROR::FRAMEBUFFER:: blur Framebuffer is incomplete");
                }
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Currently only one of the lights can generate an output target
            shadowMaps.push_back(blurredDepthTex[1]);


            glGenBuffers(1, &ShadowsUBO);

            // Shadows buffer setup
            // --------------------
            /* Shadows Uniform buffer structure:
             *layout(std140) uniform Shadows
             *{
             *    float shadowBias;
             *    float shadowSamples;
             *    float numShadowCasters;
             *    float spad3;
             *    float frustrumDistances[SHADOW_CASCADE_COUNT + 1];
             *    mat4 lightSpaceMatrices[SHADOW_CASCADE_COUNT];
             *    
             *}; 
             */
            ShadowBufferSize = 0;
            {
                ShadowBufferSize += 4 * sizeof(float);
                ShadowBufferSize += SHADOW_CASCADE_COUNT * sizeof(glm::mat4);
                ShadowBufferSize += sizeof(frustumCuts);
            }

            glBindBuffer(GL_UNIFORM_BUFFER, ShadowsUBO);
            glBufferData(GL_UNIFORM_BUFFER, ShadowBufferSize, NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            glBindBufferRange(GL_UNIFORM_BUFFER, GLOBAL_SHADOWS_BINDING, ShadowsUBO, 0, ShadowBufferSize);

            //Shadow map generation shader:
            VSMShadowPass = StandardShader(BASE_DIR"/data/shaders/shadows/shadow_mapping/layeredVert.vert", BASE_DIR"/data/shaders/shadows/VSM/vsm.frag");
            {
                std::string s = "SHADOW_CASCADE_COUNT " + std::to_string(SHADOW_CASCADE_COUNT);
                VSMShadowPass.AddPreProcessorDefines(&s,1);
            }
            VSMShadowPass.AddShaderStage(BASE_DIR"/data/shaders/shadows/shadow_mapping/layeredGeom.geom",GL_GEOMETRY_SHADER);
            VSMShadowPass.BuildProgram();
            VSMShadowPass.BindUniformBlock("Shadows", GLOBAL_SHADOWS_BINDING);


            glGenVertexArrays(1, &screenQuadVAO);
            glGenBuffers(1, &screenQuadVBO);
            glBindVertexArray(screenQuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

            GaussianBlurPass = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/common/gaussianBlur.frag");
            GaussianBlurPass.BuildProgram();
            GaussianBlurPass.UseProgram();
            GaussianBlurPass.SetSamplerBinding("image", 0);
        }

        void SetupFrustumCuts(float camNear, float camFar)
        {
            for (size_t i = 0; i <= 4; i++)
            {
                
                if (i < SHADOW_CASCADE_COUNT + 1)
                {
                    frustumCuts[i] = (1-seamCorrection)*camNear * pow(camFar/camNear, i/(float)SHADOW_CASCADE_COUNT)
                                        + seamCorrection*(camNear + (i/(float)SHADOW_CASCADE_COUNT) * (camFar - camNear));
                }
                else 
                {
                    // if we have less than 4 cascades, the other frustrum cuts should be outsizde the range of
                    // the camera (this is for shader calculations)
                    frustumCuts[i] = camFar * 1.1f;
                }
            }
        }
        
        // Add a frameResources as as struct for input!!. ADD the scene reference. viewport reference. Matrices reference
        //Returns a set of output textures
        ShadowsOutput Render(const BaseRenderer::FrameResources& frameResources)
        {
            ShadowsOutput out = {0, GL_TEXTURE_2D};

            if (frameResources.lightData->numDirLights == 0)
            {
                return out;
            }

            SetupFrustumCuts(frameResources.camera->Near,frameResources.camera->Far);

            auto mainLight = frameResources.lightData->directionalLights[0];
            glm::vec3 lightDir = glm::normalize(glm::vec3(frameResources.inverseViewMatrix * mainLight.lightDirection));

            // Filling the shadows UBO:
            glBindBuffer(GL_UNIFORM_BUFFER, ShadowsUBO); 

            // Shadow parameters:
            // 0 -> float shadowBias;
            // 1 -> float shadowSamples;
            // 2 -> float numShadowCasters;
            // 3 -> float spad3;
            float shadowParams[4] = {0.1,1.0,2.0,12.0};

            //these dont have to be set every frame
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 4 * sizeof(float), &shadowParams);
            int offset = 0;
            offset += 4 * sizeof(float);
            
            for (size_t i = 0; i < SHADOW_CASCADE_COUNT; i++)
            {
                auto frustrumCorners = frameResources.camera->GetFrustumCornersWorldSpace(frustumCuts[i], frustumCuts[i+1]);

                glm::vec3 center = glm::vec3(0, 0, 0);
                for (size_t j = 0; j < frustrumCorners.size(); j++)
                {
                    center += glm::vec3(frustrumCorners[j]);
                }
                center /= frustrumCorners.size();


                float boundingRadius = glm::length(frustrumCorners[0] - frustrumCorners[7])/1.41421f;

                //we must transform the center the light viewport coordinates in order to snap the movements to the closest texels:
                float texelsPerUnit = ((float)SHADOW_WIDTH)/(boundingRadius * 2.0f);//assuming width = height

                glm::mat4 lightViewportMatrix = glm::mat4(1);
                lightViewportMatrix = glm::scale(lightViewportMatrix, glm::vec3(texelsPerUnit,texelsPerUnit,texelsPerUnit));
                lightViewportMatrix = glm::lookAt(glm::vec3(0.0f), lightDir, glm::vec3(0.0f, 1.0f, 0.0f)) * lightViewportMatrix;
                glm::mat4 invLightViewportMatrix = glm::inverse(lightViewportMatrix);

                center = lightViewportMatrix * glm::vec4(center,1.0f);
                center.x = (float)floor(center.x);   //snap to texel
                center.y = (float)floor(center.y);   //snap to texel
                center = invLightViewportMatrix * glm::vec4(center,1.0f);

                
                const auto lightView = glm::lookAt(
                    center + lightDir,
                    center,
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );
                glm::mat4 lightProjection = glm::ortho(-boundingRadius, boundingRadius, -boundingRadius, boundingRadius, -boundingRadius * zMult, boundingRadius * zMult);
                glm::mat4 lightMatrix = lightProjection * lightView;

                glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(glm::mat4), glm::value_ptr(lightMatrix));
                offset += sizeof(glm::mat4);
            }
            glBufferSubData(GL_UNIFORM_BUFFER, offset, 4 * sizeof(float), &frustumCuts[1]);
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 
            
            
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // Rendering the shadowMap
            VSMShadowPass.UseProgram();
            
            frameResources.scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {
                if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    return;
                }

                VSMShadowPass.SetMat4("modelMatrix", objectToWorld);

                mesh->BindBuffers();
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });    


            //Blur Pass
            GLuint kernelSizeIndex = 1;

            glBindFramebuffer(GL_FRAMEBUFFER, blurPassFBOs[0]);
            glClear(GL_COLOR_BUFFER_BIT);
            
            GaussianBlurPass.UseProgram();
            GaussianBlurPass.SetVec2("direction", 1.0f, 0.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, shadowMapBuffer0);
            
            glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &kernelSizeIndex);

            glBindVertexArray(screenQuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);



            glBindFramebuffer(GL_FRAMEBUFFER, blurPassFBOs[1]);
            glClear(GL_COLOR_BUFFER_BIT);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, blurredDepthTex[0]);

            GaussianBlurPass.UseProgram();
            GaussianBlurPass.SetVec2("direction", 0.0f, 1.0f);

            glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &kernelSizeIndex);

            glBindVertexArray(screenQuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);


            glViewport(0, 0, frameResources.viewportWidth, frameResources.viewportHeight);

            out.shadowMap0 = shadowMapBuffer0;
            out.texType0 = GL_TEXTURE_2D;
            return out;
        }


        //callback used when there are viewport resizes
        void ViewportUpdate(int vpWidth, int vpHeight)
        {
            
        }

    private:
        std::vector<unsigned int> shadowMaps;

        float cameraNear;
        float cameraFar;

        unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
        unsigned int SHADOW_CASCADE_COUNT;

        StandardShader VSMShadowPass;
        StandardShader GaussianBlurPass;

        GLuint shadowMapFBO;
        GLuint shadowMapBuffer0;
        GLuint depthBuffer;

        GLuint blurPassFBOs[2];
        GLuint blurredDepthTex[2];


        unsigned int screenQuadVAO, screenQuadVBO;
        float quadVertices[24] = 
        {   // vertex attributes for a quad that fills the entire screen 
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
            1.0f, -1.0f,  1.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f
        };
        
    
        GLuint GLOBAL_SHADOWS_BINDING = 4;
        GLuint ShadowsUBO;
        unsigned int ShadowBufferSize;
        float frustumCuts[5];
};




#endif