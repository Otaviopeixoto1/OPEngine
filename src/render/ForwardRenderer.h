#ifndef FORWARD_RENDERER_H
#define FORWARD_RENDERER_H

#include "BaseRenderer.h"

class ForwardRenderer : public BaseRenderer
{
    public:
        const std::string MATRIX_UNIFORM_BLOCK = "Matrices";
        const std::string LIGHTS_UNIFORM_BLOCK = "Lights";
        unsigned int LightBufferSize = 0;

        ForwardRenderer()
        {
        
        }

        void RecreateSceneResources(Scene *scene)
        {
            
        }

        //virtual void RenderFrame(const legit::InFlightQueue::FrameInfo &frameInfo, const Camera &camera, const Camera &light, Scene *scene, GLFWwindow *window){}
        void RenderFrame(const Camera &camera, Scene *scene, GLFWwindow *window)
        {

            // the PassData can be passed as a uniform buffer that is created for the entire frame.
            /*
            struct PassData
            {
                //legit::ShaderMemoryPool *memoryPool;
                glm::mat4 viewMatrix;
                glm::mat4 projMatrix;
                Scene *scene;
            }passData;
            passData.viewMatrix = camera.GetViewMatrix();
            passData.projMatrix = camera.projectionMatrix;*/

            

            GlobalLightData lights = scene->GetLightData();

            //std::cout << sizeof(lights) << " ";

            // Global Uniforms (dont depend on objects/materials)
            // --------------------------------------------------
            glm::mat4 viewMatrix = camera.GetViewMatrix();
            // the MatricesUBO is linked to the binding at 0 and we can set its data for all shaders like so:
            glBindBuffer(GL_UNIFORM_BUFFER, MatricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(camera.projectionMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMatrix));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 

            // the LightsUBO is linked to the binding at 1:
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, LightBufferSize, &lights);
            //void *ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
            // now copy data into memory
            //memcpy(ptr, &lights, sizeof(lights));
            // make sure to tell OpenGL we're done with the pointer
            //glUnmapBuffer(GL_UNIFORM_BUFFER);
            //glBufferSubData(GL_UNIFORM_BUFFER, 4*sizeof(int), sizeof(glm::vec4), glm::value_ptr(lights.directionalLights[0].lightColor));
            glBindBuffer(GL_UNIFORM_BUFFER, 0);   

        

            unsigned int shaderCache = 0;

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;
                if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertTexFrag;
                }
                else if (materialInstance->HasFlag(OP_MATERIAL_IS_LIGHT))
                {
                    activeShader = defaultVertUnlitFrag;
                }
                else
                {
                    activeShader = defaultVertFrag;
                }

                // shader has to be used before updating the uniforms
                if (activeShader.ID != shaderCache)
                {
                    activeShader.use();
                }

                // Standard uniforms (also use UBOs ?)
                // -----------------------------------

                // the uniforms: objectToWorld, albedoColor and emissiveColor have to be set per object in the scene
                //               the uniform bindings (glVertexAttribPointers) for these can be set and stored in 
                //               each objects VAO but can also be set dynamically of on the same UBO with bindings

                // albedo color of the object
                activeShader.setVec3("albedo", materialInstance->albedoColor);

                // Matrix Projection: model matrix (object space -> world space)
                //activeShader.setMat4("modelMatrix", objectToWorld);
                glBindBuffer(GL_UNIFORM_BUFFER, MatricesUBO);
                glBufferSubData(GL_UNIFORM_BUFFER, 2*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(objectToWorld));
                glBufferSubData(GL_UNIFORM_BUFFER, 3*sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(MathUtils::ComputeNormalMatrix(viewMatrix,objectToWorld)));
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

                    activeShader.setInt((name + number).c_str(), i);
                    glBindTexture(GL_TEXTURE_2D, texture.id);
                }
                
                //bind VAO
                mesh->BindBuffers();

                //indexed draw
                glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
            });  


        }
        void ReloadShaders()
        {
            defaultVertFrag = Shader(BASE_DIR"/data/shaders/standardVert.vert", BASE_DIR"/data/shaders/albedoFrag.frag");
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/standardVert.vert", BASE_DIR"/data/shaders/texturedFrag.frag");
            defaultVertUnlitFrag = Shader(BASE_DIR"/data/shaders/standardVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            
            //Add this binding process to the shader class

            // ***Adopt a naming convention for all global uniform buffers***

            // Get the uniform blocks defined in the shader code
            unsigned int matrixBlockDVDF   = glGetUniformBlockIndex(defaultVertFrag.ID, MATRIX_UNIFORM_BLOCK.c_str());
            unsigned int matrixBlockDVTF  = glGetUniformBlockIndex(defaultVertTexFrag.ID, MATRIX_UNIFORM_BLOCK.c_str());
            unsigned int matrixBlockDVUF  = glGetUniformBlockIndex(defaultVertUnlitFrag.ID, MATRIX_UNIFORM_BLOCK.c_str());

            unsigned int lightBlockDVDF = glGetUniformBlockIndex(defaultVertFrag.ID, LIGHTS_UNIFORM_BLOCK.c_str());
            unsigned int lightBlockDVTF = glGetUniformBlockIndex(defaultVertTexFrag.ID, LIGHTS_UNIFORM_BLOCK.c_str());

            // Set the binding point of the Matrices uniform block in all shaders to 0
            glUniformBlockBinding(defaultVertFrag.ID,    matrixBlockDVDF, 0);
            glUniformBlockBinding(defaultVertTexFrag.ID,  matrixBlockDVTF, 0);
            glUniformBlockBinding(defaultVertUnlitFrag.ID,  matrixBlockDVUF, 0);
            // Set the binding point of the Lights uniform block in all shaders to 1
            glUniformBlockBinding(defaultVertFrag.ID,    lightBlockDVDF, 1);
            glUniformBlockBinding(defaultVertTexFrag.ID,  lightBlockDVTF, 1);

            glGenBuffers(1, &MatricesUBO);
            glGenBuffers(1, &LightsUBO);

            // Matrix buffer setup
            // -------------------
            /* Matrices Uniform buffer structure:
             * {
             *    mat4 projectionMatrix;
             *    mat4 viewMatrix;
             *    mat4 modelMatrix;
             *    mat4 normalMatrix;
             * }
             */
            
            // Create the buffer and specify its size:

            glBindBuffer(GL_UNIFORM_BUFFER, MatricesUBO);
            glBufferData(GL_UNIFORM_BUFFER, 4 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            // Bind a certain range of the buffer to the uniform block: this allows to use multiple UBOs 
            // per uniform block
            glBindBufferRange(GL_UNIFORM_BUFFER, 0, MatricesUBO, 0, 4 * sizeof(glm::mat4));


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
            glBufferData(GL_UNIFORM_BUFFER, LightBufferSize, NULL, GL_STATIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            
            // Bind a certain range of the buffer to the uniform block: this allows to use multiple UBOs 
            // per uniform block
            glBindBufferRange(GL_UNIFORM_BUFFER, 1, LightsUBO, 0, LightBufferSize);

        }

    private:

        //MipBuilder mipBuilder;
        //BlurBuilder blurBuilder;
        //DebugRenderer debugRenderer;
        unsigned int MatricesUBO;
        unsigned int LightsUBO;

        Shader defaultVertFrag;
        Shader defaultVertTexFrag;
        Shader defaultVertUnlitFrag;


};

#endif