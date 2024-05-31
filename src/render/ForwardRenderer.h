#ifndef FORWARD_RENDERER_H
#define FORWARD_RENDERER_H

#include "BaseRenderer.h"
#include "render_features/ShadowRenderer.h"
#include "render_features/SkyRenderer.h"
#include "../debug/OPProfiler.h"
#include "../common/Colors.h"
#include <exception>


class ForwardRenderer : public BaseRenderer
{
    public:
        static constexpr unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
        static constexpr unsigned int SHADOW_CASCADE_COUNT = 2; // MAX == 4
        
        static constexpr bool enableShadowMapping = true;
        static constexpr bool enableNormalMaps = true;

        static constexpr int MAX_DIR_LIGHTS = 3;
        static constexpr int MAX_POINT_LIGHTS = 10;

        unsigned int MSAASamples = 4; 
        float tonemapExposure = 1.0f;


        enum MainPassBufferBindings
        {
            SHADOW_MAP_BUFFER0_BINDING = 0,
            SHADOW_MAP_BUFFER1_BINDING = 1,
            SHADOW_MAP_BUFFER2_BINDING = 2,
            DIFFUSE_TEXTURE0_BINDING = 3,
            NORMAL_TEXTURE0_BINDING = 4,
            SPECULAR_TEXTURE0_BINDING = 5,
        };

        ForwardRenderer(unsigned int vpWidth, unsigned int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
        }

