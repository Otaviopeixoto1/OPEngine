#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H

#include "BaseRenderer.h"



class DeferredRenderer : public BaseRenderer
{
    public:

        std::string PreprocessorDefines[1] = { 
            "LIGHT_VOLUMES",
        };

        // ***Adopted naming conventions for the global uniform blocks***
        std::string NamedBufferBindings[4] = { // The indexes have to match values in DRBufferBindings enum
            "GlobalMatrices",
            "LocalMatrices",
            "MaterialProperties",
            "Lights"
        };

        enum GBufferBindings
        {
            COLOR_SPEC_BUFFER_BINDING = 0,
            NORMAL_BUFFER_BINDING = 1,
            POSITION_BUFFER_BINDING = 2,
        };
        enum DRGlobalBufferBindings
        {
            GLOBAL_MATRICES_BINDING = 0,
            LOCAL_MATRICES_BINDING = 1,
            MATERIAL_PROPERTIES_BINDING = 2,
            GLOBAL_LIGHTS_BINDING = 3
        };

        DeferredRenderer(unsigned int vpWidth, unsigned int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
        }

        void RecreateResources(Scene &scene)
        {
            // Quad object for rendering the final scene:
            glGenVertexArrays(1, &screenQuadVAO);
            glGenBuffers(1, &screenQuadVBO);
            glBindVertexArray(screenQuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


            // gBuffer:
            glGenFramebuffers(1, &gBufferFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

            // 1) HDR color + specular buffer attachment 
            glGenTextures(1, &gColorBuffer);
            glBindTexture(GL_TEXTURE_2D, gColorBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gColorBuffer, 0);
            
            // 2) View space normal buffer attachment 
            glGenTextures(1, &gNormalBuffer);
            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormalBuffer, 0);

            // 3) View space position buffer attachment 
            glGenTextures(1, &gPositionBuffer);
            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gPositionBuffer, 0);


            //setting the depth and stencil attachments
            glGenRenderbuffers(1, &depthStencilBuffer); 
            glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);


            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Framebuffer for MSAA is incomplete");
            }
                
            unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
            glDrawBuffers(3, attachments);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void ViewportUpdate(int vpWidth, int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;

            glBindTexture(GL_TEXTURE_2D, gColorBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vpWidth, vpHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 


            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vpWidth, vpHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 


            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vpWidth, vpHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 


            glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, vpWidth, vpHeight);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }


        void RenderFrame(const Camera &camera, Scene *scene, GLFWwindow *window)
        {
            // Rendering to the gBuffer:
            // -------------------------
            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);


            // Global Uniforms (dont depend on objects/materials)
            // --------------------------------------------------
            glm::mat4 viewMatrix = camera.GetViewMatrix();
            glm::mat4 projectionMatrix = camera.GetProjectionMatrix();

            // Setting GlobalMatricesUBO:
            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMatrix));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 
        

            int shaderCache = -1;


            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;
                if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertTexFrag;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    //activeShader = defaultVertUnlitFrag;
                    return;
                }
                else
                {
                    activeShader = defaultVertFrag;
                }

                // shader has to be used before updating the uniforms
                if (activeShader.ID != shaderCache)
                {
                    activeShader.UseProgram();
                    shaderCache = activeShader.ID;
                }


                // Setting object-related properties
                // ---------------------------------

                glBindBuffer(GL_UNIFORM_BUFFER, MaterialUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, MaterialBufferSize, &(materialInstance->properties));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);   


                // Update model and normal matrices:
                glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(objectToWorld));
                glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)));
                glBindBuffer(GL_UNIFORM_BUFFER, 0); 


                //Bind all textures
                unsigned int diffuseNr = 1;
                unsigned int specularNr = 1;
                unsigned int normalNr = 1;

                for (unsigned int i = 0; i < materialInstance->numTextures; i++)
                {
                    Texture texture = scene->GetTexture(materialInstance->GetTexturePath(i));

                    // activate proper texture unit before binding
                    glActiveTexture(GL_TEXTURE0 + i); 

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

                    activeShader.SetInt((name + number).c_str(), i);
                    glBindTexture(GL_TEXTURE_2D, texture.id);
                }
                
                //bind VAO
                mesh->BindBuffers();

                //Indexed drawing
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });  



            // Lighting pass: use g-buffer to calculate the scene's lighting
            // -------------------------------------------------------------
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);

            // Get Light data in view space:
            GlobalLightData lights = scene->GetLightData(viewMatrix);
            
            // Setting LightsUBO:
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, LightBufferSize, &lights);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);   

            // Light Volume pass:
            //lights.pointLights



            // Directional Light Pass:
            directionalLightingPass.UseProgram();
            glBindVertexArray(screenQuadVAO);
            glActiveTexture(GL_TEXTURE0 + COLOR_SPEC_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, gColorBuffer); 
            glActiveTexture(GL_TEXTURE0 + NORMAL_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glActiveTexture(GL_TEXTURE0 + POSITION_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);

            glDrawArrays(GL_TRIANGLES, 0, 6);




            // For additonal draws: Copy the gBuffer depth to default framebuffer's depth buffer
            // ---------------------------------------------------------------------------------
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); 

            glBlitFramebuffer(0, 0, viewportWidth, viewportHeight, 0, 0, viewportWidth, viewportHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);


            // Unlit and Transparent Pass: render objects with different lighting models using same depth buffer:
            // --------------------------------------------------------------------------------------------------
            
            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;
                if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    activeShader = defaultVertUnlitFrag;   
                }
                else
                {
                    return;
                }

                if (activeShader.ID != shaderCache)
                {
                    activeShader.UseProgram();
                    shaderCache = activeShader.ID; 
                }

                glBindBuffer(GL_UNIFORM_BUFFER, MaterialUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, MaterialBufferSize, &(materialInstance->properties));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);   

                glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(objectToWorld));
                glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)));
                glBindBuffer(GL_UNIFORM_BUFFER, 0); 

                mesh->BindBuffers();
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            }); 


        }




        void ReloadShaders()
        {
            defaultVertFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/gBuffer/gBufferAlbedo.frag");
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/gBuffer/gBufferTextured.frag");
            defaultVertUnlitFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            
            defaultVertFrag.BindUniformBlocks(NamedBufferBindings,3);
            defaultVertTexFrag.BindUniformBlocks(NamedBufferBindings,3);
            defaultVertUnlitFrag.BindUniformBlocks(NamedBufferBindings,3);



            directionalLightingPass = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/gBuffer/deferredLighting.frag");
            
            directionalLightingPass.UseProgram();

            // binding points of the gbuffer textures:
            directionalLightingPass.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            directionalLightingPass.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            directionalLightingPass.SetInt("gPosition", POSITION_BUFFER_BINDING);

            directionalLightingPass.BindUniformBlock(NamedBufferBindings[GLOBAL_LIGHTS_BINDING], GLOBAL_LIGHTS_BINDING);
            
            



            // Setting UBOs to the correct binding points
            // ------------------------------------------

            glGenBuffers(1, &GlobalMatricesUBO);
            glGenBuffers(1, &LocalMatricesUBO);

            // Matrix buffers setup
            // -------------------
            /* GlobalMatrices Uniform buffer structure:
             * {
             *    mat4 projectionMatrix; 
             *    mat4 viewMatrix;       
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
            glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
            glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            // Bind a certain range of the buffer to the uniform block: this allows to use multiple UBOs 
            // per uniform block
            glBindBufferRange(GL_UNIFORM_BUFFER, GLOBAL_MATRICES_BINDING, GlobalMatricesUBO, 0, 2 * sizeof(glm::mat4));
            glBindBufferRange(GL_UNIFORM_BUFFER, LOCAL_MATRICES_BINDING, LocalMatricesUBO, 0, 2 * sizeof(glm::mat4));





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
            
            glBindBufferRange(GL_UNIFORM_BUFFER, GLOBAL_LIGHTS_BINDING, LightsUBO, 0, LightBufferSize);




            glGenBuffers(1, &MaterialUBO);

            // Material buffer setup
            // -------------------
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

            glBindBufferRange(GL_UNIFORM_BUFFER, MATERIAL_PROPERTIES_BINDING, MaterialUBO, 0, MaterialBufferSize);


        }

    private:
        unsigned int viewportWidth;
        unsigned int viewportHeight;

        unsigned int gBufferFBO;
        unsigned int gColorBuffer, gNormalBuffer, gPositionBuffer;
        unsigned int depthStencilBuffer;


        unsigned int LightBufferSize = 0;
        unsigned int MaterialBufferSize = 0;
        //MipBuilder mipBuilder;
        //BlurBuilder blurBuilder;
        //DebugRenderer debugRenderer;
        unsigned int GlobalMatricesUBO;
        unsigned int LocalMatricesUBO;
        unsigned int LightsUBO;
        unsigned int MaterialUBO;

        Shader defaultVertFrag;
        Shader defaultVertTexFrag;
        Shader defaultVertUnlitFrag;

        Shader directionalLightingPass;


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

};

#endif