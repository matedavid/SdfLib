#include "Texture.h"

Texture::Texture(const Description &description) : m_description(description)
{
    glGenTextures(1, &m_id);
    bind();

    glTexImage2D(GL_TEXTURE_2D, 0, m_description.internalFormat, m_description.width, m_description.height, 0, m_description.format, m_description.pixelDataType, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    unbind();
}

Texture::~Texture()
{
    glDeleteTextures(1, &m_id);
}

void Texture::bind() const
{
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}
