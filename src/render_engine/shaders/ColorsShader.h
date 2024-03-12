#ifndef COLORS_SHADER_H
#define COLORS_SHADER_H

#include "Shader.h"

class ColorsShader : public Shader<ColorsShader>
{
public:
    ColorsShader() : Shader(SHADER_PATH + "colors.vert", SHADER_PATH + "colors.frag") {
        mColorLocation = glGetUniformLocation(getProgramId(), "color");
    }

    void setColor(glm::vec3 color)
    {
        mColor = color;
    }

    void bind() override
    {
        glUseProgram(getProgramId());
        glUniform3f(mColorLocation, mColor.r, mColor.g, mColor.b);
    }

private:
    unsigned int mColorLocation;
    glm::vec3 mColor{0.0f};
};

#endif