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
        static constexpr bool enableLightVolumes = true;
        
        const int MAX_DIR_LIGHTS = 5;
        const int MAX_POINT_LIGHTS = 20;


        float tonemapExposure = 0.3f;
        float FXAAContrastThreshold = 0.0312f;
        float FXAABrightnessThreshold = 0.063f;
        unsigned int voxelRes = 256;
        bool drawVoxels = false;

        float aoDistance = 0.03f;
        float maxConeDistance = 1.5f;



        std::unordered_map<std::string, unsigned int> preprocessorDefines =
        {
            {"MAX_DIR_LIGHTS", MAX_DIR_LIGHTS},
            {"MAX_POINT_LIGHTS", MAX_POINT_LIGHTS},
            {"SHADOW_CASCADE_COUNT", SHADOW_CASCADE_COUNT}
        };

        // Adopted naming conventions for the global uniform blocks
        std::string NamedUniformBufferBindings[5] = { // The indexes have to match values in the enum
            "GlobalMatrices",
            "LocalMatrices",
            "MaterialProperties",
            "Lights",
            "Shadows",

        };

        enum gBufferPassBindings
        {
            G_COLOR_SPEC_BUFFER_BINDING = 0,
            G_NORMAL_BUFFER_BINDING = 1,
            G_POSITION_BUFFER_BINDING = 2,
        };


        enum VoxelizationPassBindings
        {
            VX_COLOR_SPEC_BINDING = 0,
            VX_VOXEL2DTEX_BINDING = 1,
            VX_VOXEL3DTEX_BINDING = 2,
            VX_SHADOW_MAP0_BINDING = 3,
        };
        
        enum GIPassBindings
        {
            GI_VOXEL2DTEX_BINDING = 0,
            GI_VOXEL3DTEX_BINDING = 1,
            GI_SHADOW_MAP0_BINDING = 2,
            GI_COLOR_SPEC_BINDING = 3,
            GI_NORMAL_BINDING = 4,
            GI_POSITION_BINDING = 5,

        };

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
        
        
        
        VCTGIRenderer(unsigned int vpWidth, unsigned int vpHeight)
        {
            this->viewportWidth = vpWidth;
            this->viewportHeight = vpHeight;
        }

        void RecreateResources(Scene &scene, Camera &camera)
        {
            scene.MAX_DIR_LIGHTS = MAX_DIR_LIGHTS;
            scene.MAX_POINT_LIGHTS = MAX_POINT_LIGHTS;


            // Shadow maps:
            if (enableShadowMapping)
            {
                this->shadowRenderer = CascadedShadowRenderer(
                    camera.Near,
                    camera.Far,
                    UNIFORM_GLOBAL_SHADOWS_BINDING, 
                    SHADOW_CASCADE_COUNT,
                    SHADOW_WIDTH,
                    SHADOW_HEIGHT
                );
                this->shadowRenderer.RecreateResources();

                this->VSMRenderer = VarianceShadowRenderer(
                    camera.Near,
                    camera.Far,
                    UNIFORM_GLOBAL_SHADOWS_BINDING, 
                    SHADOW_CASCADE_COUNT,
                    SHADOW_WIDTH,
                    SHADOW_HEIGHT
                );
                this->VSMRenderer.RecreateResources();
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
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + G_COLOR_SPEC_BUFFER_BINDING, GL_TEXTURE_2D, gColorBuffer, 0);
            
            // 2) View space normal buffer attachment 
            glGenTextures(1, &gNormalBuffer);
            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + G_NORMAL_BUFFER_BINDING, GL_TEXTURE_2D, gNormalBuffer, 0);

            // 3) View space position buffer attachment 
            glGenTextures(1, &gPositionBuffer);
            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth, viewportHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0); 
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + G_POSITION_BUFFER_BINDING, GL_TEXTURE_2D, gPositionBuffer, 0);

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
                GL_COLOR_ATTACHMENT0 + G_COLOR_SPEC_BUFFER_BINDING, 
                GL_COLOR_ATTACHMENT0 + G_NORMAL_BUFFER_BINDING, 
                GL_COLOR_ATTACHMENT0 + G_POSITION_BUFFER_BINDING
            };
            glDrawBuffers(3, attachments);



            // Voxelization:
            glGenFramebuffers(1, &voxelFBO);

            //setup sparse active voxel list (filled during voxelization):
            glGenBuffers(1, &sparseListBuffer);
	        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SPARSE_LIST_BINDING, sparseListBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, (sizeof(GLuint) * MAX_SPARSE_BUFFER_SIZE), NULL, GL_STREAM_DRAW);

            MeshData voxelMeshData = MeshData::LoadMeshDataFromFile(BASE_DIR "/data/models/voxel.obj");
            voxelMesh = std::make_shared<Mesh>(voxelMeshData);
            voxelPosBinding = voxelMesh->AddVertexAttribute(sparseListBuffer);

            //contains data for drawing the voxels with indirect commands and for generating mipmaps:
            SetupDrawInd((GLuint)voxelMesh->indicesCount);

            //contains the workgroup structure used for the mipmap compute:
            //this will represent a 1d array of workgroups (of size 64) that will process the sparse voxel list and the 3d texture into the mipmaps
            SetupCompInd();

            numMipLevels = (GLuint)log2(voxelRes);
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

            




            // Light Accumulation (conetracing):
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
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lightAccumulationTexture, 0);
            // Bind the same depth buffer for drawing light volumes
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }
            glDrawBuffer(GL_COLOR_ATTACHMENT0);




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


        void RenderFrame(Camera &camera, Scene *scene, GLFWwindow *window, OPProfiler::OPProfiler *profiler)
        {
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

            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, 2*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(inverseViewMatrix));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 


            int offset = 0;
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
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

            

            // 1) Shadow Map Rendering Pass:
            // -----------------------------
            auto shadowTask = profiler->AddTask("Shadow Pass", Colors::sunFlower);
            shadowTask->Start();
            std::vector<unsigned int> shadowMapBuffers = {0};
            if (enableShadowMapping)
            {
                shadowMapBuffers = shadowRenderer.Render(frameResources);
                std::vector<unsigned int> vsmBuffers = VSMRenderer.Render(frameResources);
            }
            shadowTask->End();


            // 2) gBuffer Pass:
            // ----------------
            auto gbufferTask = profiler->AddTask("gBuffer Pass", Colors::emerald);
            gbufferTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLuint shaderCache = 0;

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                StandardShader activeShader;
                GLuint activeRoutine;

                if (materialInstance->HasFlag(OP_MATERIAL_UNLIT))
                {
                    return;
                }
                else if (materialInstance->HasFlags(OP_MATERIAL_TEXTURED_DIFFUSE | OP_MATERIAL_TEXTURED_NORMAL))
                {
                    activeShader = defaultVertNormalTexFrag;
                    activeRoutine = 1;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertFrag;
                    activeRoutine = 1;
                }
                else
                {
                    activeShader = defaultVertFrag;
                    activeRoutine = 0;
                }

                // shader has to be used before updating the uniforms
                if (activeShader.ID != shaderCache)
                {
                    activeShader.UseProgram();
                    shaderCache = activeShader.ID;
                }

                // setting if the color is sampled from texture or from UBO
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

                    activeShader.SetSamplerBinding((name + number).c_str(), i);
                    glBindTexture(GL_TEXTURE_2D, texture.id);
                }
                
                //bind VAO
                mesh->BindBuffers();

                //Indexed drawing
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });  

            gbufferTask->End();



            // 3) Voxelization Pass 
            auto voxelizationTask = profiler->AddTask("voxelization", Colors::belizeHole);
            voxelizationTask->Start();

            //these are bound in dispatch indirect and shader storage, since they are first filled in the voxelization (storage) and then read in the mipmap compute shader (indirect)
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndBuffer);     
            glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INDIRECT_BINDING, drawIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COMPUTE_INDIRECT_BINDING, compIndBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SPARSE_LIST_BINDING, sparseListBuffer);


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
            

            glBindImageTexture(VX_VOXEL2DTEX_BINDING, voxel2DTex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
            glBindImageTexture(VX_VOXEL3DTEX_BINDING, voxelBuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

            glActiveTexture(GL_TEXTURE0 + VX_SHADOW_MAP0_BINDING);
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffers[0]);

            
            glDisable(GL_CULL_FACE); // all faces must be rendered
            
            // 1st part of voxelization:
            voxelizationShader.UseProgram();
            voxelizationShader.SetUInt("voxelRes", voxelRes);
            
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
                    glActiveTexture(GL_TEXTURE0 + VX_COLOR_SPEC_BINDING); 

                    std::string number;
                    TextureType type = texture.type;

                    if (texture.type == OP_TEXTURE_DIFFUSE)
                    {
                        voxelizationShader.SetSamplerBinding("texture_diffuse1", 0);
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
            voxelizationTask->End();


            // 2nd part of voxelization: Mipmapping
            auto mipmappingTask = profiler->AddTask("mipmapping", Colors::pomegranate);
            mipmappingTask->Start();

            mipmappingShader.UseProgram();
            for(GLuint level = 0; level < numMipLevels; level++) 
            {
                glBindImageTexture(3, voxelBuffer, level, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
                glBindImageTexture(4, voxelBuffer, level + 1, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

                mipmappingShader.SetUInt("currentLevel", level);

                glDispatchComputeIndirect(NULL);
                
                // Wait for buffer updates before reading and writing to next mip:
                //GL_SHADER_IMAGE_ACCESS_BARRIER_BIT: for the voxel buffer
                //GL_SHADER_STORAGE_BARRIER_BIT: for the storage buffers (drawIndBuffer, compIndBuffer and sparseListBuffer)
                //GL_COMMAND_BARRIER_BIT: for the indirect buffers (drawIndBuffer and compIndBuffer)
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
            }
            //barriers for other gl commands involving the buffers and textures 
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

            mipmappingTask->End();

            
            //draw voxels
            if (drawVoxels)
            {
                auto drawVoxelsTask = profiler->AddTask("drawVoxels", Colors::wisteria);
                drawVoxelsTask->Start();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);    
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);

                glActiveTexture(GL_TEXTURE0 + GI_VOXEL3DTEX_BINDING);
                glBindTexture(GL_TEXTURE_3D, voxelBuffer);

                unsigned int mipLevel = 0;
                
                drawVoxelsShader.UseProgram();
                drawVoxelsShader.SetUInt("mipLevel", mipLevel);
                drawVoxelsShader.SetUInt("voxelRes", voxelRes);

                voxelMesh->BindBuffers();
                
                glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)(sizeof(DrawElementsIndirectCommand) * mipLevel)); // control this parameter with imgui
                
                drawVoxelsTask->End();

                
                return; // EARLY STOP
            }
            



            // 4) Conetrace Pass
            auto conetraceTask = profiler->AddTask("Cone tracing", Colors::orange);
            conetraceTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, lightAccumulationFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            glActiveTexture(GL_TEXTURE0 + GI_VOXEL2DTEX_BINDING);
            glBindTexture(GL_TEXTURE_2D_ARRAY, voxel2DTex);
            glActiveTexture(GL_TEXTURE0 + GI_VOXEL3DTEX_BINDING);
            glBindTexture(GL_TEXTURE_3D, voxelBuffer);
            glActiveTexture(GL_TEXTURE0 + GI_SHADOW_MAP0_BINDING);
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapBuffers[0]);
            glActiveTexture(GL_TEXTURE0 + GI_COLOR_SPEC_BINDING); 
            glBindTexture(GL_TEXTURE_2D, gColorBuffer);
            glActiveTexture(GL_TEXTURE0 + GI_NORMAL_BINDING);
            glBindTexture(GL_TEXTURE_2D, gNormalBuffer);
            glActiveTexture(GL_TEXTURE0 + GI_POSITION_BINDING);
            glBindTexture(GL_TEXTURE_2D, gPositionBuffer);


            conetraceShader.UseProgram();
            conetraceShader.SetUInt("voxelRes", voxelRes);
            conetraceShader.SetFloat("aoDistance", aoDistance);
            conetraceShader.SetFloat("maxConeDistance", maxConeDistance);

            glBindVertexArray(screenQuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glEnable(GL_DEPTH_TEST);
            conetraceTask->End();

            

            
            
            // 5) Unlit Pass (render objects with different lighting models using same depth buffer):
            // --------------------------------------------------------------------------------------
            auto renderUnlitTask = profiler->AddTask("Unlit & Sky Pass", Colors::greenSea);
            renderUnlitTask->Start();

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                StandardShader activeShader;
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

        
            // 6) Postprocess Pass: apply tonemap to the HDR color buffer
            // ----------------------------------------------------------
            auto postProcessTask = profiler->AddTask("Tonemapping", Colors::carrot);
            postProcessTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            postProcessShader.UseProgram();
            postProcessShader.SetFloat("exposure", tonemapExposure);
            glBindVertexArray(screenQuadVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, lightAccumulationTexture); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

            postProcessTask->End();

            // 7) Final Pass (Antialiasing):
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
            defaultVertFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            defaultVertFrag.BuildProgram();
            defaultVertFrag.BindUniformBlocks(NamedUniformBufferBindings,3);



            defaultVertNormalTexFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            if (enableNormalMaps)
            {
                std::string s = "NORMAL_MAPPED";
                defaultVertNormalTexFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertNormalTexFrag.BuildProgram();
            defaultVertNormalTexFrag.BindUniformBlocks(NamedUniformBufferBindings,3);
            


            defaultVertUnlitFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            defaultVertUnlitFrag.BuildProgram();
            defaultVertUnlitFrag.BindUniformBlocks(NamedUniformBufferBindings,3);



            postProcessShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadTonemapLum.frag");
            postProcessShader.BuildProgram();
            postProcessShader.UseProgram();
            postProcessShader.SetSamplerBinding("screenTexture", 0);



            FXAAShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadFXAA.frag");
            FXAAShader.BuildProgram();




            voxelizationShader = StandardShader(BASE_DIR"/data/shaders/voxelization/voxel.vert", BASE_DIR"/data/shaders/voxelization/voxel.frag");
            voxelizationShader.AddPreProcessorDefines(preprocessorDefines);
            voxelizationShader.AddShaderStage(BASE_DIR"/data/shaders/voxelization/voxel.geom", GL_GEOMETRY_SHADER);
            voxelizationShader.BuildProgram();
            voxelizationShader.UseProgram();
            voxelizationShader.SetSamplerBinding("texture_diffuse1", VX_COLOR_SPEC_BINDING);
            voxelizationShader.SetSamplerBinding("voxelTextures", VX_VOXEL2DTEX_BINDING);
            voxelizationShader.SetSamplerBinding("voxelData", VX_VOXEL3DTEX_BINDING);
            voxelizationShader.SetSamplerBinding("shadowMap0", VX_SHADOW_MAP0_BINDING); // do the binding properly
            voxelizationShader.BindUniformBlocks(NamedUniformBufferBindings, 5);

            mipmappingShader = ComputeShader(BASE_DIR"/data/shaders/voxelization/mipmap.comp");
            mipmappingShader.AddPreProcessorDefines(preprocessorDefines);
            mipmappingShader.BuildProgram();
            mipmappingShader.BindUniformBlocks(NamedUniformBufferBindings, 5);


            conetraceShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/voxelization/conetraceGI.frag");
            conetraceShader.AddPreProcessorDefines(preprocessorDefines);
            if (enableShadowMapping)
            {
                std::string s = "DIR_LIGHT_SHADOWS";
                conetraceShader.AddPreProcessorDefines(&s,1);
            }
            conetraceShader.BuildProgram();
            conetraceShader.UseProgram();
            conetraceShader.SetSamplerBinding("voxel2DTextures", GI_VOXEL2DTEX_BINDING); 
            conetraceShader.SetSamplerBinding("voxel3DData", GI_VOXEL3DTEX_BINDING);
            conetraceShader.SetSamplerBinding("shadowMap0", GI_SHADOW_MAP0_BINDING);
            conetraceShader.SetSamplerBinding("gColorSpec", GI_COLOR_SPEC_BINDING); 
            conetraceShader.SetSamplerBinding("gNormal", GI_NORMAL_BINDING);
            conetraceShader.SetSamplerBinding("gPosition", GI_POSITION_BINDING);
            conetraceShader.BindUniformBlocks(NamedUniformBufferBindings, 5);


            drawVoxelsShader = StandardShader(BASE_DIR"/data/shaders/voxelization/drawVoxels.vert", BASE_DIR"/data/shaders/voxelization/drawVoxels.frag");
            drawVoxelsShader.AddPreProcessorDefines(preprocessorDefines);
            if (enableShadowMapping)
            {
                std::string s = "DIR_LIGHT_SHADOWS";
                drawVoxelsShader.AddPreProcessorDefines(&s,1);
            }
            drawVoxelsShader.BuildProgram();
            drawVoxelsShader.UseProgram();
            drawVoxelsShader.SetSamplerBinding("voxel2DTextures", GI_VOXEL2DTEX_BINDING); 
            drawVoxelsShader.SetSamplerBinding("voxel3DData", GI_VOXEL3DTEX_BINDING);
            drawVoxelsShader.BindUniformBlocks(NamedUniformBufferBindings, 5);






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
             *    mat4 voxelMatrix;
             *    mat4 inverseVoxelMatrix;
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
            glBufferData(GL_UNIFORM_BUFFER, 5*sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);

            auto voxelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.02,0.02,0.02)); //calculate based on the scene size or the frustrum bounds !!
            auto invVoxelMatrix = glm::inverse(voxelMatrix);
            glBufferSubData(GL_UNIFORM_BUFFER, 3*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(voxelMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, 4*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(invVoxelMatrix));
            
            glBindBuffer(GL_UNIFORM_BUFFER, LocalMatricesUBO);
            glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            // Bind a certain range of the buffer to the uniform block: this allows to use multiple UBOs 
            // per uniform block
            glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_GLOBAL_MATRICES_BINDING, GlobalMatricesUBO, 0, 5 * sizeof(glm::mat4));
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

        void RenderGUI()
        {
            ImGui::Begin("VCTGI");
            ImGui::Text("VCTGI");
            ImGui::End();
        }

    private:
        CascadedShadowRenderer shadowRenderer;
        VarianceShadowRenderer VSMRenderer;
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



        StandardShader defaultVertFrag;
        StandardShader defaultVertNormalTexFrag;
        StandardShader defaultVertUnlitFrag;




        std::shared_ptr<Mesh> voxelMesh;
        unsigned int voxelPosBinding;
        
        GLuint voxelFBO;
        GLuint sparseListBuffer;
        GLuint voxelBuffer;
        GLuint voxel2DTex;
        GLuint numMipLevels;
        StandardShader voxelizationShader;
        ComputeShader mipmappingShader;
        StandardShader conetraceShader;
        StandardShader drawVoxelsShader;



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
    
        
        
        StandardShader postProcessShader;
        StandardShader FXAAShader;




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

        void SetupDrawInd(GLuint vertCount)
        {
            // Initialize the indirect drawing buffer
            for(int mip = MAX_MIP_MAP_LEVELS, i = 0; mip >= 0; mip--, i++) 
            {
                drawIndCmd[mip].vertexCount = vertCount;
                drawIndCmd[mip].instanceCount = 0;
                drawIndCmd[mip].firstVertex = 0;
                drawIndCmd[mip].baseVertex = 0;

                //base instance will be the offset into the sparse list buffer
                if(mip == 0) 
                    drawIndCmd[mip].baseInstance = 0;
                else if(mip == MAX_MIP_MAP_LEVELS) 
                    drawIndCmd[mip].baseInstance = MAX_SPARSE_BUFFER_SIZE - 1;
                else 
                    //the offsets will increase exponentially due to the mipmapping, the lower mips will have more voxels and when increasing mip level, we reduce the amount of voxels
                    //by a factor of 2**3                                              - (2^(3 * i))
                    drawIndCmd[mip].baseInstance = drawIndCmd[mip + 1].baseInstance - (1 << (3 * i)); 
                
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
            for(size_t mip = 0; mip <= MAX_MIP_MAP_LEVELS; mip++) 
            {
                compIndCmd[mip].workGroupSizeX = 0;
                compIndCmd[mip].workGroupSizeY = 1;
                compIndCmd[mip].workGroupSizeZ = 1;
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