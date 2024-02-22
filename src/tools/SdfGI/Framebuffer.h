#pragma once

#include <glad/glad.h>

class Texture;

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();

    void bind() const;
    void unbind() const;

    void attach(const Texture& texture, GLenum attachment);

    [[nodiscard]] bool bake() const;

private:
    GLuint m_id;
};