#pragma once

#include "Shader.h"

class GIScreenPresentShader : public Shader<GIScreenPresentShader>
{
public:
    GIScreenPresentShader() : Shader(SHADER_PATH + "screenPlane.vert", SHADER_PATH + "sdfGIScreenPresent.frag") {}

    void setInputTexture(unsigned int texture) { mInputTexture = texture; }

    void bind() override
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mInputTexture);
    }

private:
    unsigned int mInputTexture;
};
