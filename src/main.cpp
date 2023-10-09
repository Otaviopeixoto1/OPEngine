#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>

#include "common/shader.h"




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




    // Reading and compiling the shader program to be used:
    // ----------------------------------------------------
    Shader compdShader("/home/otavio/openGL/OPEngine/data/shaders/tutorial/singleUniformColor.vert", "/home/otavio/openGL/OPEngine/data/shaders/tutorial/singleUniformColor.frag");
    

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


    // Indexed drawing
    // ---------------
    
    // when drawing more complicated shapes, indexed drawing is used to avoid overlapping vertices:

    // the data for a simple rectangle:
    float iVertices[] = {
        0.5f,  0.5f, 0.0f,  // top right
        0.5f, -0.5f, 0.0f,  // bottom right
        -0.5f, -0.5f, 0.0f,  // bottom left
        -0.5f,  0.5f, 0.0f   // top left 
    };  

    unsigned int indices[] = {  // note that we start from 0!
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

    // when binding the EBO, 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);  
    glEnableVertexAttribArray(0);  

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //The last element buffer object (EBO) that gets bound while a VAO is bound, is stored as the VAO's EBO, therefore
    //we can unbind everything and only store the VAO:
    glBindVertexArray(0);

    // *HOWEVER, To unbind the element buffer object (EBO), we first must unbind the vertext array object (VAO), 
    //since the VAO stores the bind calls, which would lead to the VAO not storing the correct EBO and cause errors
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////








    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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



        // setting the shader uniform values in the CPU befor passing to GPU:
        double timeValue = glfwGetTime();
        float greenValue = static_cast<float>(sin(4.0 * timeValue) / 2.0f + 0.5f);
        compdShader.use();
        compdShader.setVec4("uColor",  0.0f, greenValue, 0.0f, 1.0f);
        //glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f);


        // Drawing a single triangle:
        // --------------------------

        // set the vertex data and its configuration that will be used for drawing
        //glBindVertexArray(VAO);
        // draw the data using the currently active shader
        //glDrawArrays(GL_TRIANGLES, 0, 3);

        // Drawing a rectangle (two triangles):
        // ------------------------------------
                
        // set the vertex data and its configuration that will be used for drawing
        glBindVertexArray(iVAO);
        // for indexed drawing, we must set the number of vetices that we want to draw (generally = n of indices)
        // as well as the format of the index buffer (EBO)
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);



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