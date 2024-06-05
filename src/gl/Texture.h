#ifndef GL_TEXTURE_H
#define GL_TEXTURE_H

#include <iostream>
#include <glad/glad.h>
#include <string>
#include <stb_image.h>




struct TextureDescriptor
{
    GLenum GLType;
    unsigned int width = 1;
    unsigned int height = 1;
    unsigned int depth = 1;
    unsigned int numMips = 1;
    GLenum pixelFormat; 
    GLenum internalFormat;
    GLint sizedInternalFormat;
    GLint minFilter = GL_LINEAR;
    GLint magFilter = GL_LINEAR;
    GLint wrapS = GL_CLAMP_TO_EDGE;
    GLint wrapT = GL_CLAMP_TO_EDGE;
    GLint wrapR = GL_CLAMP_TO_EDGE;
};

//CHECK IF THE DESCRIPTORS ARE VALID FOR EACH TYPE OF TEXTURE:
class TextureObject
{   
    private: 
        TextureObject(const TextureObject&) = delete; // no copy constructor
        TextureObject &operator = (const TextureObject &other) = delete; // no copy assignement 
    public:
        TextureObject(){}
        virtual ~TextureObject(){}

        void BindForRead(unsigned int binding)
        {
            glActiveTexture(GL_TEXTURE0 + binding); 
            glBindTexture(descriptor.GLType, GLId);
        }
        void Unbind()
        {
            glBindTexture(descriptor.GLType, 0);
        }

        void GenerateMipMaps()
        {
            glBindTexture(descriptor.GLType, GLId);
            glGenerateMipmap(descriptor.GLType);
            glBindTexture(descriptor.GLType, 0);
        }
        
    protected:
        GLuint GLId;
        TextureDescriptor descriptor;
};


class Texture1D : public TextureObject
{
    public:
        Texture1D(){GLId = 0;}
        Texture1D(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            //Do sanity check on descriptor
            glGenTextures(1, &GLId);
            //Allocate:
        }
        virtual ~Texture1D()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        Texture1D(Texture1D &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
        }   

        Texture1D &operator = (Texture1D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
            return *this;
        }

        void Resize(unsigned int width, GLuint mipLevel = 0)
        {
            descriptor.width = width;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage1D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, 0, descriptor.internalFormat, descriptor.pixelFormat, NULL); 
            glBindTexture(descriptor.GLType, 0);
        }

        void Allocate(unsigned int width, void *data, GLuint mipLevel = 0)
        {
            descriptor.width = width;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage1D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, 0, descriptor.internalFormat, descriptor.pixelFormat, data); 
            glBindTexture(descriptor.GLType, 0);
        }
};

class ITexture1D : public TextureObject
{
    private:
        void Allocate()
        {
            glBindTexture(descriptor.GLType, GLId);
            glTexStorage1D(descriptor.GLType, descriptor.numMips, descriptor.sizedInternalFormat, descriptor.width);
            glBindTexture(descriptor.GLType, 0);
        }

    public:
        ITexture1D(){GLId = 0;}
        ITexture1D(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            glGenTextures(1, &GLId);
            Allocate();
        }

        virtual ~ITexture1D()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        ITexture1D(ITexture1D &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
        }   

        ITexture1D &operator = (ITexture1D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
            return *this;
        }

};


class Texture2D : public TextureObject
{
    public:
        Texture2D(){GLId = 0;}
        Texture2D(TextureDescriptor descriptor)
        {
            this->descriptor= descriptor;
            glGenTextures(1, &GLId);
            Resize(descriptor.width, descriptor.height);
        }

        Texture2D(TextureDescriptor descriptor, void *data)
        {
            this->descriptor= descriptor;
            glGenTextures(1, &GLId);
            Allocate(descriptor.width, descriptor.height, data);
        }

        virtual ~Texture2D()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        Texture2D(Texture2D &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
        }   

        Texture2D &operator = (Texture2D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
            return *this;
        }

