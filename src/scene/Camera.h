#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>



// Defines several possible options for camera movement to avoid window-system specific input methods
enum CameraMovement {
    FORWARD_MOVEMENT,
    BACKWARD_MOVEMENT,
    LEFT_MOVEMENT,
    RIGHT_MOVEMENT,
    GLOBAL_UP_MOVEMENT,
    GLOBAL_DOWN_MOVEMENT
};


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices 
// for use in OpenGL
class Camera
{
public:
    float Near;
    float Far;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f, float near = 0.1f, float far = 100.0f) 
        : Aspect(800.0f/600.0f), Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(5.0f), MouseSensitivity(0.1f), Zoom(60.0f)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        Near = near;
        Far = far;
        updateCameraVectors();
    }
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch, float near = 0.1f, float far = 100.0f) 
        : Aspect(800.0f/600.0f), Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(5.0f), MouseSensitivity(0.1f), Zoom(60.0f)
    {
        Position = glm::vec3(posX, posY, posZ);
        WorldUp = glm::vec3(upX, upY, upZ);
        Yaw = yaw;
        Pitch = pitch;
        Near = near;
        Far = far;
        updateCameraVectors();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix() const
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    glm::mat4 GetProjectionMatrix() const
    {
        return glm::perspective(glm::radians(Zoom), Aspect, Near, Far);
    }

    void SetProjectionAspect(float aspect)
    {
        this->Aspect = aspect;
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(CameraMovement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;
        switch (direction)
        {
        case FORWARD_MOVEMENT:
            Position += Front * velocity;
            break;
        case BACKWARD_MOVEMENT:
            Position -= Front * velocity;
            break;
        case LEFT_MOVEMENT:
            Position -= Right * velocity;
            break;
        case RIGHT_MOVEMENT:
            Position += Right * velocity;
            break;
        case GLOBAL_UP_MOVEMENT:
            Position += WorldUp* velocity;
            break;
        case GLOBAL_DOWN_MOVEMENT:
            Position -= WorldUp* velocity;
            break;
        }


    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
    {
        Yaw   += xoffset;
        Pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        
        updateCameraVectors();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= (float)yoffset;
        if (Zoom < 1.0f)
            Zoom = 1.0f;
        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

    inline std::vector<glm::vec4> GetFrustumCornersWorldSpace(float nearPlane, float farPlane, const glm::mat4& view) const
    {
        glm::mat4 proj = glm::perspective(glm::radians(Zoom), Aspect, nearPlane, farPlane);

        const auto inv = glm::inverse(proj * view);
        
        std::vector<glm::vec4> frustumCorners;
        for (unsigned int x = 0; x < 2; ++x)
        {
            for (unsigned int y = 0; y < 2; ++y)
            {
                for (unsigned int z = 0; z < 2; ++z)
                {
                    const glm::vec4 pt = 
                        inv * glm::vec4(
                            2.0f * x - 1.0f,
                            2.0f * y - 1.0f,
                            2.0f * z - 1.0f,
                            1.0f);
                    frustumCorners.push_back(pt / pt.w);
                }
            }
        }
        
        return frustumCorners;
    }

    glm::vec3 GetPosition()
    {
        return Position;
    }

private:
    // camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // euler Angles
    float Yaw;
    float Pitch;

    // camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;
    float Aspect;
    

    // update Front, Right and Up Vectors using the updated Euler angles
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));

        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, WorldUp));  
        // normalize the vectors, because their length gets closer to 0 the more you look up or down which results 
        // in slower movement.
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};





#endif