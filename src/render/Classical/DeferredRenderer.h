#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H

#include "../BaseRenderer.h"
#include "../render_features/ShadowRenderer.h"
#include "../render_features/SkyRenderer.h"
#include "../../debug/OPProfiler.h"
#include "../../common/Colors.h"

class DeferredRenderer : public BaseRenderer
{
    public:
        const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
        static constexpr unsigned int SHADOW_CASCADE_COUNT = 2; // MAX == 4

        static constexpr bool enableShadowMapping = true;
        static constexpr bool enableNormalMaps = true;
        static constexpr bool enableLightVolumes = true;
        
        static constexpr int MAX_DIR_LIGHTS = 5;
        static constexpr int MAX_POINT_LIGHTS = 40;
        
        float tonemapExposure = 1.0f;
        float FXAAContrastThreshold = 0.0312f;
        float FXAABrightnessThreshold = 0.063f;

        enum GBufferPassInputBindings
        {
            DIFFUSE_TEXTURE0_BINDING = 1,
            NORMAL_TEXTURE0_BINDING = 2,
            SPECULAR_TEXTURE0_BINDING = 4,
        };

        enum LightingPassBufferBindings
        {
            COLOR_SPEC_BUFFER_BINDING = 0,
            NORMAL_BUFFER_BINDING = 1,
            POSITION_BUFFER_BINDING = 2,
            SHADOW_MAP_BUFFER0_BINDING = 3,
        };

        
        DeferredRenderer(unsigned int vpWidth, unsigned int vpHeight)
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

            // Shadow maps:
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

            MeshData PointVolData = MeshData::LoadMeshDataFromFile(BASE_DIR "/data/models/light_volumes/pointLightVolume_ico.obj");
            pointLightVolume = std::make_shared<Mesh>(PointVolData);

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
            gColorBuffer.BindToTarget(gBufferFBO, GL_COLOR_ATTACHMENT0 + COLOR_SPEC_BUFFER_BINDING);
            
            // 2) View space normal buffer attachment 
            gNormalBuffer = Texture2D(gBufferTexDescriptor);
            gNormalBuffer.BindToTarget(gBufferFBO, GL_COLOR_ATTACHMENT0 + NORMAL_BUFFER_BINDING);

            // 3) View space position buffer attachment 
            gPositionBuffer = Texture2D(gBufferTexDescriptor);
            gPositionBuffer.BindToTarget(gBufferFBO, GL_COLOR_ATTACHMENT0 + POSITION_BUFFER_BINDING);

