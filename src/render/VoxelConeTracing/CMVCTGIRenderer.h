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
        static constexpr unsigned int SHADOW_CASCADE_COUNT = 1; // MAX == 4

        static constexpr bool enableNormalMaps = true;
        static constexpr bool enableDebugMode = true;
        
        static constexpr int MAX_DIR_LIGHTS = 1;
        static constexpr int MAX_POINT_LIGHTS = 1;

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
        unsigned int voxelRes = 256;

        bool voxelize = true;
        bool drawVoxels = false;
        int mipLevel = 0;

        float aoDistance = 0.03f;
        float maxConeDistance = 1.0f;
        float accumThr = 1.1f;
        int maxLOD = 10;
        float stepMultiplier = 1.0f;
        float giIntensity = 1.0f;


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
            if (enableDebugMode)
            {
                preprocessorDefines.push_back("DEBUG_MODE");
            }
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
            gBufferTexDescriptor.minFilter = GL_NEAREST;
            gBufferTexDescriptor.magFilter = GL_NEAREST;

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
            
            // Texture for atomic operations
            TextureDescriptor packedTexDescriptor = TextureDescriptor();
            packedTexDescriptor.GLType = GL_TEXTURE_3D;
            packedTexDescriptor.numMips = 1;
            packedTexDescriptor.sizedInternalFormat = GL_R32UI;
            packedTexDescriptor.internalFormat = GL_RED_INTEGER;
            packedTexDescriptor.pixelFormat = GL_UNSIGNED_INT;
            packedTexDescriptor.width = voxelRes;
            packedTexDescriptor.height = voxelRes;
            packedTexDescriptor.depth = voxelRes;
            packedTexDescriptor.minFilter = GL_NEAREST_MIPMAP_NEAREST;
            packedTexDescriptor.magFilter = GL_NEAREST;
            packedTexDescriptor.wrapR = GL_CLAMP_TO_BORDER;
            packedTexDescriptor.wrapS = GL_CLAMP_TO_BORDER;
            packedTexDescriptor.wrapT = GL_CLAMP_TO_BORDER;

            packedVoxel3DTex = ITexture3D(packedTexDescriptor);
            float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            packedVoxel3DTex.SetBorderColor(borderColor);
            

            // Final texture used for conetracing:
            TextureDescriptor voxel3DTexDescriptor = TextureDescriptor();
            voxel3DTexDescriptor.GLType = GL_TEXTURE_3D;
            voxel3DTexDescriptor.numMips = numMipLevels + 1;
            voxel3DTexDescriptor.sizedInternalFormat = GL_RGBA8;
            voxel3DTexDescriptor.internalFormat = GL_RGBA;
            voxel3DTexDescriptor.pixelFormat = GL_FLOAT;
            voxel3DTexDescriptor.width = voxelRes;
            voxel3DTexDescriptor.height = voxelRes;
            voxel3DTexDescriptor.depth = voxelRes;
            voxel3DTexDescriptor.minFilter = GL_LINEAR_MIPMAP_LINEAR;
            voxel3DTexDescriptor.magFilter = GL_LINEAR;
            voxel3DTexDescriptor.wrapR = GL_CLAMP_TO_BORDER;
            voxel3DTexDescriptor.wrapS = GL_CLAMP_TO_BORDER;
            voxel3DTexDescriptor.wrapT = GL_CLAMP_TO_BORDER;

            voxelColorTex = ITexture3D(voxel3DTexDescriptor);
            voxelColorTex.SetBorderColor(borderColor);


            // Texture For debugging (REMOVE)
            TextureDescriptor voxel2DArrayTexDescriptor = TextureDescriptor();
            voxel2DArrayTexDescriptor.GLType = GL_TEXTURE_2D_ARRAY;
            voxel2DArrayTexDescriptor.numMips = 1;
            voxel2DArrayTexDescriptor.sizedInternalFormat = GL_R32UI;
            voxel2DArrayTexDescriptor.internalFormat = GL_RED_INTEGER;
            voxel2DArrayTexDescriptor.pixelFormat = GL_UNSIGNED_INT;
            voxel2DArrayTexDescriptor.width = voxelRes;
            voxel2DArrayTexDescriptor.height = voxelRes;
            voxel2DArrayTexDescriptor.depth = 3;
            voxel2DArrayTexDescriptor.minFilter = GL_NEAREST;
            voxel2DArrayTexDescriptor.magFilter = GL_NEAREST;
            voxel2DArrayTexDescriptor.wrapR = GL_CLAMP_TO_EDGE;
            voxel2DArrayTexDescriptor.wrapS = GL_CLAMP_TO_EDGE;
            voxel2DArrayTexDescriptor.wrapT = GL_CLAMP_TO_EDGE;

            packedVoxel2DTex = ITexture3D(voxel2DArrayTexDescriptor);


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
            lightBufferDescriptor.minFilter = GL_NEAREST;
            lightBufferDescriptor.magFilter = GL_NEAREST;

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
            postProcessBufferDescriptor.minFilter = GL_NEAREST;
            postProcessBufferDescriptor.magFilter = GL_NEAREST;

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
            if (voxelize)
            {
                auto voxelizationTask = profiler->AddTask("voxelization", Colors::belizeHole);
                voxelizationTask->Start();
                glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
                // Setup framebuffer for rendering offscreen
                GLint origViewportSize[4];
                glGetIntegerv(GL_VIEWPORT, origViewportSize);

                // These are first filled in the voxelization (storage buffer) and then read in the mipmap compute shader (indirect buffer)
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndBuffer);     
                glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compIndBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INDIRECT_BINDING, drawIndBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COMPUTE_INDIRECT_BINDING, compIndBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SPARSE_LIST_BINDING, sparseListBuffer);

                // Enable rendering to framebuffer with voxelRes resolution
                glBindFramebuffer(GL_FRAMEBUFFER, voxelFBO);
                glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, voxelRes);
                glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, voxelRes);
                glViewport(0, 0, voxelRes, voxelRes);

                // Clear the last voxelization data
                packedVoxel2DTex.ClearLevel(0, NULL);
                packedVoxel3DTex.ClearLevel(0, NULL);
                //There is no point clearing this though, the voxel color tex wont be used for atomics
                for(size_t i = 0; i <= numMipLevels; i++) 
                    voxelColorTex.ClearLevel(i, NULL);

                // Reset the sparse voxel count for drawing voxels
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawIndBuffer);
                for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) 
                    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, i * sizeof(DrawElementsIndirectCommand) + sizeof(GLuint), sizeof(GLuint), GL_RED, GL_UNSIGNED_INT, NULL); // Clear data before since data is used when drawing
                
                // Reset the sparse voxel count for compute shader
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, compIndBuffer);
                for(size_t i = 0; i <= MAX_MIP_MAP_LEVELS; i++) 
                    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, i * sizeof(ComputeIndirectCommand), sizeof(GLuint), GL_RED, GL_UNSIGNED_INT, NULL); // Clear data before since data is used when drawing
                
                packedVoxel2DTex.BindImageRW(VX_VOXEL2DTEX_BINDING, 0);
                packedVoxel3DTex.BindImageRW(VX_VOXEL3DTEX_BINDING, 0, 0);

                glActiveTexture(GL_TEXTURE0 + VX_SHADOW_MAP0_BINDING);
                glBindTexture(shadowOut.texType0, shadowOut.shadowMap0);

                
                
                
                // 1st part of voxelization:
                // -------------------------
                glDisable(GL_CULL_FACE);// all faces must be rendered
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
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
                });  
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                voxelizationTask->End();


                auto copyColorTask = profiler->AddTask("Copy Voxel Color", Colors::peterRiver);
                copyColorTask->Start();
                glViewport(origViewportSize[0], origViewportSize[1], origViewportSize[2], origViewportSize[3]);
                glEnable(GL_CULL_FACE);
                glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);

                // Copy result into the actual RGBA color buffer For first mip, then do mipmapping:
                resolveVoxelsShader.UseProgram();
                packedVoxel3DTex.BindImageR(3, 0, 0);
                voxelColorTex.BindImageW(4, 0, 0);

                glDispatchComputeIndirect(NULL);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
                copyColorTask->End();
            }


            // 2nd part of voxelization: Mipmapping
            auto mipmappingTask = profiler->AddTask("mipmapping", Colors::pomegranate);
            mipmappingTask->Start();
            
            mipmappingShader.UseProgram();
            mipmappingShader.SetUInt("baseLevelResolution", voxelRes);
            for(unsigned int level = 0; level < numMipLevels; level++) 
            {
                voxelColorTex.BindImageR(3, level, 0);
                voxelColorTex.BindImageW(4, level + 1, 0);

                mipmappingShader.SetUInt("currentLevel", level + 1);
                
                unsigned int mipSize = voxelRes >> (level + 1);
                unsigned int groupSize = (mipSize + 4 - 1)/4; // this ceils the result of: mipSize / 4
                glDispatchCompute(groupSize, groupSize, groupSize);
                
                // Wait for buffer updates before reading and writing to next mip:
                // | GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }
            //barriers for other gl commands involving the buffers and textures 
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

            mipmappingTask->End();

            
            if (drawVoxels)
            {
                auto drawVoxelsTask = profiler->AddTask("drawVoxels", Colors::wisteria);
                drawVoxelsTask->Start();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);    
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);

                //packedVoxel3DTex.BindForRead(GI_VOXEL3DTEX_BINDING);
                voxelColorTex.BindForRead(GI_VOXEL3DTEX_BINDING);

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

            //glActiveTexture(GL_TEXTURE0 + GI_VOXEL2DTEX_BINDING);
            //glBindTexture(GL_TEXTURE_2D_ARRAY, voxel2DTex);
            glActiveTexture(GL_TEXTURE0 + GI_SHADOW_MAP0_BINDING);
            glBindTexture(shadowOut.texType0, shadowOut.shadowMap0);
            voxelColorTex.BindForRead(GI_VOXEL3DTEX_BINDING);
            gColorBuffer.BindForRead(GI_COLOR_SPEC_BINDING);
            gNormalBuffer.BindForRead(GI_NORMAL_BINDING);
            gPositionBuffer.BindForRead(GI_POSITION_BINDING);


            conetraceShader.UseProgram(); // MAke UBO FOR THIS
            conetraceShader.SetUInt("voxelRes", voxelRes);
            conetraceShader.SetFloat("aoDistance", aoDistance);
            conetraceShader.SetFloat("maxConeDistance", maxConeDistance);
            conetraceShader.SetFloat("accumThr", accumThr);
            conetraceShader.SetFloat("maxLOD", maxLOD);
            conetraceShader.SetFloat("stepMultiplier", stepMultiplier);
            conetraceShader.SetFloat("giIntensity", giIntensity);

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
            lightAccumulationBuffer.BindForRead(0);
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
            postProcessColorBuffer.BindForRead(0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            FXAATask->End(); 
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


            resolveVoxelsShader = ComputeShader(BASE_DIR"/data/shaders/voxelization/resolveVoxels.comp");
            resolveVoxelsShader.AddPreProcessorDefines(preprocessorDefines);
            resolveVoxelsShader.BuildProgram();
            resolveVoxelsShader.BindUniformBlocks(bufferBindings);

            mipmappingShader = ComputeShader(BASE_DIR"/data/shaders/voxelization/mipmapSimple.comp");
            mipmappingShader.AddPreProcessorDefines(preprocessorDefines);
            mipmappingShader.BuildProgram();
            mipmappingShader.BindUniformBlocks(bufferBindings);


            conetraceShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/voxelization/conetraceGI.frag");
            conetraceShader.AddPreProcessorDefines(preprocessorDefines);
            conetraceShader.BuildProgram();
            conetraceShader.UseProgram();
            conetraceShader.SetSamplerBinding("voxelColorTex", GI_VOXEL3DTEX_BINDING);
            conetraceShader.SetSamplerBinding("shadowMap0", GI_SHADOW_MAP0_BINDING);
            conetraceShader.SetSamplerBinding("gColorSpec", GI_COLOR_SPEC_BINDING); 
            conetraceShader.SetSamplerBinding("gNormal", GI_NORMAL_BINDING);
            conetraceShader.SetSamplerBinding("gPosition", GI_POSITION_BINDING);
            conetraceShader.BindUniformBlocks(bufferBindings);


            drawVoxelsShader = StandardShader(BASE_DIR"/data/shaders/voxelization/drawVoxels.vert", BASE_DIR"/data/shaders/voxelization/drawVoxels.frag");
            drawVoxelsShader.AddPreProcessorDefines(preprocessorDefines);
            drawVoxelsShader.BuildProgram();
            drawVoxelsShader.UseProgram();
            //drawVoxelsShader.SetSamplerBinding("voxel3DData", GI_VOXEL3DTEX_BINDING);
            drawVoxelsShader.SetSamplerBinding("voxelColorTex", GI_VOXEL3DTEX_BINDING);//voxelColorTex
            drawVoxelsShader.BindUniformBlocks(bufferBindings);
        }

        void RenderGUI()
        {
            ImGui::Begin("VCTGI");
            ImGui::SeparatorText("Postprocessing");
            ImGui::SliderFloat("Tonemap Exposure", &tonemapExposure, 0.0f, 10.0f, "exposure = %.3f");

            ImGui::SeparatorText("Voxelization");
            ImGui::Checkbox("revoxelize", &voxelize);
            ImGui::Checkbox("Draw Voxels", &drawVoxels);
            ImGui::SliderInt("Mipmap Level", &mipLevel, 0, MAX_MIP_MAP_LEVELS);

            ImGui::SeparatorText("Cone Tracing");
            ImGui::SliderFloat("AO Distance", &aoDistance, 0.01f, 0.5f, "AO = %.003f");
            ImGui::SliderFloat("Max Cone Distance", &maxConeDistance, 0.0f, 10.0f, "Cone Distance = %.3f");
            ImGui::SliderFloat("Accumulation Threshold", &accumThr, 0.0f, 5.0f, "Threshold = %.3f");
            ImGui::SliderFloat("Cone Step Multiplier", &stepMultiplier, 0.1f, 10.0f, "Step Multiplier = %.3f");
            ImGui::SliderFloat("giIntensity", &giIntensity, 0.0f, 10.0f, "Intensity = %.3f");
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
        ITexture3D voxelColorTex;
        ITexture3D packedVoxel3DTex;
        ITexture3D packedVoxel2DTex;
        GLuint numMipLevels;

        StandardShader voxelizationShader;
        ComputeShader resolveVoxelsShader;
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