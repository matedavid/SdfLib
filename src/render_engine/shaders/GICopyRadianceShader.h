#pragma once

#include "Shader.h"
#include "SdfLib/OctreeSdf.h"
#include "SdfLib/utils/Timer.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>

class GICopyRadianceShader : public Shader<GICopyRadianceShader>
{
public:
    GICopyRadianceShader(sdflib::IOctreeSdf& octreeSdf, unsigned int sceneOctreeSSBO) : 
        Shader(SHADER_PATH + "sdfOctreeGI.vert", "", SHADER_PATH + "sdfGICopyRadiance.frag", ""), mSceneOctreeSSBO(sceneOctreeSSBO)
    {
        unsigned int mRenderProgramId = getProgramId();

        worldToStartGridMatrixLocation = glGetUniformLocation(mRenderProgramId, "worldToStartGridMatrix");
        worldToStartGridMatrix = glm::scale(glm::mat4x4(1.0f), 1.0f / octreeSdf.getGridBoundingBox().getSize()) *
                                 glm::translate(glm::mat4x4(1.0f), -octreeSdf.getGridBoundingBox().min);

        normalWorldToStartGridMatrixLocation = glGetUniformLocation(mRenderProgramId, "normalWorldToStartGridMatrix");
        normalWorldToStartGridMatrix = glm::inverseTranspose(glm::mat3(worldToStartGridMatrix));

        startGridSizeLocation = glGetUniformLocation(mRenderProgramId, "startGridSize");
        startGridSize = glm::vec3(octreeSdf.getStartGridSize());

        minBorderValueLocation = glGetUniformLocation(mRenderProgramId, "minBorderValue");
        minBorderValue = octreeSdf.getOctreeMinBorderValue();
        distanceScaleLocation = glGetUniformLocation(mRenderProgramId, "distanceScale");
        distanceScale = 1.0f / octreeSdf.getGridBoundingBox().getSize().x;

        resetAccumulationLocation = glGetUniformLocation(mRenderProgramId, "reset");
    }

    void setReset(bool reset_)
    {
        resetAccumulation = reset_;
    }

    void bind() override
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mSceneOctreeSSBO);

        glUniformMatrix4fv(worldToStartGridMatrixLocation, 1, GL_FALSE, glm::value_ptr(worldToStartGridMatrix));
        glUniformMatrix3fv(normalWorldToStartGridMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalWorldToStartGridMatrix));
        glUniform3f(startGridSizeLocation, startGridSize.x, startGridSize.y, startGridSize.z);
        glUniform1f(minBorderValueLocation, minBorderValue);
        glUniform1f(distanceScaleLocation, distanceScale);

        glUniform1i(resetAccumulationLocation, resetAccumulation);
    }

private:
    unsigned int mSceneOctreeSSBO;

    glm::mat4x4 worldToStartGridMatrix;
    unsigned int worldToStartGridMatrixLocation;

    glm::mat3 normalWorldToStartGridMatrix;
    unsigned int normalWorldToStartGridMatrixLocation;

    glm::vec3 startGridSize;
    unsigned int startGridSizeLocation;

    float minBorderValue;
    unsigned int minBorderValueLocation;

    float distanceScale;
    unsigned int distanceScaleLocation;

    bool resetAccumulation = false;
    unsigned int resetAccumulationLocation;
};