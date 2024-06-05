#ifndef VCTGI_CLIPMAPPED_RENDERER_H
#define VCTGI_CLIPMAPPED_RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../BaseRenderer.h"
#include "../render_features/ShadowRenderer.h"
#include "../render_features/SkyRenderer.h"
#include "../../debug/OPProfiler.h"
#include "../../common/Colors.h"


class CMVCTGIRenderer : public BaseRenderer
{
    public:
        static constexpr unsigned int MAX_MIP_MAP_LEVELS = 9;
        static constexpr unsigned int MAX_SPARSE_BUFFER_SIZE = 134217728;

        const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
        static constexpr unsigned int SHADOW_CASCADE_COUNT = 3; // MAX == 4

        static constexpr bool enableNormalMaps = true;
        static constexpr bool enableLightVolumes = true;
        
        static constexpr int MAX_DIR_LIGHTS = 5;
        static constexpr int MAX_POINT_LIGHTS = 20;

        static constexpr glm::vec3 voxelWorldScale = glm::vec3(0.025,0.025,0.025); //0.025,0.025,0.025 //sponza
        

        enum CMVCTGIShadowRenderer
        {
            NONE,
            PCF_SHADOW_MAP,
            VSM_SHADOW_MAP
        };
        CMVCTGIShadowRenderer activeShadowRenderer = PCF_SHADOW_MAP;

        float tonemapExposure = 1.3f;
        float FXAAContrastThreshold = 0.0312f;
        float FXAABrightnessThreshold = 0.063f;
        unsigned int voxelRes = 512;

        bool drawVoxels = true;
        int mipLevel = 0;

        float aoDistance = 0.03f;
        float maxConeDistance = 1.0f;
        float accumThr = 1.1f;
        int maxLOD = 10;


        enum GBufferPassInputBindings
        {
            DIFFUSE_TEXTURE0_BINDING = 1,
            NORMAL_TEXTURE0_BINDING = 2,
            SPECULAR_TEXTURE0_BINDING = 4,
        };
        enum GBufferPassOutputBindings
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

        
        enum CMVCTGIShaderStorageBindings 
        {
            DRAW_INDIRECT_BINDING = 0,
            COMPUTE_INDIRECT_BINDING = 1,
            SPARSE_LIST_BINDING = 2
        };
        
        
        
        CMVCTGIRenderer(unsigned int vpWidth, unsigned int vpHeight)
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


            switch (activeShadowRenderer)
            {
                case PCF_SHADOW_MAP:
                    this->PCFshadowRenderer = PCFShadowRenderer(
                        SHADOW_CASCADE_COUNT,
                        SHADOW_WIDTH,
                        SHADOW_HEIGHT
                    );
                    this->PCFshadowRenderer.RecreateResources(&shaderMemoryPool);
                    preprocessorDefines.push_back("DIR_LIGHT_SHADOWS");
                    preprocessorDefines.push_back("PCF_SHADOWS");
                    break;

                case VSM_SHADOW_MAP:
                    this->VSMShadowRenderer = VarianceShadowRenderer(
                        0, 
                        SHADOW_CASCADE_COUNT,
                        SHADOW_WIDTH,
                        SHADOW_HEIGHT
                    );
                    this->VSMShadowRenderer.RecreateResources();
                    preprocessorDefines.push_back("DIR_LIGHT_SHADOWS");
                    preprocessorDefines.push_back("VSM_SHADOWS");
                    break;
                
                default:
                    break;
            }

            this->skyRenderer = SkyRenderer();
            this->skyRenderer.RecreateResources();