        void Resize(unsigned int width, unsigned int height, GLuint mipLevel = 0)
        {
            descriptor.width = width;
            descriptor.height = height;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage2D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, height, 0, descriptor.internalFormat, descriptor.pixelFormat, NULL); 
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_S, descriptor.wrapS);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_T, descriptor.wrapT);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MIN_FILTER, descriptor.minFilter);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MAG_FILTER, descriptor.magFilter);
            glBindTexture(descriptor.GLType, 0);
        }
        
        void Allocate(unsigned int width, unsigned int height, void *data, GLuint mipLevel = 0)
        {
            descriptor.width = width;
            descriptor.height = height;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage2D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, height, 0, descriptor.internalFormat, descriptor.pixelFormat, data); 
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_S, descriptor.wrapS);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_T, descriptor.wrapT);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MIN_FILTER, descriptor.minFilter);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MAG_FILTER, descriptor.magFilter);
            glBindTexture(descriptor.GLType, 0);
        }

        void BindToTarget(GLuint frameBuffer, GLenum attachmentBinding, unsigned int level = 0)
        {
            glNamedFramebufferTexture(frameBuffer, attachmentBinding, GLId, level);
        }

        static Texture2D TextureFromFile(const std::string &filename, unsigned int numMips = 1)
        {
            int width, height, nrComponents;
            unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

            TextureDescriptor desc = TextureDescriptor();
            desc.GLType = GL_TEXTURE_2D;
            desc.numMips = numMips;
            desc.pixelFormat = GL_UNSIGNED_BYTE;
            desc.minFilter = GL_LINEAR; //IF MIPMAPPED: GL_LINEAR_MIPMAP_LINEAR but mips have to be generated with glGenerateMipmap(GL_TEXTURE_2D);
            desc.magFilter = GL_LINEAR;
            desc.wrapS = GL_REPEAT;
            desc.wrapT = GL_REPEAT;

            if (data)
            {
                GLenum format;
                if (nrComponents == 1)
                    format = GL_RED;
                else if (nrComponents == 3)
                    format = GL_RGB;
                else if (nrComponents == 4)
                    format = GL_RGBA;
                
                desc.width = width;
                desc.height = height;
                desc.internalFormat = format;
                desc.sizedInternalFormat = format;
                
            }
            else
            {
                std::cout << "Texture failed to load at path: " << filename << "\n";
            }

            Texture2D tex = Texture2D(desc, data);
            stbi_image_free(data);
            return tex;
        }  
};

class ITexture2D : public TextureObject
{
    private:
        void Allocate()
        {
            glBindTexture(descriptor.GLType, GLId);
            glTexStorage2D(descriptor.GLType, descriptor.numMips, descriptor.sizedInternalFormat, descriptor.width, descriptor.height);
            glBindTexture(descriptor.GLType, 0);
        }
    public:
        ITexture2D(){GLId = 0;}
        ITexture2D(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            glGenTextures(1, &GLId);
            Allocate();
        }

        virtual ~ITexture2D()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        ITexture2D(ITexture2D &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
        }   

        ITexture2D &operator = (ITexture2D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
            return *this;
        }
};

class Texture3D : public TextureObject
{
    public:
        Texture3D(){GLId = 0;}
        Texture3D(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            glGenTextures(1, &GLId);
            Resize(descriptor.width, descriptor.height, descriptor.depth);
        }

        virtual ~Texture3D()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        Texture3D(Texture3D &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
        }   

        Texture3D &operator = (Texture3D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
            return *this;
        }

