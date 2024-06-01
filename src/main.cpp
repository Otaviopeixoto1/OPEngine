//always include glad and glfw in this order
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


#include "env.h"
#include "common/Shader.h"

#include "scene/Camera.h"
//#include "scene/Scene.h"
#include "scene/SceneParser.h"
#include "render/renderers.h"
#include "debug/OPProfiler.h"

//a custom library with simple objects for testing:
#include "test/GLtest.h"

//By defining STB_IMAGE_IMPLEMENTATION the preprocessor modifies the header file such that it only contains the 
//relevant definition source code, effectively turning the header file into a .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VSYNC_OFF


// Frame time variables:
float frametime = 0;
float deltaTime = 0.0f;	
float lastFrame = 0.0f; 

//starting window size
unsigned int windowWidth = 800;
unsigned int windowHeight = 800;

//mouse position
float lastMouseX, lastMouseY;
const float mouseSensitivity = 0.1f;
bool firstMouseMovement = true;
bool windowResized = false;

//initializing the camera
Camera mainCamera(glm::vec3(0.0f, 0.0f, 3.0f));

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void toggle_camera_rotation_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif


    // GLFW: rendering window creation
    // -------------------------------
    window = glfwCreateWindow(windowWidth, windowHeight, PROJECT_NAME " " VERSION, NULL, NULL);
    glfwMakeContextCurrent(window);

    #ifdef VSYNC_OFF
        glfwSwapInterval(0);
    #endif
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

    //this callback is used for enabling/disabling camera rotation
    glfwSetKeyCallback(window, toggle_camera_rotation_callback);

    // We register the callback functions after we've created the window and before the render loop is initiated. 



    // imgui: for rendering ui elements:
    // ---------------------------------
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);          // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();






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


       
    Scene scene = Scene();
    auto sceneParser = JsonHelpers::SceneParser();
    //sceneParser.Parse(scene, &mainCamera, "/data/scenes/2D/RadianceCascadeTest.json", OP_OBJ); 

    //sceneParser.Parse(scene, &mainCamera, "/data/scenes/sponza_scene.json", OP_OBJ);
    //sceneParser.Parse(scene, &mainCamera, "/data/scenes/deferred_test.json", OP_OBJ);
    //sceneParser.Parse(scene, &mainCamera, "/data/scenes/backpack_scene.json", OP_OBJ);

    //VCTGI
    //sceneParser.Parse(scene, &mainCamera, "/data/scenes/sponza_scene.json", OP_OBJ);
    sceneParser.Parse(scene, &mainCamera, "/data/scenes/Cornell_scene.json", OP_OBJ);


    auto profiler = OPProfiler::OPProfiler(); 

    ForwardRenderer forwardRenderer = ForwardRenderer(windowWidth, windowHeight);
    DeferredRenderer deferredRenderer = DeferredRenderer(windowWidth, windowHeight);
    VCTGIRenderer vctgiRenderer = VCTGIRenderer(windowWidth, windowHeight);
    Radiance2DRenderer radiance2DRenderer = Radiance2DRenderer(windowWidth, windowHeight);

    BaseRenderer* renderer = &vctgiRenderer;

    
    try
    {
        renderer->RecreateResources(scene, mainCamera, window);
        renderer->ReloadShaders();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    
    
    // hide the cursor when the window on focus:
    if (!mainCamera.isLocked())
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
        


    // Render Loop
    // -----------
    while(!glfwWindowShouldClose(window))
    {
        // calculating the frame time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame; 

        profiler.BeginFrame();



        // GLFW: poll IO events (keys pressed/released, mouse moved etc.)
        // --------------------------------------------------------------

        // PollEvents: checks if any events are triggered (like keyboard input or mouse movement events), updates the 
        // window state and calls the corresponding functions (which we can register via callback methods):
        glfwPollEvents();


        // Start Rendering UI
        // ------------------

        ImGui_ImplOpenGL3_NewFrame();// Start the Dear ImGui frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        


        // Input Handling
        // --------------
        processInput(window);

        if (windowResized)
        {
            renderer->ViewportUpdate(windowWidth, windowHeight);
            windowResized = false;
        }
        
        


        // Rendering the scene
        // -------------------

        renderer->RenderFrame(mainCamera, &scene, window, &profiler);


        profiler.EndFrame();



        

        // Render UI
        // ---------
        ImGui::Begin("Camera");
        glm::vec3 camPos = mainCamera.GetPosition();

        if (ImGui::InputFloat3("Position", (float*)&camPos))
        {
            mainCamera.SetPosition(camPos);
        }
        ImGui::End();

        ImGui::Begin("Scene");
        ImGui::Text("Scene");
        ImGui::End();
        

        renderer->RenderGUI();

        

        // Render Profiler Window
        // ----------------------
        std::stringstream title;
        title.precision(2);
        title << std::fixed << "profiler [" << deltaTime * 1000.0f << "ms  " << 1.0f/deltaTime << "fps]###ProfilerWindow";

        ImGui::Begin(title.str().c_str(), 0, ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("GPU profiler:");
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        int sizeMargin = int(ImGui::GetStyle().ItemSpacing.y);
        int maxGraphHeight = 300;
        int availableGraphHeight = (int(canvasSize.y) - sizeMargin);// / 2;
        int graphHeight = std::min(maxGraphHeight, availableGraphHeight);
        int legendWidth = 200;
        int graphWidth = int(canvasSize.x) - legendWidth;
        int frameOffset = 0;

        profiler.RenderWindow(graphWidth, legendWidth, graphHeight, frameOffset);

        ImGui::End();


        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());




        // GLFW: swap buffers 
        // ------------------
        // Double buffering: When an application draws in a single buffer the resulting image may have flickering.
        // scince the resulting output image is not drawn in an instant, but drawn pixel by pixel. To circumvent these
        // issues, windowing applications apply a double buffer for rendering. The front buffer contains the final 
        // output image that is shown at the screen, while all the rendering commands draw to the back buffer. As soon
        // as all the rendering commands are finished we swap the back buffer to the front buffer:
        
        glfwSwapBuffers(window);

    }
    
    CameraData cameraData = mainCamera.GetCameraData();
    sceneParser.SerializeToJson(cameraData);


    // imgui: clearing all resources alocated:
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();


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


void toggle_camera_rotation_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_KP_ENTER && action == GLFW_PRESS)
    {
        bool locked = mainCamera.ToggleRotation();
        if (!locked)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
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
    
}