#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "env.h"
#include "common/shader.h"
#include "scene/camera.h"

//a custom library with simple objects for testing:
#include "test/GLtest.h"

//By defining STB_IMAGE_IMPLEMENTATION the preprocessor modifies the header file such that it only contains the 
//relevant definition source code, effectively turning the header file into a .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


// Frame time:
float deltaTime = 0.0f;	
float lastFrame = 0.0f; 

//starting window size
const unsigned int BASE_WINDOW_WIDTH = 800;
const unsigned int BASE_WINDOW_HEIGHT = 600;

//mouse position
float lastMouseX, lastMouseY;
const float mouseSensitivity = 0.1f;
bool firstMouseMovement = true;

//initializing the camera
Camera mainCamera(glm::vec3(0.0f, 0.0f, 3.0f));

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
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
    window = glfwCreateWindow(BASE_WINDOW_WIDTH, BASE_WINDOW_HEIGHT, PROJECT_NAME " " VERSION, NULL, NULL);
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
    glViewport(0, 0, BASE_WINDOW_WIDTH, BASE_WINDOW_HEIGHT);

    // setting the initial mouse position to the center of the screen
    lastMouseX = BASE_WINDOW_WIDTH/2;
    lastMouseY = BASE_WINDOW_HEIGHT/2;

    // callback functions can be customized for certain events:

    // this callback is used after a resize happens to the GLFW window and we can pass a custom function that follows
    // a defined template:
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); 

    //this callback is used every time the mouse moves:
    glfwSetCursorPosCallback(window, mouse_callback); 

    //this callback is used every time the mouse wheel is used:
    glfwSetScrollCallback(window, scroll_callback); 



    // We register the callback functions after we've created the window and before the render loop is initiated. 




    //we call glClear and clear the color buffer, the entire color buffer will be filled with the color as configured 
    //by glClearColor:
    glClearColor(0.25f, 0.5f,0.75f,1.0f);




    // Reading and compiling the shader program to be used:
    // ----------------------------------------------------
    Shader compdShader(BASE_DIR"/data/shaders/tutorial/singleUniformColor.vert", BASE_DIR"/data/shaders/tutorial/singleUniformColor.frag");
    


    // world space positions for testing 
    glm::vec3 TEST_POSITIONS[] = {
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

    // Enable z (depth) testing:
    glEnable(GL_DEPTH_TEST);  

    // hide the cursor and only show again when the window is out of focus or minimized:
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    

    // Render Loop
    // -----------
    while(!glfwWindowShouldClose(window))
    {
        // calculating the frame time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame; 


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

        // Matrix Projection: View matrix (world space -> view (camera) space)
        compdShader.setMat4("viewMatrix", mainCamera.GetViewMatrix());
        // Matrix Projection: projection matrix (view space -> clip space)
        compdShader.setMat4("projectionMatrix", projectionMatrix);
        

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

        glBindVertexArray(GetTestVAO(NI_CUBE));

        // one cube:
        // glDrawArrays(GL_TRIANGLES, 0, 36);

        // multiple cubes:
        for(unsigned int i = 0; i < 10; i++)
        {
            // Matrix Projection: Model matrix (local space -> world space)
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, TEST_POSITIONS[i]);
            float angle = 20.0f * i + 60.0f * timeValue; 
            modelMatrix = glm::rotate(modelMatrix, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            compdShader.setMat4("modelMatrix", modelMatrix);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }





        // Indexed drawing
        // ---------------
        
        // Drawing a rectangle (two triangles):

        // set the vertex data and its configuration that will be used for drawing
        //glBindVertexArray(GetTestVAO(I_RECT));
        
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


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouseMovement) // initially set to true
    {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouseMovement = false;
    }

    float xoffset = xpos - lastMouseX;
    float yoffset = lastMouseY - ypos; // reversed since y-coordinates range from bottom to top
    lastMouseX = xpos;
    lastMouseY = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;


    mainCamera.ProcessMouseMovement(xoffset, yoffset);

}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

}

// Process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    //if the user pressed escape (Esc), then the window closes
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Camera Movement
    // ---------------
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        mainCamera.ProcessKeyboard(FORWARD_MOVEMENT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        mainCamera.ProcessKeyboard(BACKWARD_MOVEMENT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        mainCamera.ProcessKeyboard(LEFT_MOVEMENT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        mainCamera.ProcessKeyboard(RIGHT_MOVEMENT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        mainCamera.ProcessKeyboard(GLOBAL_UP_MOVEMENT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        mainCamera.ProcessKeyboard(GLOBAL_DOWN_MOVEMENT, deltaTime);
    
}