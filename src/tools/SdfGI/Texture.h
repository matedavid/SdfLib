#pragma once

#include <glad/glad.h>

class Texture
{
public:
    struct Description
    {
        uint32_t width, height;
        GLint internalFormat;
        GLenum format;
        GLenum pixelDataType;
    };

    Texture(const Description &description);
    ~Texture();

    void bind() const;
    void unbind() const;

    [[nodiscard]] GLuint id() const { return m_id; }

private:
    GLuint m_id;
    Description m_description;
};