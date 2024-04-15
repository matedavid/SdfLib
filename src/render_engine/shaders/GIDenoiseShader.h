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
        mEnabledLocation = glGetUniformLocation(programId, "enabled");

        mSigmaLocation = glGetUniformLocation(programId, "uSigma");
        mkSigmaLocation = glGetUniformLocation(programId, "ukSigma");
        mThresholdLocation = glGetUniformLocation(programId, "uThreshold");
    }

    void setColorTexture(unsigned int colorTexture) { mColorTexture = colorTexture; }

    void setEnabled(bool enabled) { mEnabled = enabled; }

    void setSigma(float sigma) { mSigma = sigma; }

    void setkSigma(float kSigma) { mkSigma = kSigma; }

    void setThreshold(float treshold) { mThreshold = treshold; }

    void bind() override
    {
        glUniform1i(mColorTextureLocation, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mColorTexture);
        
        glUniform1i(mEnabledLocation, mEnabled);

        glUniform1f(mSigmaLocation, mSigma);
        glUniform1f(mkSigmaLocation, mkSigma);
        glUniform1f(mThresholdLocation, mThreshold);
    }

private:
    int mColorTextureLocation;
    unsigned int mColorTexture;

    bool mEnabled = false;
    unsigned int mEnabledLocation;

    unsigned int mSigmaLocation;
    float mSigma;

    unsigned int mkSigmaLocation;
    float mkSigma;

    unsigned int mThresholdLocation;
    float mThreshold;
};