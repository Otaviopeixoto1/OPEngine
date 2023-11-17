#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H

#include "BaseRenderer.h"



class DeferredRenderer : public BaseRenderer
{
    public:

        float tonemapExposure = 1.0f;
        float FXAAContrastThreshold = 0.0312f;
        float FXAABrightnessThreshold = 0.063f;
        const bool enableLightVolumes = true;


        enum GBufferBindings
        {
            COLOR_SPEC_BUFFER_BINDING = 0,
            NORMAL_BUFFER_BINDING = 1,
            POSITION_BUFFER_BINDING = 2,
            ACCUMULATION_BUFFER_BINDING = 3,
        };

        std::string PreprocessorDefines[2] = { 
            "MAX_DIR_LIGHTS 5",
            "MAX_POINT_LIGHTS 3",
        };

        // ***Adopted naming conventions for the global uniform blocks***
        std::string NamedBufferBindings[4] = { // The indexes have to match values in DRBufferBindings enum
            "GlobalMatrices",
            "LocalMatrices",
            "MaterialProperties",
            "Lights",

        };
        
        enum DRGlobalBufferBindings
        {
            GLOBAL_MATRICES_BINDING = 0,
            LOCAL_MATRICES_BINDING = 1,
            MATERIAL_PROPERTIES_BINDING = 2,
            GLOBAL_LIGHTS_BINDING = 3,

        };

        

        DeferredRenderer(unsigned int vpWidth, unsigned int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
        }

        void RecreateResources(Scene &scene)
        {
            LoadLightingResources();

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
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + COLOR_SPEC_BUFFER_BINDING, GL_TEXTURE_2D, gColorBuffer, 0);
            
