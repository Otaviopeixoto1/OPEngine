
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h> 
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <boost/regex.hpp>
#include <exception>
#include "env.h"

//Todo:
/*
- add support for spir-v compilation and separate shader objects
 */

#define SHADER_INCLUDE_SUBDIR "/data/shaders/include/"



class Shader
{
public:
    // shader program object ID
    unsigned int ID;


    Shader(){}
    
    Shader(std::string vertexPath, std::string fragmentPath)
    {
        this->vertexShaderPath = vertexPath;
        this->fragmentShaderPath = fragmentPath;
    }

    void AddShaderStage(const std::string& shaderPath, GLenum shaderType)
    {
        switch (shaderType)
        {
            case GL_GEOMETRY_SHADER:
                additionalShaderStages[GL_GEOMETRY_SHADER] = shaderPath;
                break;
        }
    }

  
    void BuildProgram()
    {
        // compile shaders:
        unsigned int vertex, fragment;
        int success;
        char infoLog[512];
        
        std::vector<unsigned int> shaderPipeline;

        // 1) Vertex shader
        // ----------------
        vertex = CompileShader(vertexShaderPath, GL_VERTEX_SHADER);
        shaderPipeline.push_back(vertex);
        
        // 2) Fragment shader
        // ------------------
        fragment = CompileShader(fragmentShaderPath, GL_FRAGMENT_SHADER);
        shaderPipeline.push_back(fragment);
        // 3) Additional Shaders
        // ---------------------
        
        if (additionalShaderStages.find(GL_GEOMETRY_SHADER) != additionalShaderStages.end())
        {

            unsigned int geometry = CompileShader(additionalShaderStages[GL_GEOMETRY_SHADER], GL_GEOMETRY_SHADER);
            shaderPipeline.push_back(geometry);
        }


        // Linking the final program
        // -------------------------
        ID = glCreateProgram();

        for (size_t i = 0; i < shaderPipeline.size(); i++)
        {
            glAttachShader(ID, shaderPipeline[i]);
        }
        glLinkProgram(ID);

        // print linking errors if any
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(ID, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
            throw ShaderException("Shader linking failed\n");
            
        }/*
        else
        {
            std::cout << "Successfully Compiled and Linked Shaders: " << vertexShaderPath << " and " << fragmentShaderPath << std::endl;
        }*/
        
        // delete the shaders as they're linked into the program now and therefore are longer necessary
        for (size_t i = 0; i < shaderPipeline.size(); i++)
        {
            glDeleteShader(shaderPipeline[i]);
        }

        isReady = true;
    }

    void AddPreProcessorDefines(std::string defines[], int count)
    {
        for (size_t i = 0; i < count; i++)
        {
            preDefines.push_back(defines[i]);
        }
        
    }
    

    std::string PreProcessShader(std::string &source, const std::string &filePath, unsigned int level, bool versionMatch = false)
    {
        if(level > 32)
            throw ShaderException("the" + filePath + "header inclusion reached depth limit (32), might be caused by cyclic header inclusion");

        static const boost::regex re("^[ ]*#[ ]*include[ ]+[\"<](.*)[\">].*");
        static const std::string includeDir = BASE_DIR  SHADER_INCLUDE_SUBDIR;

        static const boost::regex ver("^[ ]*#[ ]*version[ ]+(.*).*");
        static bool hasPreDefines;

        std::stringstream input;
        std::stringstream output;
        input << source;

        size_t line_number = 1;
        boost::smatch reMatches;
        boost::smatch verMatches;
        
        std::string line;


        while(std::getline(input,line))
        {
            if(!versionMatch)
            {
                hasPreDefines = false;
                versionMatch = boost::regex_search(line, verMatches, ver);

                if(versionMatch)
                    output <<  line << std::endl;

                ++line_number;
                continue;
            }

            if (!hasPreDefines)
            {
                for (size_t i = 0; i < preDefines.size(); i++)
                {
                    output << "#define " <<  preDefines[i] << std::endl;
                }
                
                hasPreDefines = true;
            }
            
            
            if (boost::regex_search(line, reMatches, re))
            {
                std::string include_file = reMatches[1];
                std::string include_string = ReadShaderFile(includeDir + include_file);
                
                output << PreProcessShader(include_string, include_file, level + 1, versionMatch) << std::endl;
            }
            else
            {
                
                output <<  line << std::endl;
                output << "#line "<< line_number << " \"" << filePath << "\""  << std::endl;
            }
            ++line_number;
        }

        return output.str();
    }

