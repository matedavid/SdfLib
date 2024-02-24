#pragma once

#include "Shader.h"

class GIAccumulationShader : public Shader<GIAccumulationShader>
{
public:
    GIAccumulationShader() : Shader(SHADER_PATH + "screenPlane.vert", SHADER_PATH + "sdfGIAccumulation.frag")
    {
        mColorTextureLocation = glGetUniformLocation(getProgramId(), "colorTexture");
        mAcccumulationTextureLocation = glGetUniformLocation(getProgramId(), "accumulationTexture");

        mAccumulationFrameLocation = glGetUniformLocation(getProgramId(), "accumulationFrame");
    }

    void setColorTexture(unsigned int colorTexture) { mColorTexture = colorTexture; }
    void setAccumulationTexture(unsigned int accumulationTexture) { mAccumulationTexture = accumulationTexture; }
    void setAccumulationFrame(unsigned int accumulationFrame) { mAccumulationFrame = accumulationFrame; }

    void bind() override
    {
        glUniform1i(mColorTextureLocation, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mColorTexture);

        glUniform1i(mAcccumulationTextureLocation, 1);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mAccumulationTexture);

        glUniform1i(mAccumulationFrameLocation, mAccumulationFrame);
    }

private:
    int mColorTextureLocation;
    unsigned int mColorTexture;

    int mAcccumulationTextureLocation;
    unsigned int mAccumulationTexture;

    int mAccumulationFrameLocation;
    unsigned int mAccumulationFrame;
};
