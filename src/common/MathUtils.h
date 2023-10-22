#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MathUtils
{
    static inline glm::mat4 ComputeNormalMatrix(glm::mat4 view, glm::mat4 model)
    {
        return glm::transpose(glm::inverse(view * model));
    }
}
#endif