        void RecreateResources(Scene &scene, Camera &camera, GLFWwindow *window)
        {
            screenQuad = Mesh::QuadMesh();

            scene.MAX_DIR_LIGHTS = MAX_DIR_LIGHTS;
            scene.MAX_POINT_LIGHTS = MAX_POINT_LIGHTS;

            shaderMemoryPool.Clear();
            shaderMemoryPool.AddUniformBuffer(sizeof(GlobalMatrices), "GlobalMatrices");
            shaderMemoryPool.AddUniformBuffer(sizeof(LocalMatrices), "LocalMatrices");
            shaderMemoryPool.AddUniformBuffer(sizeof(MaterialProperties), "MaterialProperties");
            shaderMemoryPool.AddUniformBuffer(sizeof(LightData), "LightData");

            preprocessorDefines.clear();
            preprocessorDefines.push_back("MAX_DIR_LIGHTS " + std::to_string(MAX_DIR_LIGHTS));
            preprocessorDefines.push_back("MAX_POINT_LIGHTS " + std::to_string(MAX_POINT_LIGHTS));
            preprocessorDefines.push_back("SHADOW_CASCADE_COUNT " + std::to_string(SHADOW_CASCADE_COUNT));

            if (enableShadowMapping)
            {
                preprocessorDefines.push_back("DIR_LIGHT_SHADOWS");


                this->shadowRenderer = PCFShadowRenderer(
                    SHADOW_CASCADE_COUNT,
                    SHADOW_WIDTH,
                    SHADOW_HEIGHT
                );
                this->shadowRenderer.RecreateResources(&shaderMemoryPool);
                preprocessorDefines.push_back("PCF_SHADOWS");
            }

            this->skyRenderer = SkyRenderer();
            this->skyRenderer.RecreateResources();
        


            //setting up intermediate buffer and screen texture used in the quad
            glGenFramebuffers(1, &intermediateFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

            //creating color attachment of screen texture
            glGenTextures(1, &screenTexture);
            glBindTexture(GL_TEXTURE_2D, screenTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture, 0);	

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }
            // No need for depth or stencil buffers on the intermediate FBO
            glBindFramebuffer(GL_FRAMEBUFFER, 0);




            // Setting MSAA framebuffer
            glEnable(GL_MULTISAMPLE);
            //get the shaders used by the scene
            glGenFramebuffers(1, &multisampledFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, multisampledFBO);

            //setting the multisampled color attachment GL_RGBA16F
            glGenTextures(1, &MSAATextureColorBuffer);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAATextureColorBuffer);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAASamples, GL_RGBA16F, viewportWidth, viewportHeight, GL_TRUE);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, MSAATextureColorBuffer, 0);
            
            //setting the depth and stencil attachments (with multisampling) 
            glGenRenderbuffers(1, &MSAADepthStencilRBO); 
            glBindRenderbuffer(GL_RENDERBUFFER, MSAADepthStencilRBO);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAASamples, GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, MSAADepthStencilRBO);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Framebuffer for MSAA is incomplete");
            }
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
                

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void ViewportUpdate(int vpWidth, int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;

            glBindTexture(GL_TEXTURE_2D, screenTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vpWidth, vpHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0); 
            
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAATextureColorBuffer);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAASamples, GL_RGBA16F, vpWidth, vpHeight, GL_TRUE);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0); 


            glBindRenderbuffer(GL_RENDERBUFFER, MSAADepthStencilRBO);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAASamples, GL_DEPTH24_STENCIL8, vpWidth, vpHeight);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }


        void RenderFrame(Camera &camera, Scene *scene, GLFWwindow *window, OPProfiler::OPProfiler *profiler)
        {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glEnable(GL_DEPTH_TEST);

            glm::mat4 projectionMatrix = camera.GetProjectionMatrix();
            glm::mat4 viewMatrix = camera.GetViewMatrix();
            glm::mat4 inverseViewMatrix = glm::inverse(viewMatrix);

            // Get light data in view space:
            GlobalLightData lights = scene->GetLightData(viewMatrix);

            FrameResources frameResources = FrameResources();
            frameResources.viewportHeight = viewportHeight;
            frameResources.viewportWidth = viewportWidth;
            frameResources.scene = scene;
            frameResources.camera = &camera;
            frameResources.viewMatrix = viewMatrix;
            frameResources.inverseViewMatrix = inverseViewMatrix;
            frameResources.projectionMatrix = projectionMatrix;
            frameResources.lightData = &lights;
            frameResources.shaderMemoryPool = &shaderMemoryPool;



            auto globalMatricesBuffer = shaderMemoryPool.GetUniformBuffer("GlobalMatrices");
            GlobalMatrices *globalMatrices = globalMatricesBuffer->BeginSetData<GlobalMatrices>();
            {
                globalMatrices->projectionMatrix = projectionMatrix;
                globalMatrices->viewMatrix = viewMatrix;
                globalMatrices->inverseViewMatrix = inverseViewMatrix;
            }
            globalMatricesBuffer->EndSetData();

            
            auto lightDataBuffer = shaderMemoryPool.GetUniformBuffer("LightData");
            LightData *lightData = lightDataBuffer->BeginSetData<LightData>();
            {
                lightData->ambientLight = lights.ambientLight;
                lightData->numDirLights = lights.numDirLights;
                lightData->numPointLights = lights.numPointLights;
                lightData->pad = 0;
                lightData->pad2 = 0;

                for (size_t i = 0; i < MAX_DIR_LIGHTS; i++)
                {
                    if (lights.numDirLights < i + 1) {break;}
                    lightData->dirLights[i] = lights.directionalLights[i];
                }
                for (size_t i = 0; i < MAX_POINT_LIGHTS; i++)
                {
                    if (lights.numPointLights < i + 1) {break;}
                    lightData->pointLights[i] = lights.pointLights[i];
                }
                
            }
            lightDataBuffer->EndSetData();

            
            // 1) Shadow Map Rendering Pass:
            // -----------------------------
            auto shadowTask = profiler->AddTask("shadows", Colors::amethyst);
            shadowTask->Start();

            ShadowsOutput shadowOut = {0, GL_TEXTURE_2D};
            if (enableShadowMapping)
            {
                shadowOut = shadowRenderer.Render(frameResources);
            }
            
            shadowTask->End();
                

            // 2) Main Rendering pass:
            // -----------------------
            auto mainPassTask = profiler->AddTask("main pass", Colors::emerald);
            mainPassTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, multisampledFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // -Shadows:
            glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_BUFFER0_BINDING);
            glBindTexture(shadowOut.texType0, shadowOut.shadowMap0);


            int shaderCache = -1;

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                StandardShader activeShader;
                GLuint activeRoutines[2];
                
                bool unlit = false;
                if (materialInstance->HasFlags(OP_MATERIAL_TEXTURED_DIFFUSE | OP_MATERIAL_TEXTURED_NORMAL))
                {
                    activeShader = defaultVertNormalTexFrag;
                    activeRoutines[0] = 1;
                    activeRoutines[1] = 3;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertFrag;
                    activeRoutines[0] = 1;
                    activeRoutines[1] = 3;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    unlit = true;
                    activeShader = defaultVertUnlitFrag;
                }
                else
                {
                    activeShader = defaultVertFrag;
                    activeRoutines[0] = 0;
                    activeRoutines[1] = 2;
                }


                if (activeShader.ID != shaderCache)
                {
                    activeShader.UseProgram();
                    shaderCache = activeShader.ID;
                }

                // setting if the color is sampled from texture or from UBO
                if (!unlit)
                {
                    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, activeRoutines);
                }
                

                auto materialPropertiesBuffer = shaderMemoryPool.GetUniformBuffer("MaterialProperties");
                materialPropertiesBuffer->SetData(0, sizeof(MaterialProperties), &(materialInstance->properties));

                auto localMatricesBuffer = shaderMemoryPool.GetUniformBuffer("LocalMatrices"); 
                localMatricesBuffer->SetData( 0, sizeof(glm::mat4), (void*)glm::value_ptr(objectToWorld));
                localMatricesBuffer->SetData( sizeof(glm::mat4), sizeof(glm::mat4), (void*)glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)) );



                // -Textures
                unsigned int diffuseNr = 1;
                unsigned int specularNr = 1;
                unsigned int normalNr = 1;

                for (unsigned int i = 0; i < materialInstance->numTextures; i++)
                {
                    Texture texture = scene->GetTexture(materialInstance->GetTexturePath(i));
                    // activate proper texture unit (all the shadow maps already reserve bindings 0 -> 2)
                    glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_BUFFER2_BINDING + i); 

                    std::string number;
                    TextureType type = texture.type;
                    std::string name;

                    switch (type)
                    {
                        case OP_TEXTURE_DIFFUSE:
                            number = std::to_string(std::max(diffuseNr++, (unsigned int)NORMAL_TEXTURE0_BINDING));
                            name = "texture_diffuse";
                            break;
                        case OP_TEXTURE_NORMAL:
                            number = std::to_string(std::max(normalNr++, (unsigned int)NORMAL_TEXTURE0_BINDING));
                            name = "texture_normal";
                            break;
                        case OP_TEXTURE_SPECULAR:
                            number = std::to_string(specularNr++); //add limit to specular textures
                            name = "texture_specular";
                            break;
                        
                        
                        default:
                            number = "";
                            name = "texture_unidentified";
                            break;
                    }

                    activeShader.SetSamplerBinding((name + number).c_str(), SHADOW_MAP_BUFFER2_BINDING + i);
                    glBindTexture(GL_TEXTURE_2D, texture.id);
                }
                
                //bind VAO
                mesh->BindBuffers();

                //Indexed drawing
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });  
            
            mainPassTask->End();



            // Rendering skybox
            auto skytask = profiler->AddTask("Render Sky", Colors::clouds);
            skytask->Start();
            this->skyRenderer.Render(frameResources);
            skytask->End();


            // 3) Final Pass (blit + Postprocessing):
            // --------------------------------------
            auto finalTask = profiler->AddTask("Blit & Tonemapping", Colors::carrot);
            finalTask->Start();

            // Blit the MSAA buffer to an intermediate framebuffer for postprocessing:
            glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampledFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediateFBO);
            glBlitFramebuffer(0, 0, viewportWidth, viewportHeight, 0, 0, viewportWidth, viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);


            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            postProcessShader.UseProgram();
            postProcessShader.SetFloat("exposure", tonemapExposure);
            screenQuad->BindBuffers();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, screenTexture); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

            finalTask->End();

        }



        void ReloadShaders()
        {
            auto bufferBindings = shaderMemoryPool.GetNamedBindings();

            simpleDepthPass = StandardShader(BASE_DIR"/data/shaders/simpleVert.vert", BASE_DIR"/data/shaders/nullFrag.frag");
            simpleDepthPass.BuildProgram();


            // For textured materials that may have albedo texture
            defaultVertFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/forward/texturedFrag.frag");
            defaultVertFrag.AddPreProcessorDefines(preprocessorDefines);
            defaultVertFrag.BuildProgram();
            defaultVertFrag.UseProgram();
            defaultVertFrag.SetSamplerBinding("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            defaultVertFrag.SetSamplerBinding("texture_diffuse1", DIFFUSE_TEXTURE0_BINDING);
            defaultVertFrag.SetSamplerBinding("texture_normal1", NORMAL_TEXTURE0_BINDING);
            defaultVertFrag.SetSamplerBinding("texture_specular1", SPECULAR_TEXTURE0_BINDING);
            defaultVertFrag.BindUniformBlocks(bufferBindings);


            // For textured materials with an normal map and albedo textures
            defaultVertNormalTexFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/forward/texturedFrag.frag");
            defaultVertNormalTexFrag.AddPreProcessorDefines(preprocessorDefines);
            if (enableNormalMaps)
            {
                std::string s = "NORMAL_MAPPED";
                defaultVertNormalTexFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertNormalTexFrag.BuildProgram();
            defaultVertNormalTexFrag.UseProgram();
            defaultVertNormalTexFrag.SetSamplerBinding("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            defaultVertNormalTexFrag.SetSamplerBinding("texture_diffuse1", DIFFUSE_TEXTURE0_BINDING);
            defaultVertNormalTexFrag.SetSamplerBinding("texture_normal1", NORMAL_TEXTURE0_BINDING);
            defaultVertNormalTexFrag.SetSamplerBinding("texture_specular1", SPECULAR_TEXTURE0_BINDING);
            defaultVertNormalTexFrag.BindUniformBlocks(bufferBindings);



            defaultVertUnlitFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            defaultVertUnlitFrag.BuildProgram();
            defaultVertUnlitFrag.BindUniformBlocks(bufferBindings);



            postProcessShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadTonemap.frag");
            postProcessShader.BuildProgram();

        }

    private:
        std::vector<std::string> preprocessorDefines;

        PCFShadowRenderer shadowRenderer;
        SkyRenderer skyRenderer;

        unsigned int viewportWidth;
        unsigned int viewportHeight;

        GLuint screenTexture;

        unsigned int multisampledFBO, intermediateFBO;
        unsigned int MSAATextureColorBuffer;
        unsigned int MSAADepthStencilRBO;

        StandardShader simpleDepthPass;

        StandardShader defaultVertFrag;
        StandardShader defaultVertNormalTexFrag;
        StandardShader defaultVertUnlitFrag;

        //Shader used to render to a quad:
        StandardShader postProcessShader;


        std::unique_ptr<Mesh> screenQuad;



        #pragma pack(push, 1)
        struct GlobalMatrices
        {
            glm::mat4 projectionMatrix; 
            glm::mat4 viewMatrix;       
            glm::mat4 inverseViewMatrix;
        };
        struct LocalMatrices 
        {
           glm::mat4 modelMatrix;     
           glm::mat4 normalMatrix;    
        };
        struct LightData
        {
            glm::vec4 ambientLight;
            int numDirLights;
            int numPointLights;
            int pad;
            int pad2;
            DirectionalLight::DirectionalLightData dirLights[MAX_DIR_LIGHTS];
            PointLight::PointLightData pointLights[MAX_POINT_LIGHTS];
        };
        struct MaterialProperties 
        {
            glm::vec4 albedoColor; 
            glm::vec4 emissiveColor; 
            glm::vec4 specular;      
        };
        #pragma pack(pop)

};

#endif