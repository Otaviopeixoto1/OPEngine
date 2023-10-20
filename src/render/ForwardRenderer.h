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

            // always use the object's shader program before assigning uniforms:
            //shader.use();
            // the uniform bindings (glVertexAttribPointers) have to be passed per shader and used for dynamically
            // setting the uniform bindings here

            scene->IterateObjects([&](glm::mat4 objectToWorld, Material *material, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)
            {
                /* setup the shader resources and draw */

                // shader has to be used before updating the uniforms
                Shader activeShader;
                if (material->HasFlag(OP_MATERIAL_TEXTURED_DIFFUSE))
                {
                    activeShader = defaultVertTexFrag;
                }
                else
                {
                    activeShader = defaultVertFrag;
                }

                activeShader.use();


                // Standard uniforms
                // -----------------
                activeShader.setVec3("albedo", material->albedoColor);

                // Transformation matrices
                // -----------------------

                // Matrix Projection: View matrix (world space -> view (camera) space)
                activeShader.setMat4("viewMatrix", passData.viewMatrix);
                // Matrix Projection: projection matrix (view space -> clip space)
                activeShader.setMat4("projectionMatrix", passData.projMatrix);
                // Matrix Projection: model matrix (object space -> world space)
                activeShader.setMat4("modelMatrix", objectToWorld);


                // the uniforms: viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix. can be passed as a uniform
                //               buffer object (UBO)

                // the uniforms: objectToWorld, albedoColor and emissiveColor have to be set per object in the scene
                //               the uniform bindings (glVertexAttribPointers) for these can be set and stored in 
                //               each objects VAO but can also be set dynamically of on the same UBO with bindings
                

                //Bind all textures
                unsigned int diffuseNr = 1;
                unsigned int specularNr = 1;
                unsigned int normalNr = 1;
                for (unsigned int i = 0; i < material->texturePaths.size(); i++)
                {
                    Texture texture = scene->GetTexture(material->texturePaths[i]);
                    glActiveTexture(GL_TEXTURE0 + i); // activate proper texture unit before binding

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
                //draw indexed
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