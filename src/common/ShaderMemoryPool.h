#ifndef SHADER_MEMORY_POOL_H
#define SHADER_MEMORY_POOL_H

#include "../common/Pool.h"
#include "../gl/GLBuffer.h"



//template this as well to use different kinds of buffers

class ShaderMemoryPool
{
    public:
        using UniformBufferPool = Pool<GLUniformBuffer>;
        using UniformBufferBinding = UniformBufferPool::Id;
        //ADD SHADER STORAGE BUFFERS SUPPORT
        
        ShaderMemoryPool(){}

        void AddUniformBuffer(GLuint size, const std::string &name)
        {
            GLUniformBuffer buffer(size, name);
            namedUniformBindings[name] = uniformBuffers.Add(std::move(buffer));
            GetUniformBuffer(name)->BindBufferFull(namedUniformBindings[name].asInt);
        }

        GLUniformBuffer *GetUniformBuffer(const std::string &name)
        {
            return &uniformBuffers.Get(namedUniformBindings[name]);
        }

        GLuint GetUniformBufferBinding(const std::string &name)
        {
            return namedUniformBindings[name].asInt;
        }

        std::unordered_map<std::string, UniformBufferBinding> GetNamedBindings()
        {
            return namedUniformBindings;
        }

        void DeleteBuffer(UniformBufferBinding bufferBinding)
        {
            uniformBuffers.Release(bufferBinding);
        }

        void Clear()
        {

        }


    private:
        UniformBufferPool uniformBuffers;
        std::unordered_map<std::string, UniformBufferBinding> namedUniformBindings;
};


#endif