            //setting the depth and stencil attachments
            depthStencilBuffer = RenderBuffer2D(GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);
            depthStencilBuffer.BindToTarget(gBufferFBO, GL_DEPTH_STENCIL_ATTACHMENT);


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
                auto voxelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.025,0.025,0.025)); //calculate based on the scene size or the frustrum bounds !!
                auto invVoxelMatrix = glm::inverse(voxelMatrix);

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
            auto shadowTask = profiler->AddTask("Shadow Pass", Colors::amethyst);
            shadowTask->Start();

            ShadowsOutput shadowOut = {0, GL_TEXTURE_2D};
            if (enableShadowMapping)
            {
                shadowOut = shadowRenderer.Render(frameResources);
            }
            shadowTask->End();


            // 2) gBuffer Pass:
            // ----------------
            auto gbufferTask = profiler->AddTask("gBuffer Pass", Colors::emerald);
            gbufferTask->Start();

            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            int shaderCache = -1;

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


                auto materialPropertiesBuffer = shaderMemoryPool.GetUniformBuffer("MaterialProperties");
                materialPropertiesBuffer->SetData(0, sizeof(MaterialProperties), &(materialInstance->properties));

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
            
            // 3) Lighting Accumulation pass: use g-buffer to calculate the scene's lighting
            // -----------------------------------------------------------------------------
            auto lightAccTask = profiler->AddTask("Light Accumulation", Colors::orange);
            lightAccTask->Start();
            glBindFramebuffer(GL_FRAMEBUFFER, lightAccumulationFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            glDepthMask(GL_FALSE);

            // Binding the gBuffer textures:
            //these binding dont have to be the same as the gbuffer bindings but it will be better follow a convention
            gColorBuffer.BindForRead(COLOR_SPEC_BUFFER_BINDING);
            gNormalBuffer.BindForRead(NORMAL_BUFFER_BINDING);
            gPositionBuffer.BindForRead(POSITION_BUFFER_BINDING);

            glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_BUFFER0_BINDING); //Use shadowRenderer.GetOutput to bind it here
            glBindTexture(shadowOut.texType0, shadowOut.shadowMap0);
            
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
            screenQuad->BindBuffers();
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

                auto localMatricesBuffer = shaderMemoryPool.GetUniformBuffer("LocalMatrices"); 
                localMatricesBuffer->SetData(0, sizeof(glm::mat4), (void*)glm::value_ptr(objectToWorld));
                localMatricesBuffer->SetData(sizeof(glm::mat4), sizeof(glm::mat4), (void*)glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)) );


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
            screenQuad->BindBuffers();
            lightAccumulationBuffer.BindForRead(0);
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

            directionalLightingPass = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/deferred/fsDeferredLighting.frag");
            directionalLightingPass.AddPreProcessorDefines(preprocessorDefines);
            if (enableLightVolumes)
            {
                std::string s = "LIGHT_VOLUMES";
                directionalLightingPass.AddPreProcessorDefines(&s,1);
            }
            directionalLightingPass.BuildProgram();
            directionalLightingPass.UseProgram();
            directionalLightingPass.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            directionalLightingPass.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            directionalLightingPass.SetInt("gPosition", POSITION_BUFFER_BINDING);
            directionalLightingPass.SetInt("shadowMap0", SHADOW_MAP_BUFFER0_BINDING);
            directionalLightingPass.BindUniformBlocks(bufferBindings);

            simpleDepthPass = StandardShader(BASE_DIR"/data/shaders/simpleVert.vert", BASE_DIR"/data/shaders/nullFrag.frag");
            simpleDepthPass.BuildProgram();

            pointLightVolShader = StandardShader(BASE_DIR"/data/shaders/simpleVert.vert", BASE_DIR"/data/shaders/deferred/pointVolumeLighting.frag");
            pointLightVolShader.AddPreProcessorDefines(preprocessorDefines);
            pointLightVolShader.BuildProgram();
            pointLightVolShader.UseProgram();
            pointLightVolShader.SetInt("gAlbedoSpec", COLOR_SPEC_BUFFER_BINDING); 
            pointLightVolShader.SetInt("gNormal", NORMAL_BUFFER_BINDING);
            pointLightVolShader.SetInt("gPosition", POSITION_BUFFER_BINDING);
            pointLightVolShader.BindUniformBlocks(bufferBindings);

            postProcessShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadTonemapLum.frag");
            postProcessShader.BuildProgram();
            postProcessShader.UseProgram();

            FXAAShader = StandardShader(BASE_DIR"/data/shaders/screenQuad/quad.vert", BASE_DIR"/data/shaders/screenQuad/quadFXAA.frag");
            FXAAShader.BuildProgram();
        }

        void RenderGUI()
        {
            ImGui::Begin("Deferred Renderer");
            ImGui::SeparatorText("Postprocessing");
            ImGui::SliderFloat("Tonemap Exposure", &tonemapExposure, 0.0f, 10.0f, "exposure = %.3f");

            
            ImGui::End();
        }

    private:
        std::vector<std::string> preprocessorDefines;

        PCFShadowRenderer shadowRenderer;
        SkyRenderer skyRenderer;

        unsigned int viewportWidth;
        unsigned int viewportHeight;

        GLuint gBufferFBO;
        Texture2D gColorBuffer, gNormalBuffer, gPositionBuffer;
        RenderBuffer2D depthStencilBuffer;

        GLuint lightAccumulationFBO, postProcessFBO;
        Texture2D lightAccumulationBuffer;
        Texture2D postProcessColorBuffer;

        StandardShader defaultVertFrag;
        StandardShader defaultVertNormalTexFrag;
        StandardShader defaultVertUnlitFrag;

        StandardShader directionalLightingPass;
        
        StandardShader simpleDepthPass; 
        StandardShader pointLightVolShader;
        std::shared_ptr<Mesh> pointLightVolume;
        
        StandardShader postProcessShader;
        StandardShader FXAAShader;
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