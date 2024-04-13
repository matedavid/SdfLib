#pragma once

#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>

class GIDenoiseShader : public Shader<GIDenoiseShader>
{
public:
    GIDenoiseShader() : 
        Shader(SHADER_PATH + "screenPlane.vert", SHADER_PATH + "sdfGIDenoise.frag")
    {
        unsigned int programId = getProgramId();

        mColorTextureLocation = glGetUniformLocation(programId, "colorTexture");
    }

    void setColorTexture(unsigned int colorTexture) { mColorTexture = colorTexture; }

    void bind() override
    {
        glUniform1i(mColorTextureLocation, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mColorTexture);
    }

private:
    int mColorTextureLocation;
    unsigned int mColorTexture;
};