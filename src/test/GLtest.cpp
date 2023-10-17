
#include "GLtest.h"

#include <cmath>
#include <glad/glad.h>


// Non-indexed drawing setup:
// --------------------------

// the data for a simple triangle (NON-INDEXED):
const float TEST_NI_TRIG_VERTS[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    0.0f,  0.5f, 0.0f
};  


// the data for drawing a cube (NON-INDEXED):
const float TEST_NI_CUBE_VERTS[] = {
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




// Indexed drawing setup
// ---------------------

// when drawing more complicated shapes, indexed drawing is used to avoid overlapping vertices:

// the data for a simple rectangle:
const float TEST_I_RECT_VERTS[] = {
        //position           //normal            //texture coords
        0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,  // top right
        0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,  // bottom right
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left
    -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f // top left 
};  

const unsigned int TEST_RECT_INDICES[] = {  
    0, 1, 3,   // first triangle
    1, 2, 3    // second triangle
};  






unsigned int GetTestVAO(Tests test)
{
    switch (test)
    {
    case NI_TRIG:

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // a vertex array object can be created for easily managing the vertex attribute configuration (which stores 
        // the information required to intepret the vertex buffers) and which buffer is associated with that config
        unsigned int TEST_NI_TRIG_VAO;
        glGenVertexArrays(1, &TEST_NI_TRIG_VAO); 
        // after binding, the vertex attributes configured from now on will be associated with this specific VAO
        glBindVertexArray(TEST_NI_TRIG_VAO); 
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////


        // a buffer object to store the triangle data:
        unsigned int TEST_NI_TRIG_VBO;
        glGenBuffers(1, &TEST_NI_TRIG_VBO);  //args = (number of buffers generated, buffer id/name references)
        // bind the buffer to an array buffer (the type used for the vertex shader data):
        glBindBuffer(GL_ARRAY_BUFFER, TEST_NI_TRIG_VBO);  

        // copy the previously defined "vertices" data into our array buffer in static draw mode. the draw modes are:
        // GL_STREAM_DRAW: the data is set only once and used by the GPU at most a few times.
        // GL_STATIC_DRAW: the data is set only once and used many times.
        // GL_DYNAMIC_DRAW: the data is changed a lot and used many times.
        glBufferData(GL_ARRAY_BUFFER, sizeof(TEST_NI_TRIG_VERTS), TEST_NI_TRIG_VERTS, GL_STATIC_DRAW);


        // after the buffer was set, we must inform how to interpret the vertex data stored in it through
        // vertex attribute pointers:

        // here, the vertex attribute 0 contained in the buffer is a vec3 (3D vector):
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);  
        // the vertex shader program can now take the vertex attribute 0 as an input after it is enabled:
        glEnableVertexAttribArray(0);  


        //////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // since the VAO already contains the reference to VBO after the call to glVertexAttribPointer(), the
        // VBO can be safely unbound:
        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        // the same can be done for the VAO, as long as it is bound again when we try to draw the object
        glBindVertexArray(0);

        // *unbinding is not necessary since the process of modifying a new VAO already calls glBindVertexArray()
        // and that would unbind the existing VAO
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////


        return TEST_NI_TRIG_VAO;

    case NI_CUBE:

        unsigned int TEST_NI_CUBE_VAO;
        glGenVertexArrays(1, &TEST_NI_CUBE_VAO); 
        glBindVertexArray(TEST_NI_CUBE_VAO); 

        unsigned int TEST_NI_CUBE_VBO;
        glGenBuffers(1, &TEST_NI_CUBE_VBO);  

        glBindBuffer(GL_ARRAY_BUFFER, TEST_NI_CUBE_VBO);  
        glBufferData(GL_ARRAY_BUFFER, sizeof(TEST_NI_CUBE_VERTS), TEST_NI_CUBE_VERTS, GL_STATIC_DRAW);

        //positions:
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);  
        glEnableVertexAttribArray(0);  

        //uvs:
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3* sizeof(float)));  
        glEnableVertexAttribArray(2);  

        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0);

        return TEST_NI_CUBE_VAO;

    case I_RECT:

        unsigned int TEST_I_RECT_VAO;
        glGenVertexArrays(1, &TEST_I_RECT_VAO); 
        glBindVertexArray(TEST_I_RECT_VAO); 

        unsigned int TEST_I_RECT_VBO, TEST_I_RECT_EBO; // now an element buffer object has to be generated to store indices
        glGenBuffers(1, &TEST_I_RECT_VBO);  
        glGenBuffers(1, &TEST_I_RECT_EBO);

        glBindBuffer(GL_ARRAY_BUFFER, TEST_I_RECT_VBO);  
        glBufferData(GL_ARRAY_BUFFER, sizeof(TEST_I_RECT_VERTS), TEST_I_RECT_VERTS, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TEST_I_RECT_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(TEST_RECT_INDICES), TEST_RECT_INDICES, GL_STATIC_DRAW);

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
        // The last element buffer object (EBO) that gets bound while a VAO is bound, is stored as the VAO's EBO, 
        // therefore we can unbind everything and only store the VAO:

        // *HOWEVER, To unbind the element buffer object (EBO), we first must unbind the vertext array object (VAO), 
        //since the VAO stores the bind calls, which would lead to the VAO not storing the correct EBO and cause errors
        glBindVertexArray(0);
        // unbinding the VBO:
        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        return TEST_I_RECT_VAO;

    }
}