            // 2) View space normal buffer attachment 
            glGenTextures(1, &gNormalBuffer);
            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + NORMAL_BUFFER_BINDING, GL_TEXTURE_2D, gNormalBuffer, 0);

            // 3) View space position buffer attachment 
            glGenTextures(1, &gPositionBuffer);
            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + POSITION_BUFFER_BINDING, GL_TEXTURE_2D, gPositionBuffer, 0);


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
                
            unsigned int attachments[3] = { 
                GL_COLOR_ATTACHMENT0 + COLOR_SPEC_BUFFER_BINDING, 
                GL_COLOR_ATTACHMENT0 + NORMAL_BUFFER_BINDING, 
                GL_COLOR_ATTACHMENT0 + POSITION_BUFFER_BINDING
            };
            glDrawBuffers(3, attachments);




            // Light Accumulation:
            glGenFramebuffers(1, &lightAccumulationFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, lightAccumulationFBO);

            glGenTextures(1, &lightAccumulationTexture);
            glBindTexture(GL_TEXTURE_2D, lightAccumulationTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 

            // Bind the accumulation texture, making sure it will be a different binding from those
            // that will be used for the gbuffer textures that will be sampled on the accumulation pass
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + ACCUMULATION_BUFFER_BINDING, GL_TEXTURE_2D, lightAccumulationTexture, 0);
            // Bind the same depth buffer for drawing light volumes
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + ACCUMULATION_BUFFER_BINDING);




            // Postprocessing Pass (Tonemapping): 
            glGenFramebuffers(1, &postProcessFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);

            glGenTextures(1, &postProcessColorBuffer);
            glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer);
            // Clamped between 0 and 1 (no longer needs to be a floating point buffer)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            // This texture will not have a special binding, instead we just bind to 0 (the standard binding)
            // as currently we dont use the gBuffer in this pass (no bind conflicts)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, postProcessColorBuffer, 0);

            // no depth buffer needed

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }

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

            glBindTexture(GL_TEXTURE_2D, lightAccumulationTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 

            glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void LoadLightingResources()
        {
            glGenVertexArrays(1, &screenQuadVAO);
            glGenBuffers(1, &screenQuadVBO);
            glBindVertexArray(screenQuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

            pointLightVolume = MeshData::LoadMeshFromFile(BASE_DIR "/data/models/light_volumes/pointLightVolume.obj");
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

            glBindFramebuffer(GL_FRAMEBUFFER, lightAccumulationFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDepthMask(GL_FALSE);


            // Binding the gBuffer textures:
            //these binding dont have to be the same as the gbuffer bindings but it will be better follow a convention
            glActiveTexture(GL_TEXTURE0 + COLOR_SPEC_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, gColorBuffer); 
            glActiveTexture(GL_TEXTURE0 + NORMAL_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glActiveTexture(GL_TEXTURE0 + POSITION_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);

            // Get Light data in view space:
            GlobalLightData lights = scene->GetLightData(viewMatrix);
            
            // Setting LightsUBO:
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, LightBufferSize, &lights);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);   


            
            // Light Volumes: Render all point light volumes
            if (enableLightVolumes)
            {
                // Blend the lighting passes
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_ONE, GL_ONE);

                glEnable(GL_STENCIL_TEST);
                pointLightVolume->BindBuffers();
                
                for (size_t i = 0; i < lights.numPointLights; i++)
                {
                    // Stencil Pass:
                    lightVolumeStencilPass.UseProgram();

                    glDisable(GL_CULL_FACE);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    // We need the stencil test to be enabled but we want it to succeed always.
                    // Only the depth test matters.
                    glStencilFunc(GL_ALWAYS, 0, 0);

                    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
                    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

                    PointLight::PointLightData l = lights.pointLights[i];
                    glm::mat4 rootTransform = glm::mat4(1);
                    rootTransform = glm::translate(rootTransform, glm::vec3(l.position));
                    rootTransform = glm::scale(rootTransform, glm::vec3(l.radius, l.radius, l.radius));

                    lightVolumeStencilPass.SetMat4("modelViewMatrix", rootTransform);

                    glDrawElements(GL_TRIANGLES, pointLightVolume->indicesCount, GL_UNSIGNED_INT, 0);


                    // Lighting calculation pass:
                    glDisable(GL_DEPTH_TEST);
                    // Discard all pixels that are not equal to zero:
                    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);

                    pointLightVolShader.UseProgram();

                    pointLightVolShader.SetInt("instanceID", i);
                    pointLightVolShader.SetMat4("modelViewMatrix", rootTransform);
                    pointLightVolShader.SetVec2("gScreenSize", viewportWidth, viewportHeight);

                    glDrawElements(GL_TRIANGLES, pointLightVolume->indicesCount, GL_UNSIGNED_INT, 0);
                    glCullFace(GL_BACK);
                }
                glDisable(GL_STENCIL_TEST);
            }

            // Directional Lights:
            directionalLightingPass.UseProgram();
            glBindVertexArray(screenQuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);



            // Unlit Pass: render objects with different lighting models using same depth buffer:
            // ----------------------------------------------------------------------------------

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

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

            // Postprocess Pass: apply tonemap to the HDR color buffer
            // ---------------------------------------------------------

            glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            postProcessShader.UseProgram();
            postProcessShader.SetFloat("exposure", tonemapExposure);
            glBindVertexArray(screenQuadVAO);
            glActiveTexture(GL_TEXTURE0 + ACCUMULATION_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, lightAccumulationTexture); 
            glDrawArrays(GL_TRIANGLES, 0, 6);


            // Final Pass (Antialiasing):
            // --------------------------

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            FXAAShader.UseProgram();
            FXAAShader.SetVec2("pixelSize", 1.0f/(float)viewportWidth, 1.0f/(float)viewportHeight);
            FXAAShader.SetFloat("contrastThreshold", FXAAContrastThreshold);
            FXAAShader.SetFloat("brightnessThreshold", FXAABrightnessThreshold);
            glBindVertexArray(screenQuadVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

        }




        void ReloadShaders()
        {
            //Material specific shaders. These should dynamically load depending on available scene materials
            defaultVertFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferAlbedo.frag");
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            defaultVertUnlitFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");

            defaultVertFrag.Build();
            defaultVertTexFrag.Build();
            defaultVertUnlitFrag.Build();

            defaultVertFrag.BindUniformBlocks(NamedBufferBindings,3);
            defaultVertTexFrag.BindUniformBlocks(NamedBufferBindings,3);
            defaultVertUnlitFrag.BindUniformBlocks(NamedBufferBindings,3);



            directionalLightingPass = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/deferred/fsDeferredLighting.frag");
            directionalLightingPass.AddPreProcessorDefines(PreprocessorDefines,2);
            if (enableLightVolumes)
            {
                std::string pd = "LIGHT_VOLUMES";
                directionalLightingPass.AddPreProcessorDefines(&pd,1);
            }

            directionalLightingPass.Build();
            directionalLightingPass.UseProgram();

            // binding points of the gbuffer textures:
            directionalLightingPass.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            directionalLightingPass.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            directionalLightingPass.SetInt("gPosition", POSITION_BUFFER_BINDING);
            directionalLightingPass.BindUniformBlock(NamedBufferBindings[GLOBAL_LIGHTS_BINDING], GLOBAL_LIGHTS_BINDING);
            

            lightVolumeStencilPass = Shader(BASE_DIR"/data/shaders/deferred/lightVolume.vert", BASE_DIR"/data/shaders/deferred/nullFrag.frag");
            lightVolumeStencilPass.Build();
            lightVolumeStencilPass.BindUniformBlock(NamedBufferBindings[GLOBAL_MATRICES_BINDING],GLOBAL_MATRICES_BINDING);
            

            pointLightVolShader = Shader(BASE_DIR"/data/shaders/deferred/lightVolume.vert", BASE_DIR"/data/shaders/deferred/pointVolDeferredLighting.frag");
            pointLightVolShader.Build();
            pointLightVolShader.BindUniformBlock(NamedBufferBindings[GLOBAL_MATRICES_BINDING],GLOBAL_MATRICES_BINDING);
            pointLightVolShader.BindUniformBlock(NamedBufferBindings[GLOBAL_LIGHTS_BINDING],GLOBAL_LIGHTS_BINDING);
            pointLightVolShader.UseProgram();
            pointLightVolShader.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            pointLightVolShader.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            pointLightVolShader.SetInt("gPosition", POSITION_BUFFER_BINDING);


            postProcessShader = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadTonemapLum.frag");
            postProcessShader.Build();
            postProcessShader.UseProgram();
            postProcessShader.SetInt("screenTexture", ACCUMULATION_BUFFER_BINDING);


            FXAAShader = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadFXAA.frag");
            FXAAShader.Build();


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

        unsigned int lightAccumulationFBO;
        unsigned int lightAccumulationTexture;

        unsigned int postProcessFBO;
        unsigned int postProcessColorBuffer;

        unsigned int LightBufferSize = 0;
        unsigned int MaterialBufferSize = 0;

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
        

        Shader lightVolumeStencilPass; 
        Shader pointLightVolShader;
        std::shared_ptr<Mesh> pointLightVolume;
        
        
        Shader postProcessShader;
        Shader FXAAShader;



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