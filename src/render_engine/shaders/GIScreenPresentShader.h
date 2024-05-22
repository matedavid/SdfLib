#pragma once

#include "Shader.h"

class GIScreenPresentShader : public Shader<GIScreenPresentShader>
{
public:
    GIScreenPresentShader() : Shader(SHADER_PATH + "screenPlane.vert", SHADER_PATH + "sdfGIScreenPresent.frag") {
        mInputTextureLocation = glGetUniformLocation(getProgramId(), "inputTexture");
        mTonemappingConstantLocation = glGetUniformLocation(getProgramId(), "tonemappingConstant");
    }

    void setInputTexture(unsigned int texture) { mInputTexture = texture; }

    void setTonemappingConstant(unsigned int constant) { mTonemappingConstant = constant; }

    void bind() override
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mInputTexture);

        glUniform1i(mInputTextureLocation, 0);
        glUniform1f(mTonemappingConstantLocation, mTonemappingConstant);
    }

private:
    int mInputTextureLocation;
    unsigned int mInputTexture;

    int mTonemappingConstantLocation;
    float mTonemappingConstant = 2.2f;
};
