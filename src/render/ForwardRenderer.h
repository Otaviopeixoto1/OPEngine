#ifndef FORWARD_RENDERER_H
#define FORWARD_RENDERER_H

#include "BaseRenderer.h"
#include "render_features/ShadowRenderer.h"
#include "render_features/SkyRenderer.h"
#include <exception>


class ForwardRenderer : public BaseRenderer
{
    public:
        static constexpr unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
        static constexpr unsigned int SHADOW_CASCADE_COUNT = 3; // MAX == 4
        
        static constexpr bool enableShadowMapping = true;
        static constexpr bool enableNormalMaps = false;

        unsigned int MSAASamples = 4; 
        float tonemapExposure = 1.0f;

        const int MAX_DIR_LIGHTS = 3;
        const int MAX_POINT_LIGHTS = 10;


        std::unordered_map<std::string, unsigned int> preprocessorDefines =
        {
            {"MAX_DIR_LIGHTS", MAX_DIR_LIGHTS},
            {"MAX_POINT_LIGHTS", MAX_POINT_LIGHTS},
            {"SHADOW_CASCADE_COUNT", SHADOW_CASCADE_COUNT}
        };


        enum MainPassBufferBindings
        {
            SHADOW_MAP_BUFFER0_BINDING = 0,
            SHADOW_MAP_BUFFER1_BINDING = 1,
            SHADOW_MAP_BUFFER2_BINDING = 2,
        };


        // Adopted naming conventions for the global uniform blocks
        std::string NamedBufferBindings[5] = {
            "GlobalMatrices",
            "LocalMatrices",
            "MaterialProperties",
            "Lights",
            "Shadows",
            
        };
        
        //ADD UNIFORM AS PREFIX TO MAKE CLEAR THAT THESE ARE NOT TEXTURE BINDINGS BUT UNIFORM BUFFER BINDINGS
        enum FRUniformBufferBindings
        {
            UNIFORM_GLOBAL_MATRICES_BINDING = 0,
            UNIFORM_LOCAL_MATRICES_BINDING = 1,
            UNIFORM_MATERIAL_PROPERTIES_BINDING = 2,
            UNIFORM_GLOBAL_LIGHTS_BINDING = 3,
            UNIFORM_GLOBAL_SHADOWS_BINDING = 4,
            
        };

        ForwardRenderer(unsigned int vpWidth, unsigned int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
        }

        void RecreateResources(Scene &scene, Camera &camera)
        {
            scene.MAX_DIR_LIGHTS = MAX_DIR_LIGHTS;
            scene.MAX_POINT_LIGHTS = MAX_POINT_LIGHTS;

            if (enableShadowMapping)
            {
                this->shadowRenderer = ShadowRenderer(
                    camera.Near,
                    camera.Far,
                    UNIFORM_GLOBAL_SHADOWS_BINDING, 
                    SHADOW_CASCADE_COUNT,
                    SHADOW_WIDTH,
                    SHADOW_HEIGHT

                );
                this->shadowRenderer.RecreateResources();
            }

            this->skyRenderer = SkyRenderer();
            this->skyRenderer.RecreateResources();
            


            // Setting up screen quad for postprocessing: HDR tonemapping and gamma correction
            glGenVertexArrays(1, &screenQuadVAO);
            glGenBuffers(1, &screenQuadVBO);
            glBindVertexArray(screenQuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


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


        void RenderFrame(Camera &camera, Scene *scene, GLFWwindow *window)
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




            // 1) Shadow Map Rendering Pass:
            // -----------------------------

            std::vector<unsigned int> shadowMapBuffers = {0};
            if (enableShadowMapping)
            {
                shadowMapBuffers = shadowRenderer.Render(frameResources);
            }
                

            // 2) Main Rendering pass:
            // -----------------------

            glBindFramebuffer(GL_FRAMEBUFFER, multisampledFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


            // Setting input Resources:

            // -GlobalMatrices:
            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(inverseViewMatrix));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 
            
            // -Lights:
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            int offset = 0;

            glBufferSubData(GL_UNIFORM_BUFFER, offset, 4 * sizeof(float), glm::value_ptr(lights.ambientLight));
            offset = 4* sizeof(float);

            glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(int), &lights.numDirLights);
            offset += sizeof(int);
            glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(int), &lights.numPointLights);
            offset += sizeof(int);

            offset += 2 * sizeof(int);

