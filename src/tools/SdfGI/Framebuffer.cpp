#include "Framebuffer.h"

#include "Texture.h"

Framebuffer::Framebuffer()
{
    glGenFramebuffers(1, &m_id);
}

Framebuffer::~Framebuffer()
{
    glDeleteFramebuffers(1, &m_id);
}

void Framebuffer::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
}

void Framebuffer::unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attach(const Texture &texture, GLenum attachment)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture.id(), 0);
}

bool Framebuffer::bake() const
{
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}