    void BindUniformBlocks(std::string namedBindings[], std::size_t size, unsigned int bindingOffset = 0)
    {
        for (size_t i = 0; i < size; i++)
        {
            BindUniformBlock(namedBindings[i], i + bindingOffset);
        }
        
    }

    // bind the property block to a binding point using its name
    void BindUniformBlock(const std::string &block, unsigned int binding)
    {
        unsigned int blockId = glGetUniformBlockIndex(ID, block.c_str());
        glUniformBlockBinding(ID, blockId, binding);
    } 


    // use (activate) the shader program
    void UseProgram()
    { 
        if (!isReady)
        {
            throw ShaderException("Shader Object is incomplete");
        }
        glUseProgram(ID);
    }  

    
    // utility functions for setting uniforms
    void SetBool(const std::string &name, bool value) const
    {         
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); 
    }
    void SetInt(const std::string &name, int value) const
    { 
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value); 
    }
    void SetFloat(const std::string &name, float value) const
    { 
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
    } 

    void SetVec2(const std::string &name, float v1, float v2) const
    { 
        glUniform2f(glGetUniformLocation(ID, name.c_str()), v1, v2); 
    } 

    void SetVec2(const std::string &name, glm::vec2 v) const
    { 
        glUniform2f(glGetUniformLocation(ID, name.c_str()), v.x, v.y); 
    } 

    void SetVec3(const std::string &name, glm::vec3 v) const
    { 
        glUniform3f(glGetUniformLocation(ID, name.c_str()), v.x, v.y, v.z); 
    } 
    void SetVec3(const std::string &name, float v1, float v2, float v3) const
    { 
        glUniform3f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3); 
    } 

    void SetVec4(const std::string &name, glm::vec4 v) const
    { 
        glUniform4f(glGetUniformLocation(ID, name.c_str()), v.x, v.y, v.z, v.w); 
    } 
    void SetVec4(const std::string &name, float v1, float v2, float v3, float v4) const
    { 
        glUniform4f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3, v4); 
    } 

    void SetMat4(const std::string &name, glm::mat4 mat4, GLboolean transpose = GL_FALSE)
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, transpose, glm::value_ptr(mat4)); 
    }


    class ShaderException: public std::exception
    {
        std::string message;
        public:
            ShaderException(const std::string &message)
            {
                this->message = message;
            }
            virtual const char* what() const throw()
            {
                return message.c_str();
            }
    };
private:
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    bool isReady = false;
    std::vector<std::string> preDefines;

    std::unordered_map<GLenum, std::string> additionalShaderStages;

    
    unsigned int CompileShader(const std::string& srcPath, GLenum ShaderType)
    {
        std::string srcCode = ReadShaderFile(srcPath);
        std::string ppSrcCode = PreProcessShader(srcCode, srcPath, 0);

        const char* shaderCode = ppSrcCode.c_str();

        unsigned int shaderObject;
        int success;
        char infoLog[512];
        
        shaderObject = glCreateShader(ShaderType);
        glShaderSource(shaderObject, 1, &shaderCode, NULL);
        glCompileShader(shaderObject);

        // print compile errors if any
        glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(shaderObject, 512, NULL, infoLog);

            switch (ShaderType)
            {
                case GL_VERTEX_SHADER:
                    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
                    throw ShaderException(vertexShaderPath +  ": vertex Shader compilation failed failed\n");
                    return 0;
                case GL_GEOMETRY_SHADER:
                    std::cout << "ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n" << infoLog << std::endl;
                    throw ShaderException(vertexShaderPath +  ": geometry Shader compilation failed failed\n");
                    return 0;
                case GL_FRAGMENT_SHADER:
                    std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
                    throw ShaderException(vertexShaderPath +  ": fragment Shader compilation failed failed\n");
                    return 0;
                
                default:
                    std::cout << "ERROR::SHADER::UNKNOWN::COMPILATION_FAILED\n" << infoLog << std::endl;
                    throw ShaderException(vertexShaderPath +  ": unknown Shader failed to compile\n");
                    return 0;
            }
            
        };
        return shaderObject;
    }

    std::string ReadShaderFile(std::string path)
    {
        std::string sourceCode = "";
        std::ifstream file;
        // ensure ifstream objects can throw exceptions:
        file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        try 
        {
            // open files
            file.open(path);
            std::stringstream shaderStream;
            // read file's buffer contents into streams
            shaderStream << file.rdbuf();

            // close file handlers
            file.close();
            // convert stream into string
            sourceCode = shaderStream.str();	
        }
        catch(std::ifstream::failure e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
            throw ShaderException("Shader construction failed. Cannot read file: " + path);
        }

        return sourceCode;
    }


};
  
#endif