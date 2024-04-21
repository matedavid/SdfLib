#pragma once

#include "Shader.h"
#include "SdfLib/OctreeSdf.h"
#include "SdfLib/utils/Timer.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include "tools/SdfGI/SceneOctree.h"

class GICopyRadianceShader 
{
public:
    GICopyRadianceShader(std::shared_ptr<SceneOctree> octree, unsigned int sceneOctreeSSBO) : 
        mSceneOctree(octree), mSceneOctreeSSBO(sceneOctreeSSBO)
    {
        // Compile compute shader
        {
            std::ifstream file(SHADER_PATH + "sdfGICopyRadiance.comp");
            std::string shaderContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            const char* shaderContentC = shaderContent.c_str();

            unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);
            glShaderSource(compute, 1, &shaderContentC, NULL);
            glCompileShader(compute);

            int success;
            glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(compute, 512, NULL, infoLog);
                std::cout << "-> Compute Shader error ( " << "sdfGICopyRadiance.comp" << " ):" << std::endl;
                std::cout << infoLog << std::endl;
            }

            mRenderProgramId = glCreateProgram();
            glAttachShader(mRenderProgramId, compute);
            glLinkProgram(mRenderProgramId);

            glGetProgramiv(mRenderProgramId, GL_LINK_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(mRenderProgramId, 512, NULL, infoLog);
                std::cout << "-> Link Shader error" << std::endl;
                std::cout << infoLog << std::endl;
            }

            glDeleteShader(compute);
        }

        mLeafIndicesSize = octree->getLeafIndices().size();

        glGenBuffers(1, &mLeafIndicesSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLeafIndicesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, mLeafIndicesSize * sizeof(uint32_t), octree->getLeafIndices().data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, mLeafIndicesSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        constexpr int LOCAL_SIZE_X = 64;

        mNumGroupsX = glm::ceil(float(mLeafIndicesSize) / float(LOCAL_SIZE_X));

        std::cout << "Num work groups x: " << mNumGroupsX << " for local size: " << LOCAL_SIZE_X << " and size(): " << mLeafIndicesSize << "\n";

        mResetAccumulationLocation = glGetUniformLocation(mRenderProgramId, "reset");
        mLeafIndicesSizeLocation = glGetUniformLocation(mRenderProgramId, "leafIndicesSize");
    }

    ~GICopyRadianceShader()
    {
        glDeleteProgram(mRenderProgramId);
    }

    void setReset(bool reset_)
    {
        mResetAccumulation = reset_;
    }

    void dispatch()
    {
        glUseProgram(mRenderProgramId);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mSceneOctreeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLeafIndicesSSBO);

        glUniform1i(mResetAccumulationLocation, mResetAccumulation);
        glUniform1i(mLeafIndicesSizeLocation, mLeafIndicesSize);

        glDispatchCompute(mNumGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

private:
    unsigned int mRenderProgramId;

    std::shared_ptr<SceneOctree> mSceneOctree;
    unsigned int mSceneOctreeSSBO;
    unsigned int mLeafIndicesSSBO;

    unsigned int mNumGroupsX;

    bool mResetAccumulation = false;
    unsigned int mResetAccumulationLocation;

    int mLeafIndicesSize;
    unsigned int mLeafIndicesSizeLocation;
};