            // gBuffer:
            glGenFramebuffers(1, &gBufferFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

            TextureDescriptor gBufferTexDescriptor = TextureDescriptor();
            gBufferTexDescriptor.GLType = GL_TEXTURE_2D;
            gBufferTexDescriptor.sizedInternalFormat = GL_RGBA16F;
            gBufferTexDescriptor.internalFormat = GL_RGBA;
            gBufferTexDescriptor.pixelFormat = GL_FLOAT;
            gBufferTexDescriptor.width = viewportWidth;
            gBufferTexDescriptor.height = viewportHeight;
            gBufferTexDescriptor.minFilter = GL_LINEAR;
            gBufferTexDescriptor.magFilter = GL_LINEAR;

            // 1) HDR color + specular buffer attachment 
            gColorBuffer = Texture2D(gBufferTexDescriptor);
            gColorBuffer.BindToTarget(gBufferFBO, GL_COLOR_ATTACHMENT0 + G_COLOR_SPEC_BUFFER_BINDING);
            
            // 2) View space normal buffer attachment 
            gNormalBuffer = Texture2D(gBufferTexDescriptor);
            gNormalBuffer.BindToTarget(gBufferFBO, GL_COLOR_ATTACHMENT0 + G_NORMAL_BUFFER_BINDING);

            // 3) View space position buffer attachment 
            gPositionBuffer = Texture2D(gBufferTexDescriptor);
            gPositionBuffer.BindToTarget(gBufferFBO, GL_COLOR_ATTACHMENT0 + G_POSITION_BUFFER_BINDING);

            //setting the depth and stencil attachments
            depthStencilBuffer = RenderBuffer2D(GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);
            depthStencilBuffer.BindToTarget(gBufferFBO, GL_DEPTH_STENCIL_ATTACHMENT);


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
            glGenTextures(1, &voxel3DTex);
            glBindTexture(GL_TEXTURE_3D, voxel3DTex);
            glTexStorage3D(GL_TEXTURE_3D, numMipLevels + 1, GL_R32UI, voxelRes, voxelRes, voxelRes);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
            float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, borderColor);  

            // For debugging
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

            TextureDescriptor lightBufferDescriptor = TextureDescriptor();
            lightBufferDescriptor.GLType = GL_TEXTURE_2D;
            lightBufferDescriptor.sizedInternalFormat = GL_RGBA16F;
            lightBufferDescriptor.internalFormat = GL_RGBA;
            lightBufferDescriptor.pixelFormat = GL_FLOAT;
            lightBufferDescriptor.width = viewportWidth;
            lightBufferDescriptor.height = viewportHeight;
            lightBufferDescriptor.minFilter = GL_LINEAR;
            lightBufferDescriptor.magFilter = GL_LINEAR;

            lightAccumulationBuffer = Texture2D(lightBufferDescriptor);
            lightAccumulationBuffer.BindToTarget(lightAccumulationFBO, GL_COLOR_ATTACHMENT0);