            glBufferSubData(GL_UNIFORM_BUFFER, offset, MAX_DIR_LIGHTS * sizeof(DirectionalLight::DirectionalLightData), lights.directionalLights.data());
            offset += MAX_DIR_LIGHTS * sizeof(DirectionalLight::DirectionalLightData);
            glBufferSubData(GL_UNIFORM_BUFFER, offset, MAX_POINT_LIGHTS * sizeof(PointLight::PointLightData), lights.pointLights.data());
            offset += MAX_POINT_LIGHTS * sizeof(PointLight::PointLightData);

            glBindBuffer(GL_UNIFORM_BUFFER, 0);



            // -Shadows:
            glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_BUFFER0_BINDING);
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffers[0]);


            int shaderCache = -1;

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;
                
                if (materialInstance->HasFlags(OP_MATERIAL_TEXTURED_DIFFUSE | OP_MATERIAL_TEXTURED_NORMAL))
                {
                    activeShader = defaultVertNormalTexFrag;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertTexFrag;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    activeShader = defaultVertUnlitFrag;
                }
                else
                {
                    activeShader = defaultVertFrag;
                }

                if (activeShader.ID != shaderCache)
                {
                    activeShader.UseProgram();
                    shaderCache = activeShader.ID;
                }


                // -Material
                glBindBuffer(GL_UNIFORM_BUFFER, MaterialUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, MaterialBufferSize, &(materialInstance->properties));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);   


                // -Local Matrices
                glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(objectToWorld));
                glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)));
                glBindBuffer(GL_UNIFORM_BUFFER, 0); 


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
                            number = std::to_string(diffuseNr++);
                            name = "texture_diffuse";
                            break;
                        case OP_TEXTURE_SPECULAR:
                            number = std::to_string(specularNr++);
                            name = "texture_specular";
                            break;
                        case OP_TEXTURE_NORMAL:
                            number = std::to_string(normalNr++);
                            name = "texture_normal";
                            break;
                        
                        default:
                            number = "";
                            name = "texture_unidentified";
                            break;
                    }

                    activeShader.SetInt((name + number).c_str(), SHADOW_MAP_BUFFER2_BINDING + i);
                    glBindTexture(GL_TEXTURE_2D, texture.id);
                }
                
                //bind VAO
                mesh->BindBuffers();

                //Indexed drawing
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });  

            // Rendering skybox
            this->skyRenderer.Render(frameResources);

            // Blit the MSAA buffer to an intermediate framebuffer for postprocessing:
            glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampledFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediateFBO);
            glBlitFramebuffer(0, 0, viewportWidth, viewportHeight, 0, 0, viewportWidth, viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);





            // 3) Final Pass (Postprocessing):
            // -------------------------------

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            postProcessShader.UseProgram();
            postProcessShader.SetFloat("exposure", tonemapExposure);
            glBindVertexArray(screenQuadVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, screenTexture); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

        }



        void ReloadShaders()
        {
            simpleDepthPass = Shader(BASE_DIR"/data/shaders/simpleVert.vert", BASE_DIR"/data/shaders/nullFrag.frag");
            simpleDepthPass.BuildProgram();



            defaultVertFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/forward/albedoFrag.frag");
            defaultVertFrag.AddPreProcessorDefines(preprocessorDefines);
            if (enableShadowMapping)
            {
                std::string s = "DIR_LIGHT_SHADOWS";
                defaultVertFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertFrag.BuildProgram();
            defaultVertFrag.UseProgram();
            defaultVertFrag.SetInt("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            defaultVertFrag.BindUniformBlocks(NamedBufferBindings,5);


            // For textured materials with an albedo texture
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/forward/texturedFrag.frag");
            defaultVertTexFrag.AddPreProcessorDefines(preprocessorDefines);
            if (enableShadowMapping)
            {
                std::string s = "DIR_LIGHT_SHADOWS";
                defaultVertTexFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertTexFrag.BuildProgram();
            defaultVertTexFrag.UseProgram();
            defaultVertTexFrag.SetInt("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            defaultVertTexFrag.BindUniformBlocks(NamedBufferBindings,5);


            // For textured materials with an normal map and albedo textures
            defaultVertNormalTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/forward/texturedFrag.frag");
            defaultVertNormalTexFrag.AddPreProcessorDefines(preprocessorDefines);
            if (enableShadowMapping)
            {
                std::string s = "DIR_LIGHT_SHADOWS";
                defaultVertNormalTexFrag.AddPreProcessorDefines(&s,1);
            }
            if (enableNormalMaps)
            {
                std::string s = "NORMAL_MAPPED";
                defaultVertNormalTexFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertNormalTexFrag.BuildProgram();
            defaultVertNormalTexFrag.UseProgram();
            defaultVertNormalTexFrag.SetInt("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            defaultVertNormalTexFrag.BindUniformBlocks(NamedBufferBindings,5);



            defaultVertUnlitFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            defaultVertUnlitFrag.BuildProgram();
            defaultVertUnlitFrag.BindUniformBlocks(NamedBufferBindings,3);



            postProcessShader = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadTonemap.frag");
            postProcessShader.BuildProgram();



            
            
            

            



            // Binding UBOs to the correct points
            // ----------------------------------

            glGenBuffers(1, &GlobalMatricesUBO);
            glGenBuffers(1, &LocalMatricesUBO);

            // Matrix buffers setup
            // -------------------
            /* GlobalMatrices Uniform buffer structure:
             * {
             *    mat4 projectionMatrix; 
             *    mat4 viewMatrix; 
             *    mat4 inverseViewMatrix;      
             * }
             * 
             * LocalMatrices Uniform buffer structure:
             * {
             *    mat4 modelMatrix;     
             *    mat4 normalMatrix;    
             * }
             */
            
            // Create the buffer and specify its size:

            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
            glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
            glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            // Bind a certain range of the buffer to the uniform block: this allows to use multiple UBOs 
            // per uniform block
            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_GLOBAL_MATRICES_BINDING, GlobalMatricesUBO, 0, 3 * sizeof(glm::mat4));
            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_LOCAL_MATRICES_BINDING, LocalMatricesUBO, 0, 2 * sizeof(glm::mat4));





            glGenBuffers(1, &LightsUBO);

            // Light buffer setup
            // ------------------
            /* DirLight struct structure:
             * {
             *    vec4 lightColor; 
             *    mat4 lightMatrix;
             *    vec4 direction; 
             * }
             */

            /* Lights Uniform buffer structure:
             * {
             *    vec4 ambientLight;
             *    int numDirLights;
             *    int numPointLights;
             *    int pad;
             *    int pad;
             *    DirLight dirLights[MAX_DIR_LIGHTS];
             *    PointLight pointLights[MAX_POINT_LIGHTS];
             *    
             *     
             * }
             */
            
            LightBufferSize = 0;
            {
                LightBufferSize += sizeof(glm::vec4);
                LightBufferSize += 4 * sizeof(int);
                LightBufferSize += MAX_DIR_LIGHTS * sizeof(DirectionalLight::DirectionalLightData);
                LightBufferSize += MAX_POINT_LIGHTS * sizeof(PointLight::PointLightData);
            }

            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            glBufferData(GL_UNIFORM_BUFFER, LightBufferSize, NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_GLOBAL_LIGHTS_BINDING, LightsUBO, 0, LightBufferSize);




            glGenBuffers(1, &MaterialUBO);

            // Material buffer setup
            // ---------------------
            /* MaterialProperties Uniform buffer structure:
             * {
             *    vec4 albedoColor; 
             *    vec4 emissiveColor; 
             *    vec4 specular;      
             * }
             */

            MaterialBufferSize = 0;
            {
                MaterialBufferSize += 3 * sizeof(glm::vec4);
            }

            glBindBuffer(GL_UNIFORM_BUFFER, MaterialUBO);
            glBufferData(GL_UNIFORM_BUFFER, MaterialBufferSize, NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_MATERIAL_PROPERTIES_BINDING, MaterialUBO, 0, MaterialBufferSize);

        }

    private:
        ShadowRenderer shadowRenderer;
        SkyRenderer skyRenderer;

        unsigned int viewportWidth;
        unsigned int viewportHeight;


        unsigned int LightBufferSize = 0;
        unsigned int MaterialBufferSize = 0;

        unsigned int screenQuadVAO, screenQuadVBO;
        unsigned int screenTexture;

        unsigned int multisampledFBO, intermediateFBO;
        unsigned int MSAATextureColorBuffer;
        unsigned int MSAADepthStencilRBO;

        unsigned int GlobalMatricesUBO;
        unsigned int LocalMatricesUBO;
        unsigned int MaterialUBO;
        unsigned int LightsUBO;

        Shader simpleDepthPass;

        Shader defaultVertFrag;
        Shader defaultVertTexFrag;
        Shader defaultVertNormalTexFrag;
        Shader defaultVertUnlitFrag;

        //Shader used to render to a quad:
        Shader postProcessShader;


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



        

};

#endif