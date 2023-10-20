#ifndef FORWARD_RENDERER_H
#define FORWARD_RENDERER_H

#include "BaseRenderer.h"

class ForwardRenderer : public BaseRenderer
{
    public:

        ForwardRenderer()
        {
            defaultVertFrag = Shader(BASE_DIR"/data/shaders/standardVert.vert", BASE_DIR"/data/shaders/albedoFrag.frag");
            defaultVertTexFrag = Shader(BASE_DIR"/data/shaders/standardVert.vert", BASE_DIR"/data/shaders/texturedFrag.frag");
        }

        void RecreateSceneResources(Scene *scene)
        {
            
        }

        //virtual void RenderFrame(const legit::InFlightQueue::FrameInfo &frameInfo, const Camera &camera, const Camera &light, Scene *scene, GLFWwindow *window){}
        void RenderFrame(const Camera &camera, Scene *scene, GLFWwindow *window)
        {

            // the PassData can be passed as a uniform buffer that is created for the entire frame.
            struct PassData
            {
                //legit::ShaderMemoryPool *memoryPool;
                glm::mat4 viewMatrix;
                glm::mat4 projMatrix;
                glm::mat4 lightViewMatrix;
                glm::mat4 lightProjMatrix;
                Scene *scene;
            }passData;
            passData.viewMatrix = camera.GetViewMatrix();
            passData.projMatrix = camera.projectionMatrix;


            BaseLight::LightData mainLightData = scene->GetLightData(0);
            // Global Uniforms (dont depend on objects/materials)
            // --------------------------------------------------

            defaultVertFrag.use();

            //these can all be set in a UBO: and be shared with all shaders

            // ***Adopt a naming convention for all global uniform buffers***

            // Matrix Projection: View matrix (world space -> view (camera) space)
            defaultVertFrag.setMat4("viewMatrix", passData.viewMatrix);
            // Matrix Projection: projection matrix (view space -> clip space)
            defaultVertFrag.setMat4("projectionMatrix", passData.projMatrix);
            // Light properties
            defaultVertFrag.setVec3("lightColor", mainLightData.lightColor);

            defaultVertTexFrag.use();
            // Matrix Projection: View matrix (world space -> view (camera) space)
            defaultVertTexFrag.setMat4("viewMatrix", passData.viewMatrix);
            // Matrix Projection: projection matrix (view space -> clip space)
            defaultVertTexFrag.setMat4("projectionMatrix", passData.projMatrix);
            // Light properties
            defaultVertTexFrag.setVec3("lightColor", mainLightData.lightColor);

            unsigned int shaderCache = 0;

            scene->IterateObjects([&](glm::mat4 objectToWorld, Material *material, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {    
                Shader activeShader;
                if (material->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
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
                    activeShader.use();
                }

                // Standard uniforms (also use UBOs ?)
                // -----------------

                // the uniforms: objectToWorld, albedoColor and emissiveColor have to be set per object in the scene
                //               the uniform bindings (glVertexAttribPointers) for these can be set and stored in 
                //               each objects VAO but can also be set dynamically of on the same UBO with bindings

                // albedo color of the object
                activeShader.setVec3("albedo", material->albedoColor);

                // Matrix Projection: model matrix (object space -> world space)
                activeShader.setMat4("modelMatrix", objectToWorld);


                //Bind all textures
                unsigned int diffuseNr = 1;
                unsigned int specularNr = 1;
                unsigned int normalNr = 1;
                for (unsigned int i = 0; i < material->texturePaths.size(); i++)
                {
                    Texture texture = scene->GetTexture(material->texturePaths[i]);
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

        }
        void ChangeView()
        {

        }

    private:

        //MipBuilder mipBuilder;
        //BlurBuilder blurBuilder;
        //DebugRenderer debugRenderer;
        
        // Define the shader structs containing shader programs
        Shader defaultVertFrag;
        Shader defaultVertTexFrag;
};

#endif