            // Bind the same depth buffer for drawing light volumes
            depthStencilBuffer.BindToTarget(lightAccumulationFBO, GL_DEPTH_STENCIL_ATTACHMENT);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw RendererException("ERROR::FRAMEBUFFER:: Intermediate Framebuffer is incomplete");
            }
            glDrawBuffer(GL_COLOR_ATTACHMENT0);


            // Postprocessing Pass (Tonemapping): 
            glGenFramebuffers(1, &postProcessFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);

            TextureDescriptor postProcessBufferDescriptor = TextureDescriptor();
            postProcessBufferDescriptor.GLType = GL_TEXTURE_2D;
            postProcessBufferDescriptor.sizedInternalFormat = GL_RGBA16F;
            postProcessBufferDescriptor.internalFormat = GL_RGBA;
            postProcessBufferDescriptor.pixelFormat = GL_FLOAT;
            postProcessBufferDescriptor.width = viewportWidth;
            postProcessBufferDescriptor.height = viewportHeight;
            postProcessBufferDescriptor.minFilter = GL_LINEAR;
            postProcessBufferDescriptor.magFilter = GL_LINEAR;

            postProcessColorBuffer = Texture2D(postProcessBufferDescriptor);
            postProcessColorBuffer.BindToTarget(postProcessFBO, GL_COLOR_ATTACHMENT0);

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

            gColorBuffer.Resize(vpWidth, vpHeight);
            gNormalBuffer.Resize(vpWidth, vpHeight);
            gPositionBuffer.Resize(vpWidth, vpHeight);
            
            depthStencilBuffer.Resize(vpWidth, vpHeight);

            lightAccumulationBuffer.Resize(vpWidth, vpHeight);
            postProcessColorBuffer.Resize(vpWidth, vpHeight);
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
                auto voxelMatrix = glm::scale(glm::mat4(1.0f), voxelWorldScale); 
                auto invVoxelMatrix = glm::inverse(voxelMatrix);

                globalMatrices->projectionMatrix = projectionMatrix;
                globalMatrices->viewMatrix = viewMatrix;
                globalMatrices->inverseViewMatrix = inverseViewMatrix;
                globalMatrices->voxelMatrix = voxelMatrix;
                globalMatrices->inverseVoxelMatrix = invVoxelMatrix;
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
            auto shadowTask = profiler->AddTask("Shadow Pass", Colors::sunFlower);
            shadowTask->Start();

            ShadowsOutput shadowOut = {0, GL_TEXTURE_2D};

            switch (activeShadowRenderer)
            {
                case PCF_SHADOW_MAP:
                    shadowOut = PCFshadowRenderer.Render(frameResources);
                    break;

                case VSM_SHADOW_MAP:
                    shadowOut = VSMShadowRenderer.Render(frameResources);
                    break;
                
                default:
                    break;
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
                auto materialPropertiesBuffer = shaderMemoryPool.GetUniformBuffer("MaterialProperties");
                materialPropertiesBuffer->SetData(0, sizeof(MaterialProperties), &(materialInstance->properties));

                // Update model and normal matrices:
                auto localMatricesBuffer = shaderMemoryPool.GetUniformBuffer("LocalMatrices"); 
                localMatricesBuffer->SetData( 0, sizeof(glm::mat4), (void*)glm::value_ptr(objectToWorld));
                localMatricesBuffer->SetData( sizeof(glm::mat4), sizeof(glm::mat4), (void*)glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)) );

                //Bind all textures
                unsigned int diffuseBinding = DIFFUSE_TEXTURE0_BINDING;
                unsigned int normalBinding = NORMAL_TEXTURE0_BINDING;
                unsigned int specularBinding = SPECULAR_TEXTURE0_BINDING;

                for (unsigned int i = 0; i < materialInstance->GetNumTextures(OP_TEXTURE_DIFFUSE); i++)
                {
                    diffuseBinding = std::min(diffuseBinding, (unsigned int)NORMAL_TEXTURE0_BINDING);
                    
                    auto texName = materialInstance->GetDiffuseMapName(i);
                    scene->GetTexture(texName).BindForRead(diffuseBinding);
                    
                    diffuseBinding++;
                }

                for (unsigned int i = 0; i < materialInstance->GetNumTextures(OP_TEXTURE_NORMAL); i++)
                {
                    normalBinding = std::min(normalBinding++, (unsigned int)SPECULAR_TEXTURE0_BINDING);
                    auto texName = materialInstance->GetNormalMapName(i);
                    scene->GetTexture(texName).BindForRead(normalBinding);
                    
                    normalBinding++;
                }

                for (unsigned int i = 0; i < materialInstance->GetNumTextures(OP_TEXTURE_SPECULAR); i++)
                {
                    //specularBinding = std::min(specularBinding++, (unsigned int)SPECULAR_TEXTURE0_BINDING);
                    auto texName = materialInstance->GetSpecularMapName(i);
                    scene->GetTexture(texName).BindForRead(specularBinding);
                    
                    specularBinding++;
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
            glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
            
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
                glClearTexImage(voxel3DTex, (GLint)i, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

            // Reset the sparse voxel count
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawIndBuffer);
            for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) 
                glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, i * sizeof(DrawElementsIndirectCommand) + sizeof(GLuint), sizeof(GLuint), GL_RED, GL_UNSIGNED_INT, NULL); // Clear data before since data is used when drawing
            

            // Reset the sparse voxel count for compute shader
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, compIndBuffer);
            for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) 
                glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, i * sizeof(ComputeIndirectCommand), sizeof(GLuint), GL_RED, GL_UNSIGNED_INT, NULL); // Clear data before since data is used when drawing
            

            glBindImageTexture(VX_VOXEL2DTEX_BINDING, voxel2DTex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
            glBindImageTexture(VX_VOXEL3DTEX_BINDING, voxel3DTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

            glActiveTexture(GL_TEXTURE0 + VX_SHADOW_MAP0_BINDING);
            glBindTexture(shadowOut.texType0, shadowOut.shadowMap0);

            
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
                auto materialPropertiesBuffer = shaderMemoryPool.GetUniformBuffer("MaterialProperties");
                materialPropertiesBuffer->SetData(0, sizeof(MaterialProperties), &(materialInstance->properties));

                // Update model and normal matrices:
                auto localMatricesBuffer = shaderMemoryPool.GetUniformBuffer("LocalMatrices");
                localMatricesBuffer->SetData(0, sizeof(glm::mat4), (void*)glm::value_ptr(objectToWorld));
                localMatricesBuffer->SetData(sizeof(glm::mat4), sizeof(glm::mat4), (void*)glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)) );

                if (materialInstance->GetNumTextures(OP_TEXTURE_DIFFUSE) > 0)
                {
                    auto texName = materialInstance->GetDiffuseMapName(0);
                    scene->GetTexture(texName).BindForRead(VX_COLOR_SPEC_BINDING);
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
            glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
            voxelizationTask->End();


            // 2nd part of voxelization: Mipmapping
            auto mipmappingTask = profiler->AddTask("mipmapping", Colors::pomegranate);
            mipmappingTask->Start();

            mipmappingShader.UseProgram();
            for(GLuint level = 0; level < numMipLevels; level++) 
            {
                glBindImageTexture(3, voxel3DTex, level, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
                glBindImageTexture(4, voxel3DTex, level + 1, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

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
                glBindTexture(GL_TEXTURE_3D, voxel3DTex);
                
                drawVoxelsShader.UseProgram();
                drawVoxelsShader.SetUInt("mipLevel", mipLevel);
                drawVoxelsShader.SetUInt("voxelRes", voxelRes);

                voxelMesh->BindBuffers();
                
                glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)(sizeof(DrawElementsIndirectCommand) * mipLevel)); // control this parameter with imgui
                
                drawVoxelsTask->End();

                
                return; // EARLY STOP
            }
            


            /*
            // 4) Conetrace Pass
            auto conetraceTask = profiler->AddTask("Cone tracing", Colors::orange);
            conetraceTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, lightAccumulationFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            glActiveTexture(GL_TEXTURE0 + GI_VOXEL2DTEX_BINDING);
            glBindTexture(GL_TEXTURE_2D_ARRAY, voxel2DTex);
            glActiveTexture(GL_TEXTURE0 + GI_VOXEL3DTEX_BINDING);
            glBindTexture(GL_TEXTURE_3D, voxel3DTex);
            glActiveTexture(GL_TEXTURE0 + GI_SHADOW_MAP0_BINDING);
            glBindTexture(shadowOut.texType0, shadowOut.shadowMap0);
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
            conetraceShader.SetFloat("accumThr", accumThr);
            conetraceShader.SetFloat("maxLOD", maxLOD);

            screenQuad->BindBuffers();
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

                auto materialPropertiesBuffer = shaderMemoryPool.GetUniformBuffer("MaterialProperties");
                materialPropertiesBuffer->SetData(0, sizeof(MaterialProperties), &(materialInstance->properties));

                // Update model and normal matrices:
                auto localMatricesBuffer = shaderMemoryPool.GetUniformBuffer("LocalMatrices");
                localMatricesBuffer->SetData( 0, sizeof(glm::mat4), (void*)glm::value_ptr(objectToWorld));
                localMatricesBuffer->SetData( sizeof(glm::mat4), sizeof(glm::mat4), (void*)glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)) );


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
            screenQuad->BindBuffers();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, lightAccumulationBuffer); 
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
            screenQuad->BindBuffers();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer); 
            glDrawArrays(GL_TRIANGLES, 0, 6);

            FXAATask->End();       */
        }

        void ReloadShaders()
        {   
            auto bufferBindings = shaderMemoryPool.GetNamedBindings();


            defaultVertFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            defaultVertFrag.BuildProgram();
            defaultVertFrag.UseProgram();
            defaultVertFrag.SetSamplerBinding("texture_diffuse1", DIFFUSE_TEXTURE0_BINDING);
            defaultVertFrag.SetSamplerBinding("texture_normal1", NORMAL_TEXTURE0_BINDING);
            defaultVertFrag.SetSamplerBinding("texture_specular1", SPECULAR_TEXTURE0_BINDING);
            defaultVertFrag.BindUniformBlocks(bufferBindings);


            defaultVertNormalTexFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/deferred/gBufferTextured.frag");
            if (enableNormalMaps)
            {
                std::string s = "NORMAL_MAPPED";
                defaultVertNormalTexFrag.AddPreProcessorDefines(&s,1);
            }
            defaultVertNormalTexFrag.BuildProgram();
            defaultVertNormalTexFrag.UseProgram();
            defaultVertNormalTexFrag.SetSamplerBinding("texture_diffuse1", DIFFUSE_TEXTURE0_BINDING);
            defaultVertNormalTexFrag.SetSamplerBinding("texture_normal1", NORMAL_TEXTURE0_BINDING);
            defaultVertNormalTexFrag.SetSamplerBinding("texture_specular1", SPECULAR_TEXTURE0_BINDING);
            defaultVertNormalTexFrag.BindUniformBlocks(bufferBindings);


            defaultVertUnlitFrag = StandardShader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            defaultVertUnlitFrag.BuildProgram();
            defaultVertUnlitFrag.UseProgram();
            defaultVertUnlitFrag.SetSamplerBinding("texture_diffuse1", DIFFUSE_TEXTURE0_BINDING);
            defaultVertUnlitFrag.SetSamplerBinding("texture_normal1", NORMAL_TEXTURE0_BINDING);
            defaultVertUnlitFrag.SetSamplerBinding("texture_specular1", SPECULAR_TEXTURE0_BINDING);
            defaultVertUnlitFrag.BindUniformBlocks(bufferBindings);


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
            voxelizationShader.SetSamplerBinding("voxel3DData", VX_VOXEL3DTEX_BINDING);
            voxelizationShader.SetSamplerBinding("shadowMap0", VX_SHADOW_MAP0_BINDING); // do the binding properly
            voxelizationShader.BindUniformBlocks(bufferBindings);


            mipmappingShader = ComputeShader(BASE_DIR"/data/shaders/voxelization/mipmap.comp");
            mipmappingShader.AddPreProcessorDefines(preprocessorDefines);
            mipmappingShader.BuildProgram();
            mipmappingShader.BindUniformBlocks(bufferBindings);


            conetraceShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/voxelization/conetraceGI.frag");
            conetraceShader.AddPreProcessorDefines(preprocessorDefines);
            conetraceShader.BuildProgram();
            conetraceShader.UseProgram();
            conetraceShader.SetSamplerBinding("voxel2DTextures", GI_VOXEL2DTEX_BINDING); 
            conetraceShader.SetSamplerBinding("voxel3DData", GI_VOXEL3DTEX_BINDING);
            conetraceShader.SetSamplerBinding("shadowMap0", GI_SHADOW_MAP0_BINDING);
            conetraceShader.SetSamplerBinding("gColorSpec", GI_COLOR_SPEC_BINDING); 
            conetraceShader.SetSamplerBinding("gNormal", GI_NORMAL_BINDING);
            conetraceShader.SetSamplerBinding("gPosition", GI_POSITION_BINDING);
            conetraceShader.BindUniformBlocks(bufferBindings);


            drawVoxelsShader = StandardShader(BASE_DIR"/data/shaders/voxelization/drawVoxels.vert", BASE_DIR"/data/shaders/voxelization/drawVoxels.frag");
            drawVoxelsShader.AddPreProcessorDefines(preprocessorDefines);
            drawVoxelsShader.BuildProgram();
            drawVoxelsShader.UseProgram();
            drawVoxelsShader.SetSamplerBinding("voxel2DTextures", GI_VOXEL2DTEX_BINDING); 
            drawVoxelsShader.SetSamplerBinding("voxel3DData", GI_VOXEL3DTEX_BINDING);
            drawVoxelsShader.BindUniformBlocks(bufferBindings);
        }

        void RenderGUI()
        {
            ImGui::Begin("VCTGI");
            ImGui::SeparatorText("Postprocessing");
            ImGui::SliderFloat("Tonemap Exposure", &tonemapExposure, 0.0f, 10.0f, "exposure = %.3f");

            ImGui::SeparatorText("Voxelization");
            ImGui::Checkbox("Draw Voxels", &drawVoxels);
            ImGui::SliderInt("Mipmap Level", &mipLevel, 0, MAX_MIP_MAP_LEVELS);

            ImGui::SeparatorText("Cone Tracing");
            ImGui::SliderFloat("AO Distance", &aoDistance, 0.01f, 0.5f, "AO = %.003f");
            ImGui::SliderFloat("Max Cone Distance", &maxConeDistance, 0.0f, 10.0f, "Cone Distance = %.3f");
            ImGui::SliderFloat("Accumulation Threshold", &accumThr, 0.0f, 6.0f, "threshold = %.3f");
            ImGui::SliderInt("Max LOD Level", &maxLOD, 0, MAX_MIP_MAP_LEVELS +1);
            
            ImGui::End();
        }

    private:
        std::vector<std::string> preprocessorDefines;
        
        PCFShadowRenderer PCFshadowRenderer;
        VarianceShadowRenderer VSMShadowRenderer;
        SkyRenderer skyRenderer;

        unsigned int viewportWidth;
        unsigned int viewportHeight;

        GLuint gBufferFBO;
        Texture2D gColorBuffer, gNormalBuffer, gPositionBuffer;
        RenderBuffer2D depthStencilBuffer;

        GLuint lightAccumulationFBO, postProcessFBO;
        Texture2D lightAccumulationBuffer;
        Texture2D postProcessColorBuffer;


        #pragma pack(push, 1)
        struct GlobalMatrices
        {
            glm::mat4 projectionMatrix; 
            glm::mat4 viewMatrix;       
            glm::mat4 inverseViewMatrix;
            glm::mat4 voxelMatrix;
            glm::mat4 inverseVoxelMatrix;
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
        



        StandardShader defaultVertFrag;
        StandardShader defaultVertNormalTexFrag;
        StandardShader defaultVertUnlitFrag;




        std::shared_ptr<Mesh> voxelMesh;
        unsigned int voxelPosBinding;
        
        GLuint voxelFBO;
        GLuint sparseListBuffer;
        GLuint voxel3DTex;
        GLuint voxel2DTex;
        GLuint numMipLevels;
        StandardShader voxelizationShader;
        ComputeShader mipmappingShader;
        StandardShader conetraceShader;
        StandardShader drawVoxelsShader;



        std::unique_ptr<Mesh> screenQuad;
        
        
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