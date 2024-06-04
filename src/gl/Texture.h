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
    GLint minFilter;
    GLint magFilter;
    GLint wrapS;
    GLint wrapT;
    GLint wrapR;
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

        void SetBinding(unsigned int binding)
        {
            glActiveTexture(GL_TEXTURE0 + binding); 
            glBindTexture(descriptor.GLType, GLId);
        }
        void Unbind()
        {

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

//these are the necessary classes for implementing all functions
//void glTexStorage1D( GLenum target​, GLint levels​, GLint internalformat​, GLsizei width​ );
//void glTexImage1D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, void *data ); 
class Texture1D : public TextureObject
{
    private:
        Texture1D(const Texture1D&) = delete; // no copy constructor
        Texture1D &operator = (const Texture1D &other) = delete; // no copy assignement 

    public:
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

        void Allocate(GLuint mipLevel, unsigned int width, void *data)
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
        ITexture1D(const ITexture1D&) = delete; // no copy constructor
        ITexture1D &operator = (const ITexture1D &other) = delete; // no copy assignement   

        void Allocate()
        {
            glBindTexture(descriptor.GLType, GLId);
            glTexStorage1D(descriptor.GLType, descriptor.numMips, descriptor.sizedInternalFormat, descriptor.width);
            glBindTexture(descriptor.GLType, 0);
        }

    public:
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

//void glTexStorage2D( GLenum target​, GLint levels​, GLint internalformat​, GLsizei width​, GLsizei height​ );
//void glTexImage2D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, void *data ); 
class Texture2D : public TextureObject
{
    private:
        Texture2D(const Texture2D&) = delete; // no copy constructor
        Texture2D &operator = (const Texture2D &other) = delete; // no copy assignement 

    public:
        Texture2D(TextureDescriptor descriptor)
        {
            this->descriptor= descriptor;
            glGenTextures(1, &GLId);
        }

        Texture2D(TextureDescriptor descriptor, unsigned int mipLevel, void *data)
        {
            this->descriptor= descriptor;
            glGenTextures(1, &GLId);
            Allocate(mipLevel, descriptor.width, descriptor.height, data);
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
        
        void Allocate(GLuint mipLevel, unsigned int width, unsigned int height, void *data)
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

            Texture2D tex = Texture2D(desc, 0, data);
            stbi_image_free(data);
            return tex;
        }  
};

class ITexture2D : public TextureObject
{
    private:
        ITexture2D(const ITexture2D&) = delete; // no copy constructor
        ITexture2D &operator = (const ITexture2D &other) = delete; // no copy assignement 

        void Allocate()
        {
            glBindTexture(descriptor.GLType, GLId);
            glTexStorage2D(descriptor.GLType, descriptor.numMips, descriptor.sizedInternalFormat, descriptor.width, descriptor.height);
            glBindTexture(descriptor.GLType, 0);
        }
    public:
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

//void glTexStorage3D( GLenum target​, GLint levels​, GLint internalformat​, GLsizei width​, GLsizei height​, GLsizei depth​ );
//void glTexImage3D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, void *data ); 
class Texture3D : public TextureObject
{
    private:
        Texture3D(const Texture3D&) = delete; // no copy constructor
        Texture3D &operator = (const Texture3D &other) = delete; // no copy assignement 

    public:
        Texture3D(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            glGenTextures(1, &GLId);
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

        void Allocate(GLuint mipLevel, unsigned int width, unsigned int height, unsigned int depth, void *data)
        {
            descriptor.width = width;
            descriptor.height = height;
            descriptor.depth = depth;
            glBindTexture(descriptor.GLType, GLId);
            glTexImage3D(descriptor.GLType, mipLevel, descriptor.sizedInternalFormat, width, height, depth, 0, descriptor.internalFormat, descriptor.pixelFormat, data); 
            glBindTexture(descriptor.GLType, 0);
        }
};

class ITexture3D : public TextureObject
{
    private:
        ITexture3D(const ITexture3D&) = delete; // no copy constructor
        ITexture3D &operator = (const ITexture3D &other) = delete; // no copy assignement 

        void Allocate()
        {
            glBindTexture(descriptor.GLType, GLId);
            glTexStorage3D(descriptor.GLType, descriptor.numMips, descriptor.sizedInternalFormat, descriptor.width, descriptor.height, descriptor.depth);
            glBindTexture(descriptor.GLType, 0);
        }

    public:
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

//void glTexStorage2DMultisample( GLenum target​, GLsizei samples​​, GLint internalformat​, GLsizei width​, GLsizei height​, GLboolean fixedsamplelocations​​ );
//void glTexImage2DMultisample( GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean ﬁxedsamplelocations ); 
class Texture2DMultisampled : public TextureObject
{
    private:
        Texture2DMultisampled(const Texture2DMultisampled&) = delete; // no copy constructor
        Texture2DMultisampled &operator = (const Texture2DMultisampled &other) = delete; // no copy assignement 

    public:
        Texture2DMultisampled(TextureDescriptor descriptor)
        {
            this->descriptor = descriptor;
            glGenTextures(1, &GLId);
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
            other.GLId = 0;
            return *this;
        }
};




#endif