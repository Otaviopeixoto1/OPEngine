#ifndef VCTGI_RENDERER_H
#define VCTGI_RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "BaseRenderer.h"
#include "render_features/ShadowRenderer.h"
#include "render_features/SkyRenderer.h"
#include "../debug/OPProfiler.h"
#include "../common/Colors.h"


class VCTGIRenderer : public BaseRenderer
{
    public:
        static constexpr unsigned int MAX_MIP_MAP_LEVELS = 9;
        static constexpr unsigned int MAX_SPARSE_BUFFER_SIZE = 134217728;

        const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
        static constexpr unsigned int SHADOW_CASCADE_COUNT = 1; // MAX == 4

        static constexpr bool enableShadowMapping = true;
        static constexpr bool enableNormalMaps = true;

        float tonemapExposure = 1.0f;
        float FXAAContrastThreshold = 0.0312f;
        float FXAABrightnessThreshold = 0.063f;

        static constexpr bool enableLightVolumes = true;
        

        const int MAX_DIR_LIGHTS = 5;
        const int MAX_POINT_LIGHTS = 20;

        std::unordered_map<std::string, unsigned int> preprocessorDefines =
        {
            {"MAX_DIR_LIGHTS", MAX_DIR_LIGHTS},
            {"MAX_POINT_LIGHTS", MAX_POINT_LIGHTS},
            {"SHADOW_CASCADE_COUNT", SHADOW_CASCADE_COUNT}
        };

        enum LightingPassBufferBindings
        {
            COLOR_SPEC_BUFFER_BINDING = 0,
            NORMAL_BUFFER_BINDING = 1,
            POSITION_BUFFER_BINDING = 2,
            ACCUMULATION_BUFFER_BINDING = 3,
            SHADOW_MAP_BUFFER0_BINDING = 4,
        };

        // Adopted naming conventions for the global uniform blocks
        std::string NamedBufferBindings[5] = { // The indexes have to match values in the enum
            "GlobalMatrices",
            "LocalMatrices",
            "MaterialProperties",
            "Lights",
            "Shadows",

        };

        //ADD UNIFORM AS PREFIX TO MAKE CLEAR THAT THESE ARE NOT TEXTURE BINDINGS BUT UNIFORM BUFFER BINDINGS
        enum VCTGIUniformBufferBindings
        {
            UNIFORM_GLOBAL_MATRICES_BINDING = 0,
            UNIFORM_LOCAL_MATRICES_BINDING = 1,
            UNIFORM_MATERIAL_PROPERTIES_BINDING = 2,
            UNIFORM_GLOBAL_LIGHTS_BINDING = 3,
            UNIFORM_GLOBAL_SHADOWS_BINDING = 4,

        };
        enum VCTGIShaderStorageBindings 
        {
            DRAW_INDIRECT_BINDING = 0,
            COMPUTE_INDIRECT_BINDING = 1,
            SPARSE_LIST_BINDING = 2
        };
        
        
        
