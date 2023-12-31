#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "env.h"
#include "common/Shader.h"

#include "scene/Camera.h"
#include "scene/Scene.h"
#include "render/renderers.h"


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
unsigned int windowWidth = 800;
unsigned int windowHeight = 600;

//mouse position
float lastMouseX, lastMouseY;
const float mouseSensitivity = 0.1f;
bool firstMouseMovement = true;
bool windowResized = false;

//initializing the camera
Camera mainCamera(glm::vec3(0.0f, 0.0f, 3.0f));
bool freeCamMode = true;

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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif


    // GLFW: rendering window creation
    // -------------------------------
    window = glfwCreateWindow(windowWidth, windowHeight, PROJECT_NAME " " VERSION, NULL, NULL);
    glfwMakeContextCurrent(window);


    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "ERROR: couldn't load openGL" << std::endl;

        glfwTerminate();
        return -1;
    }


    // GLFW: initial viewport configuration and callbacks
    // --------------------------------------------------

    // the viewport is the rendering window i.e. its the region where openGL will draw and it can be different from
    // GLFW's window size processed coordinates in OpenGL are between -1 and 1 so we effectively map from the range 
    // (-1 to 1) to (0, 640) and (0, 360). 
    glViewport(0, 0, windowWidth, windowHeight);

    // setting the initial mouse position to the center of the screen
    lastMouseX = windowWidth/2;
    lastMouseY = windowHeight/2;

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
    //glClearColor(0.25f, 0.5f,0.75f,1.0f);






    // flip all stbi loaded images vertically for compatibility with openGL
    // CURRENTLY THIS HAS TO BE DEACTIVATED ON SPONZA SCENE:
    //stbi_set_flip_vertically_on_load(true); 
    
    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Enable z (depth) testing:
    glEnable(GL_DEPTH_TEST);  

    // Enable backface culling: 
    glEnable(GL_CULL_FACE);  
    glCullFace(GL_BACK); 
    glFrontFace(GL_CCW);

    // hide the cursor and only show again when the window is out of focus or minimized:
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    

    Scene scene = Scene("/data/scenes/sponza_scene.json", OP_OBJ);

    ForwardRenderer forwardRenderer = ForwardRenderer(windowWidth, windowHeight);
    DeferredRenderer deferredRenderer = DeferredRenderer(windowWidth, windowHeight);
    BaseRenderer* renderer = &forwardRenderer;
    
    try
    {
        renderer->RecreateResources(scene);
        renderer->ReloadShaders();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    
    


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

        if (windowResized)
        {
            renderer->ViewportUpdate(windowWidth, windowHeight);
            windowResized = false;
        }

        // Rendering
        // ---------

        renderer->RenderFrame(mainCamera, &scene, window);



        // GLFW: swap buffers 
        // ------------------

        // Double buffering: When an application draws in a single buffer the resulting image may have flickering.
        // scince the resulting output image is not drawn in an instant, but drawn pixel by pixel. To circumvent these
        // issues, windowing applications apply a double buffer for rendering. The front buffer contains the final 
        // output image that is shown at the screen, while all the rendering commands draw to the back buffer. As soon
        // as all the rendering commands are finished we swap the back buffer to the front buffer:
        glfwSwapBuffers(window);


        // GLFW: poll IO events (keys pressed/released, mouse moved etc.)
        // --------------------------------------------------------------

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
    windowResized = true;
    windowWidth = width;
    windowHeight = height;
    mainCamera.SetProjectionAspect(width/(float)height);
    glViewport(0, 0, width, height);
}  


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!freeCamMode)
    {
        return;
    }
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

void ToggleCameraMovementMode(GLFWwindow *window)
{
    if (freeCamMode)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        freeCamMode = false;
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        freeCamMode = true;
    }
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

    if (glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS)
        ToggleCameraMovementMode(window);
    
}