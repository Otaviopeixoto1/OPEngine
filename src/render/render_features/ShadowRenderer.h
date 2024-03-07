#ifndef SHADOW_RENDER_FEATURE_H
#define SHADOW_RENDER_FEATURE_H

#include "RenderFeature.h"



// Cascade partitioning scheme was Based on: https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf


//Add enum containing all shadow mapping techniques to select from

class CascadedShadowRenderer : public RenderFeature
{
    public:
        float seamCorrection = 0.4f;
        
        // This parameter multiplies the size of each frustrum in the CSM
        float zMult = 4.0f;

        CascadedShadowRenderer(){}

        CascadedShadowRenderer(float camNear, float camFar, unsigned int shadowBufferBinding, unsigned int cascadeCount, unsigned int ShadowWidth, unsigned int ShadowHeight)
        {
            this->cameraNear = camNear;
            this->cameraFar = camFar;

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

            
            // Currently only one of the lights can generate an output target
            shadowMaps.push_back(shadowMapBuffer0);
            



            for (size_t i = 0; i <= 4; i++)
            {
                
                if (i < SHADOW_CASCADE_COUNT + 1)
                {
                    frustrumCuts[i] = (1-seamCorrection)*cameraNear * pow(cameraFar/cameraNear, i/(float)SHADOW_CASCADE_COUNT)
                                        + seamCorrection*(cameraNear + (i/(float)SHADOW_CASCADE_COUNT) * (cameraFar - cameraNear));
                }
                else 
                {
                    // if we have less than 4 cascades, the other frustrum cuts should be outsizde the range of
                    // the camera (this is for shader calculations)
                    frustrumCuts[i] = cameraFar * 1.1f;
                }
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
                ShadowBufferSize += sizeof(frustrumCuts);
            }

            glBindBuffer(GL_UNIFORM_BUFFER, ShadowsUBO);
            glBufferData(GL_UNIFORM_BUFFER, ShadowBufferSize, NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            glBindBufferRange(GL_UNIFORM_BUFFER, GLOBAL_SHADOWS_BINDING, ShadowsUBO, 0, ShadowBufferSize);

            //Shadow map generation shader:
            shadowDepthPass = StandardShader(BASE_DIR"/data/shaders/shadow_mapping/layeredVert.vert", BASE_DIR"/data/shaders/nullFrag.frag");
            {
                std::string s = "SHADOW_CASCADE_COUNT " + std::to_string(SHADOW_CASCADE_COUNT);
                shadowDepthPass.AddPreProcessorDefines(&s,1);
            }
            shadowDepthPass.AddShaderStage(BASE_DIR"/data/shaders/shadow_mapping/layeredGeom.geom",GL_GEOMETRY_SHADER);
            shadowDepthPass.BuildProgram();
            shadowDepthPass.BindUniformBlock("Shadows", GLOBAL_SHADOWS_BINDING);
        }
        
        // Add a frameResources as as struct for input!!. ADD the scene reference. viewport reference. Matrices reference
        //Returns a set of output textures
        std::vector<unsigned int> Render(const BaseRenderer::FrameResources& frameResources)
        {
            if (frameResources.lightData->numDirLights == 0)
            {
                return std::vector<unsigned int>(1, 0);
            }

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
                auto frustrumCorners = frameResources.camera->GetFrustumCornersWorldSpace(frustrumCuts[i], frustrumCuts[i+1], frameResources.viewMatrix);

                glm::vec3 center = glm::vec3(0, 0, 0);
                for (size_t j = 0; j < frustrumCorners.size(); j++)
                {
                    center += glm::vec3(frustrumCorners[j]);
                }
                center /= frustrumCorners.size();
                
                const auto lightView = glm::lookAt(
                    center + lightDir,
                    center,
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );

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
                
                glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

                // multiply by the inverse projection matrix
                glm::mat4 lightMatrix = lightProjection * lightView;

                //std::cout << "render shadows set parameter: " << glGetError() << std::endl; 
                glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(glm::mat4), glm::value_ptr(lightMatrix));
                offset += sizeof(glm::mat4);
            }
            //std::cout << "render start render: " << glGetError() << std::endl; 
            // these dont have to be set every frame
            glBufferSubData(GL_UNIFORM_BUFFER, offset, 4 * sizeof(float), &frustrumCuts[1]);
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

            return shadowMaps;
        }


        //callback used when there are viewport resizes
        void ViewportUpdate(int vpWidth, int vpHeight)
        {
            
        }

    private:
        float cameraNear;
        float cameraFar;

        unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
        unsigned int SHADOW_CASCADE_COUNT;
        unsigned int shadowMapFBO;
        unsigned int shadowMapBuffer0;

        std::vector<unsigned int> shadowMaps;

        unsigned int GLOBAL_SHADOWS_BINDING = 4;
        unsigned int ShadowsUBO;
        unsigned int ShadowBufferSize;
        float frustrumCuts[5];
        StandardShader shadowDepthPass;
};

#endif