        VCTGIRenderer(unsigned int vpWidth, unsigned int vpHeight, OPProfiler::OPProfiler *profiler)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
            this->profiler = profiler;
        }

        void RecreateResources(Scene &scene, Camera &camera)
        {
            scene.MAX_DIR_LIGHTS = MAX_DIR_LIGHTS;
            scene.MAX_POINT_LIGHTS = MAX_POINT_LIGHTS;


            // Shadow maps:
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



            glGenVertexArrays(1, &screenQuadVAO);
            glGenBuffers(1, &screenQuadVBO);
            glBindVertexArray(screenQuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


            MeshData PointVolData = MeshData::LoadMeshDataFromFile(BASE_DIR "/data/models/light_volumes/pointLightVolume.obj");
            pointLightVolume = std::make_shared<Mesh>(PointVolData);


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




            // Voxelization:
            glGenFramebuffers(1, &voxelFBO);

            //setup sparse active voxel list (filled during voxelization):
            glGenBuffers(1, &sparseListBuffer);
	        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SPARSE_LIST_BINDING, sparseListBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, (sizeof(GLuint) * MAX_SPARSE_BUFFER_SIZE), NULL, GL_STREAM_DRAW);

            //contains data for drawing the voxels with indirect commands and for generating mipmaps:
            SetupDrawInd();

            //contains the workgroup structure used for the mipmap compute:
            //this will represent a 1d array of workgroups (of size 64) that will process the sparse voxel list and the 3d texture into the mipmaps
            SetupCompInd();
            
            numMipLevels = (GLuint)log2(256);
            glGenTextures(1, &voxelBuffer);
            glBindTexture(GL_TEXTURE_3D, voxelBuffer);
            glTexStorage3D(GL_TEXTURE_3D, numMipLevels + 1, GL_R32UI, voxelRes, voxelRes, voxelRes);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glGenTextures(1, &voxel2DTex);
            glBindTexture(GL_TEXTURE_2D_ARRAY, voxel2DTex);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32UI, voxelRes, voxelRes, 3);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);




            // Light Accumulation:
            glGenFramebuffers(1, &lightAccumulationFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, lightAccumulationFBO);

            glGenTextures(1, &lightAccumulationTexture);
            glBindTexture(GL_TEXTURE_2D, lightAccumulationTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 

            // Bind the accumulation texture, making sure it will be a different binding from those               (this is probably not necessary)
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


        void RenderFrame(Camera &camera, Scene *scene, GLFWwindow *window)
        {
            /*
                VCTGI order:
                CreateShadow();
                RenderData(); (gbuffer)
                Voxelize(); (light accumulation)
                MipMap();
                Draw();
            
            */



            // Set default rendering settings:
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glDepthMask(GL_TRUE);
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
            auto shadowTask = profiler->AddTask("Shadow Pass", Colors::nephritis);
            shadowTask->Start();
            std::vector<unsigned int> shadowMapBuffers = {0};
            if (enableShadowMapping)
            {
                shadowMapBuffers = shadowRenderer.Render(frameResources);
            }
            shadowTask->End();


            // 2) gBuffer Pass:
            // ----------------
            auto gbufferTask = profiler->AddTask("gBuffer Pass", Colors::emerald);
            gbufferTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, 2*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(inverseViewMatrix));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 


            int shaderCache = -1;

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;

                if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    return;
                }
                else if (materialInstance->HasFlags(OP_MATERIAL_TEXTURED_DIFFUSE | OP_MATERIAL_TEXTURED_NORMAL))
                {
                    activeShader = defaultVertNormalTexFrag;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertTexFrag;
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

            gbufferTask->End();



            // VOXELIZATION PASS 
            auto voxelizationTask = profiler->AddTask("voxelization", Colors::belizeHole);
            voxelizationTask->Start();

            //these are bound in dispatch indirect and shader storage, since they are first filled in the voxelization (storage) and then read in the mipmap compute shader (indirect)
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndBuffer);     
            glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INDIRECT_BINDING, drawIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COMPUTE_INDIRECT_BINDING, compIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SPARSE_LIST_BINDING, sparseListBuffer);

            auto sceneMat2 = glm::scale(glm::mat4(1.0f), glm::vec3(0.02,0.02,0.02)); //calculate based on the scene size or the frustrum bounds !!
            auto sceneMat0 = glm::rotate(sceneMat2, -glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
            auto sceneMat1 = glm::rotate(sceneMat2, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));

            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);  //this buffer could be set only once on initialization
            glBufferSubData(GL_UNIFORM_BUFFER, 3*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(sceneMat0));
            glBufferSubData(GL_UNIFORM_BUFFER, 4*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(sceneMat1));
            glBufferSubData(GL_UNIFORM_BUFFER, 5*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(sceneMat2));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 

            // Setup framebuffer for rendering offscreen
            GLint origViewportSize[4];
            glGetIntegerv(GL_VIEWPORT, origViewportSize);

            // Enable rendering to framebuffer with voxelRes resolution
            glBindFramebuffer(GL_FRAMEBUFFER, voxelFBO);
            glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, voxelRes);
            glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, voxelRes);
            glViewport(0, 0, voxelRes, voxelRes);

            // Clear the last voxelization data
            glClearTexImage(voxel2DTex, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
            for(size_t i = 0; i <= numMipLevels; i++) 
                glClearTexImage(voxelBuffer, (GLint)i, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

            // Reset the sparse voxel count
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawIndBuffer);
            for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) 
                glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, i * sizeof(DrawElementsIndirectCommand) + sizeof(GLuint), sizeof(GLuint), GL_RED, GL_UNSIGNED_INT, NULL); // Clear data before since data is used when drawing
            

            // Reset the sparse voxel count for compute shader
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, compIndBuffer);
            for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) 
                glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, i * sizeof(ComputeIndirectCommand), sizeof(GLuint), GL_RED, GL_UNSIGNED_INT, NULL); // Clear data before since data is used when drawing
            

            glBindImageTexture(2, voxel2DTex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
            glBindImageTexture(3, voxelBuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

            glActiveTexture(GL_TEXTURE5); // use a propper binding point variable
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffers[0]);

            // All faces must be rendered
            glDisable(GL_CULL_FACE);
            
            // 1st VOXELIZATION:
            
            voxelizationShader.UseProgram();
            voxelizationShader.SetInt("shadowMap0", 5); // do the binding properly
            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                GLuint activeRoutine;

                if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    return;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeRoutine = 1;
                }
                else
                {
                    activeRoutine = 0;
                }

                glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &activeRoutine);


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
                    glActiveTexture(GL_TEXTURE0); 

                    std::string number;
                    TextureType type = texture.type;

                    if (texture.type == OP_TEXTURE_DIFFUSE)
                    {
                        voxelizationShader.SetInt("texture_diffuse1", 0);
                        glBindTexture(GL_TEXTURE_2D, texture.id);
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                
                //bind VAO
                mesh->BindBuffers();

                //Indexed drawing
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            });  
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            glViewport(origViewportSize[0], origViewportSize[1], origViewportSize[2], origViewportSize[3]);
            glEnable(GL_CULL_FACE);
            
            
            
            //MIPMAPPING
















            voxelizationTask->End();
            //

            
            // 3) Lighting Accumulation pass: use g-buffer to calculate the scene's lighting
            // -----------------------------------------------------------------------------
            auto lightAccTask = profiler->AddTask("Light Accumulation", Colors::orange);
            lightAccTask->Start();

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
            glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_BUFFER0_BINDING);
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffers[0]);

            
            
            // Setting LightsUBO:
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            int offset = 0;

            glBufferSubData(GL_UNIFORM_BUFFER, offset, 4* sizeof(float), glm::value_ptr(lights.ambientLight));
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
                    simpleDepthPass.UseProgram();
                    glEnable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    glClear(GL_STENCIL_BUFFER_BIT);

                    // We need the stencil test to be enabled but we want it to succeed always.
                    // Only the depth test matters.
                    glStencilFunc(GL_ALWAYS, 0, 0);

                    // For the back facing polygons, INCREMENT the value in the stencil buffer when the depth test 
                    // fails but keep it unchanged when either depth test or stencil test succeed.
                    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);

                    // For the front facing polygons, DECREMENT the value in the stencil buffer when the depth test
                    // fails but keep it unchanged when either depth test or stencil test succeed.
                    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

                    // this will guarantee that the points inside the sphere have stencil of 1,
                    // points outside the sphere will have stencil of 0


                    PointLight::PointLightData l = lights.pointLights[i];
                    glm::mat4 viewTransform = glm::mat4(1);
                    viewTransform = glm::translate(viewTransform, glm::vec3(l.position));
                    viewTransform = glm::scale(viewTransform, glm::vec3(l.radius, l.radius, l.radius));

                    simpleDepthPass.SetMat4("MVPMatrix", projectionMatrix * viewTransform);

                    glDrawElements(GL_TRIANGLES, pointLightVolume->indicesCount, GL_UNSIGNED_INT, 0);


                    // Lighting calculation pass:
                    glDisable(GL_DEPTH_TEST);

                    // Only run for pixels which have stencil different from zero
                    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);

                    pointLightVolShader.UseProgram();

                    pointLightVolShader.SetInt("instanceID", i);
                    pointLightVolShader.SetMat4("MVPMatrix", projectionMatrix * viewTransform);
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

            lightAccTask->End();


            // 4) Unlit Pass (render objects with different lighting models using same depth buffer):
            // --------------------------------------------------------------------------------------
            auto renderUnlitTask = profiler->AddTask("Unlit & Sky Pass", Colors::greenSea);
            renderUnlitTask->Start();

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

            this->skyRenderer.Render(frameResources);

            renderUnlitTask->End();

            // 5) Postprocess Pass: apply tonemap to the HDR color buffer
            // ----------------------------------------------------------
            auto postProcessTask = profiler->AddTask("Tonemapping", Colors::carrot);
            postProcessTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            postProcessShader.UseProgram();
            postProcessShader.SetFloat("exposure", tonemapExposure);
            glBindVertexArray(screenQuadVAO);
            glActiveTexture(GL_TEXTURE0 + ACCUMULATION_BUFFER_BINDING);
            glBindTexture(GL_TEXTURE_2D, lightAccumulationTexture); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

            postProcessTask->End();

            // 6) Final Pass (Antialiasing):
            // -----------------------------
            auto FXAATask = profiler->AddTask("FXAA Pass", Colors::alizarin);
            FXAATask->Start();

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

            FXAATask->End();
        }




        void ReloadShaders()
        {
            //Material specific shaders. These should dynamically load depending on available scene materials
            defaultVertFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferAlbedo.frag");
            defaultVertFrag.BuildProgram();
            defaultVertFrag.BindUniformBlocks(NamedBufferBindings,3);


            
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            defaultVertTexFrag.BuildProgram();
            defaultVertTexFrag.BindUniformBlocks(NamedBufferBindings,3);



            defaultVertNormalTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            if (enableNormalMaps)
            {
                std::string s = "NORMAL_MAPPED";
                defaultVertNormalTexFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertNormalTexFrag.BuildProgram();
            defaultVertNormalTexFrag.BindUniformBlocks(NamedBufferBindings,3);
            


            defaultVertUnlitFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            defaultVertUnlitFrag.BuildProgram();
            defaultVertUnlitFrag.BindUniformBlocks(NamedBufferBindings,3);
            


            directionalLightingPass = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/deferred/fsDeferredLighting.frag");
            directionalLightingPass.AddPreProcessorDefines(preprocessorDefines);
            if (enableLightVolumes)
            {
                std::string s = "LIGHT_VOLUMES";
                directionalLightingPass.AddPreProcessorDefines(&s,1);
            }
            if (enableShadowMapping)
            {
                std::string s = "DIR_LIGHT_SHADOWS";
                directionalLightingPass.AddPreProcessorDefines(&s,1);
            }
            directionalLightingPass.BuildProgram();
            directionalLightingPass.UseProgram();
            directionalLightingPass.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            directionalLightingPass.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            directionalLightingPass.SetInt("gPosition", POSITION_BUFFER_BINDING);
            directionalLightingPass.SetInt("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            directionalLightingPass.BindUniformBlock(NamedBufferBindings[UNIFORM_GLOBAL_MATRICES_BINDING], UNIFORM_GLOBAL_MATRICES_BINDING);
            directionalLightingPass.BindUniformBlock(NamedBufferBindings[UNIFORM_GLOBAL_LIGHTS_BINDING], UNIFORM_GLOBAL_LIGHTS_BINDING);
            directionalLightingPass.BindUniformBlock(NamedBufferBindings[UNIFORM_GLOBAL_SHADOWS_BINDING], UNIFORM_GLOBAL_SHADOWS_BINDING);



            simpleDepthPass = Shader(BASE_DIR"/data/shaders/simpleVert.vert", BASE_DIR"/data/shaders/nullFrag.frag");
            simpleDepthPass.BuildProgram();

            

            pointLightVolShader = Shader(BASE_DIR"/data/shaders/simpleVert.vert", BASE_DIR"/data/shaders/deferred/pointVolumeLighting.frag");
            pointLightVolShader.AddPreProcessorDefines(preprocessorDefines);
            pointLightVolShader.BuildProgram();
            pointLightVolShader.UseProgram();
            pointLightVolShader.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            pointLightVolShader.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            pointLightVolShader.SetInt("gPosition", POSITION_BUFFER_BINDING);
            pointLightVolShader.BindUniformBlock(NamedBufferBindings[UNIFORM_GLOBAL_LIGHTS_BINDING],UNIFORM_GLOBAL_LIGHTS_BINDING);



            postProcessShader = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadTonemapLum.frag");
            postProcessShader.BuildProgram();
            postProcessShader.UseProgram();
            postProcessShader.SetInt("screenTexture", ACCUMULATION_BUFFER_BINDING);



            FXAAShader = Shader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadFXAA.frag");
            FXAAShader.BuildProgram();




            voxelizationShader = Shader(BASE_DIR"/data/shaders/voxelization/voxel.vert", BASE_DIR"/data/shaders/voxelization/voxel.frag");
            voxelizationShader.AddPreProcessorDefines(preprocessorDefines);
            voxelizationShader.AddShaderStage(BASE_DIR"/data/shaders/voxelization/voxel.geom", GL_GEOMETRY_SHADER);
            voxelizationShader.BuildProgram();
            voxelizationShader.BindUniformBlocks(NamedBufferBindings, 5);




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
             *    mat4 inverseViewMatrix
             *    mat4 SceneMatrices[3];
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
            glBufferData(GL_UNIFORM_BUFFER, 6 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
            glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            // Bind a certain range of the buffer to the uniform block: this allows to use multiple UBOs 
            // per uniform block
            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_GLOBAL_MATRICES_BINDING, GlobalMatricesUBO, 0, 6 * sizeof(glm::mat4));
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

            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_MATERIAL_PROPERTIES_BINDING, MaterialUBO, 0, MaterialBufferSize);


        }

    private:
        OPProfiler::OPProfiler *profiler;

        ShadowRenderer shadowRenderer;
        SkyRenderer skyRenderer;

        unsigned int viewportWidth;
        unsigned int viewportHeight;

        GLuint gBufferFBO;
        GLuint gColorBuffer, gNormalBuffer, gPositionBuffer;
        GLuint depthStencilBuffer;

        GLuint lightAccumulationFBO;
        GLuint lightAccumulationTexture;

        GLuint postProcessFBO;
        GLuint postProcessColorBuffer;

        GLuint LightBufferSize = 0;
        GLuint MaterialBufferSize = 0;

        GLuint GlobalMatricesUBO;
        GLuint LocalMatricesUBO;
        GLuint LightsUBO;
        GLuint MaterialUBO;



        Shader defaultVertFrag;
        Shader defaultVertTexFrag;
        Shader defaultVertNormalTexFrag;
        Shader defaultVertUnlitFrag;


        
        GLuint voxelFBO;
        GLuint sparseListBuffer;
        GLuint voxelBuffer;
        GLuint voxel2DTex;
        unsigned int voxelRes = 256;
        GLuint numMipLevels;
        Shader voxelizationShader;




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
        

        Shader simpleDepthPass; 

        Shader pointLightVolShader;
        std::shared_ptr<Mesh> pointLightVolume;
        
        
        Shader postProcessShader;
        Shader FXAAShader;




        //indirect buffers:
        struct DrawElementsIndirectCommand 
        {
            GLuint vertexCount;
            GLuint instanceCount;
            GLuint firstVertex;
            GLuint baseVertex;
            GLuint baseInstance;
        };

        struct ComputeIndirectCommand 
        {
            GLuint workGroupSizeX;
            GLuint workGroupSizeY;
            GLuint workGroupSizeZ;
        };

        // Draw indirect buffer and struct
        DrawElementsIndirectCommand drawIndCmd[10];
        GLuint drawIndBuffer;

        // Compute indirect buffer and struct
        ComputeIndirectCommand compIndCmd[10];
        GLuint compIndBuffer;

        void SetupDrawInd()
        {
            // Initialize the indirect drawing buffer
            for(int i = MAX_MIP_MAP_LEVELS, j = 0; i >= 0; i--, j++) 
            {
                //drawIndCmd[i].vertexCount = (GLuint)voxelModel->GetNumIndices();
                drawIndCmd[i].instanceCount = 0;
                drawIndCmd[i].firstVertex = 0;
                drawIndCmd[i].baseVertex = 0;

                if(i == 0) 
                    drawIndCmd[i].baseInstance = 0;
                else if(i == MAX_MIP_MAP_LEVELS) 
                    drawIndCmd[i].baseInstance = MAX_SPARSE_BUFFER_SIZE - 1;
                else 
                    drawIndCmd[i].baseInstance = drawIndCmd[i + 1].baseInstance - (1 << (3 * j));
                
            }

            // Draw Indirect Command buffer for drawing voxels
            glGenBuffers(1, &drawIndBuffer);
            //the buffer will be filled in a shader as a storage buffer and will be read for an indirect draw as a draw indirect buffer:
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INDIRECT_BINDING, drawIndBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(drawIndCmd), drawIndCmd, GL_STREAM_DRAW);
        }
        void SetupCompInd()
        {
            // Initialize the indirect compute buffer
            for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) {
                compIndCmd[i].workGroupSizeX = 0;
                compIndCmd[i].workGroupSizeY = 1;
                compIndCmd[i].workGroupSizeZ = 1;
            }

            // Draw Indirect Command buffer for drawing voxels
            glGenBuffers(1, &compIndBuffer);
            //the buffer will be filled in a shader as a storage buffer and will be read for a compute buffer:
            glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COMPUTE_INDIRECT_BINDING, compIndBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(compIndCmd), compIndCmd, GL_STREAM_DRAW);
        }
};

#endif