        void Resize(unsigned int width, unsigned int height, unsigned int depth, GLuint mipLevel = 0)
        {
            descriptor.width = width;
            descriptor.height = height;
            descriptor.depth = depth;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage3D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, height, depth, 0, descriptor.internalFormat, descriptor.pixelFormat, NULL); 
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_S, descriptor.wrapS);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_T, descriptor.wrapT);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_R, descriptor.wrapR);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MIN_FILTER, descriptor.minFilter);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MAG_FILTER, descriptor.magFilter);
            glBindTexture(descriptor.GLType, 0);
        }

        void Allocate(unsigned int width, unsigned int height, unsigned int depth, void *data, GLuint mipLevel = 0)
        {
            descriptor.width = width;
            descriptor.height = height;
            descriptor.depth = depth;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage3D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, height, depth, 0, descriptor.internalFormat, descriptor.pixelFormat, data); 
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_S, descriptor.wrapS);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_T, descriptor.wrapT);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_R, descriptor.wrapR);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MIN_FILTER, descriptor.minFilter);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MAG_FILTER, descriptor.magFilter);
            glBindTexture(descriptor.GLType, 0);
        }
};

class ITexture3D : public TextureObject
{
    private:
        void Allocate()
        {
            glBindTexture(descriptor.GLType, GLId);
            glTexStorage3D(descriptor.GLType, descriptor.numMips, descriptor.sizedInternalFormat, descriptor.width, descriptor.height, descriptor.depth);
            glBindTexture(descriptor.GLType, 0);
        }

    public:
        ITexture3D(){GLId = 0;}
        ITexture3D(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            glGenTextures(1, &GLId);
            Allocate();
        }

        virtual ~ITexture3D()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        ITexture3D(ITexture3D &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
        }   

        ITexture3D &operator = (ITexture3D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            other.GLId = 0;
            return *this;
        }
};


class Texture2DMultisampled : public TextureObject
{
    private:
        unsigned int samples;

    public:
        Texture2DMultisampled(){GLId = 0;}
        Texture2DMultisampled(TextureDescriptor descriptor, unsigned int samples)
        {
            if (descriptor.GLType == GL_TEXTURE_2D_MULTISAMPLE || descriptor.GLType == GL_PROXY_TEXTURE_2D_MULTISAMPLE)
            {
                this->descriptor = descriptor;
                this->samples = samples;
                glGenTextures(1, &GLId);
                Resize(descriptor.height, descriptor.width);
            }
            else
            {
                GLId = 0;
                std::cout << "ERROR::TEXTURE: trying to assing wrong texture type as multisampled texture" << "\n";
            }
        }

        virtual ~Texture2DMultisampled()
        {
            if (GLId != 0)
            {
                glDeleteTextures(1, &GLId);
            }
        }

        Texture2DMultisampled(Texture2DMultisampled &&other)
        {
            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            this->samples = other.samples;
            other.GLId = 0;
        }   

        Texture2DMultisampled &operator = (Texture2DMultisampled &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->descriptor = other.descriptor;
            this->samples = other.samples;
            other.GLId = 0;
            return *this;
        }

        void Resize(unsigned int width, unsigned int height, GLboolean fixSampleLocations = GL_TRUE)
        {
            descriptor.width = width;
            descriptor.height = height;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage2DMultisample(descriptor.GLType, this->samples, descriptor.sizedInternalFormat, width, height, fixSampleLocations);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_S, descriptor.wrapS);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_T, descriptor.wrapT);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MIN_FILTER, descriptor.minFilter);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MAG_FILTER, descriptor.magFilter);
            glBindTexture(descriptor.GLType, 0);
        }

        void Resize(unsigned int width, unsigned int height, unsigned int samples, GLboolean fixSampleLocations = GL_TRUE)
        {
            descriptor.width = width;
            descriptor.height = height;
            this->samples = samples;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage2DMultisample(descriptor.GLType, samples, descriptor.sizedInternalFormat, width, height, fixSampleLocations);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_S, descriptor.wrapS);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_WRAP_T, descriptor.wrapT);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MIN_FILTER, descriptor.minFilter);
            glTexParameteri(descriptor.GLType, GL_TEXTURE_MAG_FILTER, descriptor.magFilter);
            glBindTexture(descriptor.GLType, 0);
        }

        void BindToTarget(GLuint frameBuffer, GLenum attachmentBinding, unsigned int level = 0)
        {
            glNamedFramebufferTexture(frameBuffer, attachmentBinding, GLId, level);
        }

};


