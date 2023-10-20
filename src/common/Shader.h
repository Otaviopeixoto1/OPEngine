
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h> 
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

//Todo:
/*
- add support for spir-v compilation and separate shader objects
- REDEFINE SHADER CONSTRUCTOR AND SEPARATE THE COMPILE AND LINK INTO FUNCTIONS
 */



class Shader
{
public:
    // shader program object ID
    unsigned int ID;

    //default constructor (DONT USE)
    Shader(){}
  
    Shader(const char* vertexPath, const char* fragmentPath)
    {
        // retrieve the vertex/fragment source code from filePath
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        try 
        {
            // open files
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();		
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode   = vShaderStream.str();
            fragmentCode = fShaderStream.str();		
        }
        catch(std::ifstream::failure e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        // compile shaders:
        unsigned int vertex, fragment;
        int success;
        char infoLog[512];
        
        // 1) Vertex shader
        // ----------------
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);

        // print compile errors if any
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(vertex, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        };
        
        // 2) Fragment shader
        // ------------------
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);

        // print compile errors if any
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(fragment, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        };



        
        // Linking the final program
        // -------------------------
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        // print linking errors if any
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(ID, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }
        else
        {
            std::cout << "successfully compiled shader" << std::endl;
        }
        
        // delete the shaders as they're linked into the program now and therefore are longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);

    }


    // use (activate) the shader
    void use()
    { 
        glUseProgram(ID);
    }  

    
    // utility functions for setting uniforms
    void setBool(const std::string &name, bool value) const
    {         
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); 
    }
    void setInt(const std::string &name, int value) const
    { 
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value); 
    }
    void setFloat(const std::string &name, float value) const
    { 
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
    } 

    void setVec3(const std::string &name, glm::vec3 v) const
    { 
        glUniform3f(glGetUniformLocation(ID, name.c_str()), v.x, v.y, v.z); 
    } 
    void setVec3(const std::string &name, float v1, float v2, float v3) const
    { 
        glUniform3f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3); 
    } 

    void setVec4(const std::string &name, glm::vec4 v) const
    { 
        glUniform4f(glGetUniformLocation(ID, name.c_str()), v.x, v.y, v.z, v.w); 
    } 
    void setVec4(const std::string &name, float v1, float v2, float v3, float v4) const
    { 
        glUniform4f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3, v4); 
    } 

    void setMat4(const std::string &name, glm::mat4 mat4, GLboolean transpose = GL_FALSE)
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, transpose, glm::value_ptr(mat4)); 
    }
};
  
#endif