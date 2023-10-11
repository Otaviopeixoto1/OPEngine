

#ifndef GL_TEST_H
#define GL_TEST_H


// Setup vertex data and buffers
// -----------------------------

/* Vertex Buffer structure
 * [position(float, float, float){REQUIRED}, normal(float, float, float){OPTIONAL}, ub(float, float){REQUIRED}]
 *
 * VertexAttribArray indexes:
 * 0 -> position
 * 1 -> normal
 * 2 -> uv
 */

enum Tests {
    NI_TRIG,
    NI_CUBE,
    I_RECT,
};



unsigned int GetTestVAO(Tests test);

  





#endif