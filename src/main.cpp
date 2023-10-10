#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "common/shader.h"

//By defining STB_IMAGE_IMPLEMENTATION the preprocessor modifies the header file such that it only contains the 
//relevant definition source code, effectively turning the header file into a .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "env.h"




void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

int main()
{

    // GLFW: initialize and configure
    // ------------------------------
    GLFWwindow* window;

    if (!glfwInit())
    {
        return -1;
    } 

    // makes sure the glfw is the right version and using core profile:
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif


    // GLFW: rendering window creation
    // -------------------------------
    window = glfwCreateWindow(640,360, PROJECT_NAME " " VERSION, NULL, NULL);
    glfwMakeContextCurrent(window);


    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "couldn't load openGL" << std::endl;

        glfwTerminate();
        return -1;
    }


    // GLFW: initial viewport configuration and callbacks
    // --------------------------------------------------


    // the viewport is the rendering window i.e. its the region where openGL will draw and it can be different from
    // GLFW's window size processed coordinates in OpenGL are between -1 and 1 so we effectively map from the range 
    // (-1 to 1) to (0, 640) and (0, 360). 
    glViewport(0, 0, 640, 360);


    // callback functions can be customized for certain events:

    // this callback is used after a resize happens to the GLFW window and we can pass a custom function that follows
    // a defined template:
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  

    // We register the callback functions after we've created the window and before the render loop is initiated. 


    //we call glClear and clear the color buffer, the entire color buffer will be filled with the color as configured 
    //by glClearColor:
    glClearColor(0.25f, 0.5f,0.75f,1.0f);




    // Reading and compiling the shader program to be used:
    // ----------------------------------------------------
    Shader compdShader(BASE_DIR"/data/shaders/tutorial/singleUniformColor.vert", BASE_DIR"/data/shaders/tutorial/singleUniformColor.frag");
    

    // Setup vertex data and buffers
    // -----------------------------

    // the data for a simple triangle (NON-INDEXED):
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f,  0.5f, 0.0f
    };  


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // a vertex array object can be created for easily managing the vertex attribute configuration (which stores the
    // information required to intepret the vertex buffers) and which buffer is associated with that config
    unsigned int VAO;
    glGenVertexArrays(1, &VAO); 
    // after binding, the vertex attributes configured from now on will be associated with this specific VAO
    glBindVertexArray(VAO); 
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    
    // a buffer object to store the triangle data:
    unsigned int VBO;
    glGenBuffers(1, &VBO);  //args = (number of buffers generated, buffer id/name references)
    // bind the buffer to an array buffer (the type used for the vertex shader data):
    glBindBuffer(GL_ARRAY_BUFFER, VBO);  

    // copy the previously defined "vertices" data into our array buffer in static draw mode. the draw modes are:
    // GL_STREAM_DRAW: the data is set only once and used by the GPU at most a few times.
    // GL_STATIC_DRAW: the data is set only once and used many times.
    // GL_DYNAMIC_DRAW: the data is changed a lot and used many times.
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);



    // after the buffer was set, we must inform how to interpret the vertex data stored in it through
    // vertex attribute pointers:

    // here, the vertex attribute 0 contained in the buffer is a vec3 (3D vector):
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);  
    // the vertex shader program can now take the vertex attribute 0 as an input after it is enabled:
    glEnableVertexAttribArray(0);  


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // since the VAO already contains the reference to VBO after the call to glVertexAttribPointer(), the
    // VBO can be safely unbound:
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    // the same can be done for the VAO, as long as it is bound again when we try to draw the object
    glBindVertexArray(0);

    // *unbinding is not necessary since the process of modifying a new VAO already calls glBindVertexArray()
    // and that would unbind the existing VAO
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////






    // the data for a cube (NON-INDEXED):
    float cVertices[] = {
        //position            //uv
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    // world space positions of the cubes
    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f),
        glm::vec3( 2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3( 2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f,  3.0f, -7.5f),
        glm::vec3( 1.3f, -2.0f, -2.5f),
        glm::vec3( 1.5f,  2.0f, -2.5f),
        glm::vec3( 1.5f,  0.2f, -1.5f),
        glm::vec3(-1.3f,  1.0f, -1.5f)
    };

    unsigned int cVAO;
    glGenVertexArrays(1, &cVAO); 
    glBindVertexArray(cVAO); 
    
    unsigned int cVBO;
    glGenBuffers(1, &cVBO);  
    
    glBindBuffer(GL_ARRAY_BUFFER, cVBO);  
    glBufferData(GL_ARRAY_BUFFER, sizeof(cVertices), cVertices, GL_STATIC_DRAW);

    //positions:
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);  
    glEnableVertexAttribArray(0);  

    //uvs:
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3* sizeof(float)));  
    glEnableVertexAttribArray(2);  





    // Indexed drawing setup
    // ---------------------
    
    // when drawing more complicated shapes, indexed drawing is used to avoid overlapping vertices:

    // the data for a simple rectangle:
    float iVertices[] = {
         //position           //normal            //texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,  // top right
         0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,  // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f // top left 
    };  

    unsigned int indices[] = {  
        0, 1, 3,   // first triangle
        1, 2, 3    // second triangle
    };  

    // we use a new vertex array object for the new object:
    unsigned int iVAO;
    glGenVertexArrays(1, &iVAO); 
    glBindVertexArray(iVAO); 

    unsigned int iVBO, EBO; // now an element buffer object has to be generated to store indices
    glGenBuffers(1, &iVBO);  
    glGenBuffers(1, &EBO);

    glBindBuffer(GL_ARRAY_BUFFER, iVBO);  
    glBufferData(GL_ARRAY_BUFFER, sizeof(iVertices), iVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // configuring the vertex positions:
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);  
    glEnableVertexAttribArray(0);  

    // configuring the vertex normals:
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3* sizeof(float)));
    glEnableVertexAttribArray(1);

    // configuring the vertex uvs/texcoords:
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);    


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //The last element buffer object (EBO) that gets bound while a VAO is bound, is stored as the VAO's EBO, therefore
    //we can unbind everything and only store the VAO:
    glBindVertexArray(0);

    // *HOWEVER, To unbind the element buffer object (EBO), we first must unbind the vertext array object (VAO), 
    //since the VAO stores the bind calls, which would lead to the VAO not storing the correct EBO and cause errors
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////



    // Matrix Projection: Model matrix (local space -> world space)
    // ---updated in the render loop---
    //glm::mat4 modelMatrix = glm::mat4(1.0f);
    //modelMatrix = glm::rotate(modelMatrix, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Matrix Projection: View matrix (world space -> view (camera) space)
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    // translating the scene in the reverse direction of where we want the camera to move
    viewMatrix = glm::translate(viewMatrix, glm::vec3(0.0f, 0.0f, -3.0f)); 

    // Matrix Projection: projection matrix (view space -> clip space)
    glm::mat4 projectionMatrix;
    projectionMatrix = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);



    // Loading Textures
    // ----------------
    int width, height, nrChannels; //these properties will be filled when loading the image

    // loading image data:
    unsigned char *data = stbi_load(BASE_DIR"/data/textures/container.jpg", &width, &height, &nrChannels, 0);

    // creating the texture object:
    unsigned int texture;
    glGenTextures(1, &texture); 

    // activate the texture unit first before binding its corresponding texture:
    glActiveTexture(GL_TEXTURE0); //there are a total of 16 texture units (slots)
    glBindTexture(GL_TEXTURE_2D, texture);  

    // setting the texture wrapping/filtering options:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // check for errors in the image loading
    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }

    // the texture unit also has to be associated with a uniform sampler in the shader program:

    // always activate the shader before setting uniforms
    compdShader.use(); 
    compdShader.setInt("main_texture", 0);


    
    stbi_image_free(data);
    

    
    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Enable z (depth) testing
    glEnable(GL_DEPTH_TEST);  


    // Render Loop
    // -----------
    while(!glfwWindowShouldClose(window))
    {
        // Input Handling
        // --------------
        processInput(window);
        

        // Rendering
        // ---------

        // clear the data on the depth buffer:
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // clear the data on the color buffer:
        glClear(GL_COLOR_BUFFER_BIT);


        // setting the shader uniform values in the CPU before passing to GPU:

        // shader has to be used before updating the uniforms
        compdShader.use();

        // Scene-based properties
        // -------------------------

        // adding transformation matrices:
        compdShader.setMat4("viewMatrix", glm::value_ptr(viewMatrix));
        compdShader.setMat4("projectionMatrix", glm::value_ptr(projectionMatrix));
        

        // Material-based properties
        // -------------------------

        // bind textures that will be used before drawing
        glBindTexture(GL_TEXTURE_2D, texture);

        // adding a color
        double timeValue = glfwGetTime();
        float greenValue = static_cast<float>(sin(4.0 * timeValue) / 2.0f + 0.5f);
        compdShader.setVec4("uColor",  0.0f, greenValue, 0.0f, 1.0f);



        // Non-indexed drawing
        // -------------------

        // drawing a single triangle:

        // set the vertex data and its configuration that will be used for drawing
        //glBindVertexArray(VAO);
        

        // the model matrix depends on the object:
        //glm::mat4 modelMatrix = glm::mat4(1.0f);
        //modelMatrix = glm::rotate(modelMatrix, glm::radians(60.0f * (float)timeValue), glm::vec3(1.0, 0.0, 0.0));
        //modelMatrix = glm::scale(modelMatrix, glm::vec3(0.5, 0.5, 0.5));
        //compdShader.setMat4("modelMatrix", glm::value_ptr(modelMatrix));

        // draw the data using the currently active shader
        //glDrawArrays(GL_TRIANGLES, 0, 3);



        // drawing spinning cubes:
        glBindVertexArray(cVAO);

        // drawing one object:
        // glDrawArrays(GL_TRIANGLES, 0, 36);

        // drawing multiple objects:
        for(unsigned int i = 0; i < 10; i++)
        {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, cubePositions[i]);
            float angle = 20.0f * i + 60.0f * timeValue; 
            modelMatrix = glm::rotate(modelMatrix, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            compdShader.setMat4("modelMatrix", glm::value_ptr(modelMatrix));

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }


        // Indexed drawing
        // ---------------
        
        // Drawing a rectangle (two triangles):

        // set the vertex data and its configuration that will be used for drawing
        //glBindVertexArray(iVAO);
        
        // the model matrix depends on the object:
        //glm::mat4 modelMatrix = glm::mat4(1.0f);
        //modelMatrix = glm::rotate(modelMatrix, glm::radians(60.0f * (float)timeValue), glm::vec3(1.0, 0.0, 0.0));
        //modelMatrix = glm::scale(modelMatrix, glm::vec3(0.5, 0.5, 0.5));
        //compdShader.setMat4("modelMatrix", glm::value_ptr(modelMatrix));

        
        // for indexed drawing, we must set the number of vetices that we want to draw (generally = n of indices)
        // as well as the format of the index buffer (EBO)
        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);









        // GLFW: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------

        // Double buffering: When an application draws in a single buffer the resulting image may have flickering.
        // scince the resulting output image is not drawn in an instant, but drawn pixel by pixel. To circumvent these
        // issues, windowing applications apply a double buffer for rendering. The front buffer contains the final 
        // output image that is shown at the screen, while all the rendering commands draw to the back buffer. As soon
        // as all the rendering commands are finished we swap the back buffer to the front buffer:
        glfwSwapBuffers(window);


        // PollEvents: checks if any events are triggered (like keyboard input or mouse movement events), updates the 
        // window state and calls the corresponding functions (which we can register via callback methods):
        glfwPollEvents();
    }


    // GLFW: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}


// A callback that is used after a resize happens to the GLFW window. This is passed
// to glfwSetFramebufferSizeCallback() and can be customized to use the arguments that are automatically
// filled in by GLFW
// -----------------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}  

// Process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    //if the user pressed escape (Esc), then the window closes
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}