class RenderBufferObject
{
    private:
        RenderBufferObject(const RenderBufferObject&) = delete; // no copy constructor
        RenderBufferObject &operator = (const RenderBufferObject &other) = delete; // no copy assignement 

    public:
        RenderBufferObject(){}
        virtual ~RenderBufferObject(){}

        void BindToTarget(GLuint frameBuffer, GLenum attachmentBinding, unsigned int level = 0)
        {
            glNamedFramebufferRenderbuffer(frameBuffer, attachmentBinding, GL_RENDERBUFFER, GLId);
        }
    
    protected:
        GLuint GLId;
        GLint sizedInternalFormat;
        unsigned int width = 1;
        unsigned int height = 1;
};


class RenderBuffer2D : public RenderBufferObject
{
    public:
        RenderBuffer2D(){GLId = 0;}
        RenderBuffer2D(GLint sizedInternalFormat, unsigned int width, unsigned int height)
        {
            this->sizedInternalFormat = sizedInternalFormat;
            this->width = width;
            this->height = height;
            glGenRenderbuffers(1, &GLId); 
            Resize(width ,height);
        }

        virtual ~RenderBuffer2D(){}
        RenderBuffer2D(RenderBuffer2D &&other)
        {
            this->GLId = other.GLId;
            this->sizedInternalFormat = other.sizedInternalFormat;
            this->width = other.width;
            this->height = other.height;
            other.GLId = 0;
        }   
        RenderBuffer2D &operator = (RenderBuffer2D &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->sizedInternalFormat = other.sizedInternalFormat;
            this->width = other.width;
            this->height = other.height;
            other.GLId = 0;
            return *this;
        }

        void Resize(unsigned int width, unsigned int height)
        {   
            this->width = width;
            this->height = height;
            glBindRenderbuffer(GL_RENDERBUFFER, GLId);
            glRenderbufferStorage(GL_RENDERBUFFER, sizedInternalFormat, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        
};


class RenderBuffer2DMultisample : public RenderBufferObject
{
    private:
        unsigned int samples;

    public:
        RenderBuffer2DMultisample(){GLId = 0;}
        RenderBuffer2DMultisample(GLint sizedInternalFormat, unsigned int width, unsigned int height, unsigned int samples)
        {
            this->sizedInternalFormat = sizedInternalFormat;
            this->width = width;
            this->height = height;
            this->samples = samples;
            glGenRenderbuffers(1, &GLId); 
            Resize(width ,height, samples);
        }

        virtual ~RenderBuffer2DMultisample(){}
        RenderBuffer2DMultisample(RenderBuffer2DMultisample &&other)
        {
            this->GLId = other.GLId;
            this->sizedInternalFormat = other.sizedInternalFormat;
            this->width = other.width;
            this->height = other.height;
            this->samples = other.samples;
            other.GLId = 0;
        }   
        RenderBuffer2DMultisample &operator = (RenderBuffer2DMultisample &&other)
        {
            if (GLId != 0)
            {
                glDeleteBuffers(1, &GLId);
            }

            this->GLId = other.GLId;
            this->sizedInternalFormat = other.sizedInternalFormat;
            this->width = other.width;
            this->height = other.height;
            this->samples = other.samples;
            other.GLId = 0;
            return *this;
        }

        void Resize(unsigned int width, unsigned int height)
        {   
            this->width = width;
            this->height = height;
            glBindRenderbuffer(GL_RENDERBUFFER, GLId);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, this->samples, sizedInternalFormat, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        void Resize(unsigned int width, unsigned int height, unsigned int samples)
        {   
            this->width = width;
            this->height = height;
            this->samples = samples;
            glBindRenderbuffer(GL_RENDERBUFFER, GLId);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, sizedInternalFormat, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
};


#endif