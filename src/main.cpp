#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Temporary hardcoded shader programs 
// -------------------------
const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n\0";
///////////////////////////////////////////////////////////////



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
    window = glfwCreateWindow(640,360,"testWindow",NULL,NULL);
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












    // Build and compile the shader programs
    // -------------------------------------

    // 1) Vertex shader
    // ----------------
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    // add the source code (hardcoded string or read from file)
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // checking for compile-time errors:
    int  success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // 2) Fragment shader
    // ------------------
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // checking for compile-time errors:
    //int  success;
    //char infoLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }


    // Linking the final program
    // -------------------------
    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();

    // attach all previous shaders to the same program
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);

    // checking for linking errors:
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // all shaders have to be deleted once linked to their main program:
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);





    // Setup vertex data and buffers
    // -----------------------------

    // the data for a simple triangle:
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



    // after the buffer was set, we must inform how to interpret the vertex data stored in it:

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






    // Render Loop
    // -----------
    while(!glfwWindowShouldClose(window))
    {
        // Input Handling
        // --------------
        processInput(window);
        

        // Rendering
        // ---------
        glClear(GL_COLOR_BUFFER_BIT);

        // use the shader program to render:
        glUseProgram(shaderProgram);
        // set the vertex data and its configuration that will be used for drawing
        glBindVertexArray(VAO);
        // draw the data using the currently active shader
        glDrawArrays(GL_TRIANGLES, 0, 3);


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