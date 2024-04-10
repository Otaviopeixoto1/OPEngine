#ifndef UNIFORM_BUFFER_H
#define UNIFORM_BUFFER_H


// change to GLBuffer
class GLUniformBuffer
{
    public:
        GLUniformBuffer(GLuint size, const std::string& name)
        {
            glGenBuffers(1, &GLId);
            glBindBuffer(GL_UNIFORM_BUFFER, GLId);
            glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STREAM_DRAW);
            this->size = size;
            this->name = name;
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        GLUniformBuffer(GLuint size, void *data, const std::string& name)
        {
            glGenBuffers(1, &GLId);
            glBindBuffer(GL_UNIFORM_BUFFER, GLId);
            glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
            this->size = size;
            this->name = name;
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void SetData(GLuint offset, GLuint size, void *data)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, GLId);
            glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        GLUniformBuffer(GLUniformBuffer &&other)
        {
            this->GLId = other.GLId;
            this->size = other.size;
            this->name = other.name;
            other.GLId = 0;
        }   

        GLUniformBuffer &operator = (GLUniformBuffer &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->size = other.size;
            this->name = other.name;
            other.GLId = 0;
            return *this;
        }

        ~GLUniformBuffer()
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }
        }

        void BindBufferFull(GLuint binding)
        {
            glBindBufferRange(GL_UNIFORM_BUFFER, binding, GLId, 0, size);
        }
        

        template<typename BufferData>
        BufferData *BeginSetData()
        {
            assert(size == sizeof(BufferData));
            glBindBuffer(GL_UNIFORM_BUFFER, GLId);
            void *bufferData = glMapBufferRange(GL_UNIFORM_BUFFER, 0, size, GL_MAP_WRITE_BIT);
            
            return (BufferData*)(bufferData);
        }

        void Clear()
        {
            glInvalidateBufferData(GLId);
        }
        void EndSetData()
        {
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }
    
    private:
        GLuint GLId;
        GLuint size;
        std::string name;
        // this is to prevent the destructor from deleting the buffer unintentionally by calling the destructor on a copy of the object
        GLUniformBuffer(const GLUniformBuffer&) = delete; // no copy constructor
        GLUniformBuffer &operator = (const GLUniformBuffer &other) = delete; // no copy assignement 
};





#endif