#ifndef FORWARD_RENDERER_H
#define FORWARD_RENDERER_H

#include "BaseRenderer.h"

enum DRBufferBindings{
    GLOBAL_MATRICES_BINDING = 0,
    LOCAL_MATRICES_BINDING = 1,
    GLOBAL_LIGHTS_BINDING = 2,
    MATERIAL_PROPERTIES_BINDING = 3
};

class DeferredRenderer : public BaseRenderer
{
    public:
        
        // ***Adopted naming conventions for the global uniform blocks***

        std::string NamedBufferBindings[4] = {
            "GlobalMatrices",
            "LocalMatrices",
            "Lights",
            "MaterialProperties"
        };

        DeferredRenderer()
        {
        
        }

        void RecreateResources(Scene &scene)
        {
            
        }

        void ViewportUpdate(int vpWidth, int vpHeight)
        {

        }

        //Original:
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



            // Global Uniforms (dont depend on objects/materials)
            // --------------------------------------------------

            glm::mat4 viewMatrix = camera.GetViewMatrix();
            glm::mat4 projectionMatrix = camera.GetProjectionMatrix();

            // Setting GlobalMatricesUBO:
            glBindBuffer(GL_UNIFORM_BUFFER, GlobalMatricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionMatrix));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewMatrix));
            glBindBuffer(GL_UNIFORM_BUFFER, 0); 

            // Get Light data in view space:
            GlobalLightData lights = scene->GetLightData(viewMatrix);
            
            // Setting LightsUBO:
            glBindBuffer(GL_UNIFORM_BUFFER, LightsUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, LightBufferSize, &lights);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);   

        

            unsigned int shaderCache = 0;

            scene->IterateObjects([&](glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;
                if (materialInstance->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
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

                // shader has to be used before updating the uniforms
                if (activeShader.ID != shaderCache)
                {
                    activeShader.UseProgram();
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


        }
        void ReloadShaders()
        {
            
            defaultVertFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/albedoFrag.frag");
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/texturedFrag.frag");
            defaultVertUnlitFrag = Shader(BASE_DIR"/data/shaders/defaultVert.vert", BASE_DIR"/data/shaders/UnlitAlbedoFrag.frag");
            
            
            defaultVertFrag.BindUniformBlocks(NamedBufferBindings,4);
            defaultVertTexFrag.BindUniformBlocks(NamedBufferBindings,4);
            defaultVertUnlitFrag.BindUniformBlocks(NamedBufferBindings,4);



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


};

#endif