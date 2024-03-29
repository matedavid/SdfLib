#ifndef SDF_OCTREE_LIGHT_SHADER_H
#define SDF_OCTREE_LIGHT_SHADER_H

#include "Shader.h"
#include "SdfLib/OctreeSdf.h"
#include "SdfLib/utils/Timer.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>

#include "tools/SdfGI/Cubemap.h"
#include "tools/SdfGI/SceneOctree.h"

class SdfOctreeGIShader : public Shader<SdfOctreeGIShader>
{
public:
    SdfOctreeGIShader(sdflib::IOctreeSdf& octreeSdf, SceneOctree& sceneOctree) : 
        Shader(SHADER_PATH + "sdfOctreeGI.vert", "", SHADER_PATH + "sdfOctreeGI.frag", getFragmentShaderHeader(octreeSdf))
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
        timeLocation = glGetUniformLocation(mRenderProgramId, "time");
        timer.start();

        mEpsilonLocation = glGetUniformLocation(mRenderProgramId, "epsilon");

        //Global Illumination Settings
        mUseIndirectLocation = glGetUniformLocation(mRenderProgramId, "useIndirect");
        mNumSamplesLocation = glGetUniformLocation(mRenderProgramId, "numSamples");
        mMaxDepthLocation = glGetUniformLocation(mRenderProgramId, "maxDepth");
        mMaxRaycastIterationsLocation = glGetUniformLocation(mRenderProgramId, "maxRaycastIterations");
        mUseDirectSphereSamplingLocation = glGetUniformLocation(mRenderProgramId, "useDirectSphereSampling");
        mFrameIndexLocation = glGetUniformLocation(mRenderProgramId, "frameIndex");
        mUseCubemapSkyboxLocation = glGetUniformLocation(mRenderProgramId, "useCubemapSkybox");
        mSkyboxColorLocation = glGetUniformLocation(mRenderProgramId, "skyboxColor");
        mCubemapSkyboxLocation = glGetUniformLocation(mRenderProgramId, "cubemapSkybox");

        //Options
        mUseSoftShadowsLocation = glGetUniformLocation(mRenderProgramId, "useSoftShadows");
        mMaxShadowIterationsLocation = glGetUniformLocation(mRenderProgramId, "maxShadowIterations");

        //Lighting
        mLightNumberLocation = glGetUniformLocation(mRenderProgramId, "lightNumber");
        mLightPosLocation = glGetUniformLocation(mRenderProgramId, "lightPos");
        mLightColorLocation = glGetUniformLocation(mRenderProgramId, "lightColor");
        mLightIntensityLocation = glGetUniformLocation(mRenderProgramId, "lightIntensity");
        mLightRadiusLocation = glGetUniformLocation(mRenderProgramId, "lightRadius");
        
        //Material
        mMetallicLocation = glGetUniformLocation(mRenderProgramId, "matMetallic");
        mRoughnessLocation = glGetUniformLocation(mRenderProgramId, "matRoughness");
        mAlbedoLocation = glGetUniformLocation(mRenderProgramId, "matAlbedo");
        mF0Location = glGetUniformLocation(mRenderProgramId, "matF0");

        mSimpleLocation = glGetUniformLocation(mRenderProgramId, "simple");

        // Set octree data
        glGenBuffers(1, &mOctreeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mOctreeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, octreeSdf.getOctreeData().size() * sizeof(sdflib::OctreeSdf::OctreeNode), octreeSdf.getOctreeData().data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mOctreeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Set scene octree data
        glGenBuffers(1, &mSceneOctreeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mSceneOctreeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sceneOctree.getShaderOctreeData().size() * sizeof(ShaderOctreeNode), sceneOctree.getShaderOctreeData().data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mSceneOctreeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    ~SdfOctreeGIShader()
    {
        glDeleteBuffers(1, &mOctreeSSBO);
        glDeleteBuffers(1, &mSceneOctreeSSBO);
    }

    //Global Illumination Settings
    void setUseIndirect(bool useIndirect)
    {
        mUseIndirect = useIndirect;
    }

    void setNumSamples(int numSamples)
    {
        mNumSamples = numSamples;
    }

    void setMaxDepth(int maxDepth)
    {
        mMaxDepth = maxDepth;
    }

    void setMaxRaycastIterations(int maxIterations)
    {
        mMaxRaycastIterations = maxIterations;
    }

    void setUseDirectSphereSampling(bool useDirectSphereSampling)
    {
        mUseDirectSphereSampling = useDirectSphereSampling;
    }

    void setFrameIndex(int frameIndex)
    {
        mFrameIndex = frameIndex;
    }

    void setUseCubemapSkybox(bool use) {
        mUseCubemapSkybox = use;
    }

    void setSkyboxColor(glm::vec3 skyboxColor) {
        mSkyboxColor = skyboxColor;
    }

    void setCubemapSkybox(std::shared_ptr<Cubemap> skybox)
    {
        mCubemapSkybox = std::move(skybox);
    }

    //Options
    void setUseSoftShadows(bool useSoftShadows)
    {
        mUseSoftShadows = useSoftShadows;
    }

    void setMaxShadowIterations(int maxShadowIterations)
    {
        mMaxShadowIterations = maxShadowIterations;
    }

    //Material
    void setMaterial(glm::vec3 albedo, float roughness, float metallic, glm::vec3 F0)
    {
        mAlbedo = albedo;
        mRoughness = roughness;
        mMetallic = metallic;
        mF0 = F0;
    }

    void setSimple(bool simple)
    {
        mSimple = simple;
    }

    //Lighting
    void setLightNumber(int lightNumber)
    {
        mLightNumber = lightNumber;
    }
    void setLightInfo(int i, glm::vec3 lightPosition, glm::vec3 lightColor, float lightIntensity, float lightRadius)
    {
        mLightPosition[i] = lightPosition;
        mLightColor[i] = lightColor;
        mLightIntensity[i] = lightIntensity;
        mLightRadius[i] = lightRadius;
    }

    void bind() override
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mOctreeSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mSceneOctreeSSBO);

        glUniformMatrix4fv(worldToStartGridMatrixLocation, 1, GL_FALSE, glm::value_ptr(worldToStartGridMatrix));
        glUniformMatrix3fv(normalWorldToStartGridMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalWorldToStartGridMatrix));
        glUniform3f(startGridSizeLocation, startGridSize.x, startGridSize.y, startGridSize.z);
        glUniform1f(minBorderValueLocation, minBorderValue);
        glUniform1f(distanceScaleLocation, distanceScale);
        glUniform1f(timeLocation, timer.getElapsedSeconds());

        //mEpsilon = 0.5f*(2.0f/mRenderTextureSize.x); //radius of a pixel in screen space
        mEpsilon = 1e-3;
        glUniform1f(mEpsilonLocation, mEpsilon);

        glUniform1i(mSimpleLocation, mSimple);

        //Global Illumination options
        glUniform1i(mUseIndirectLocation, mUseIndirect);
        glUniform1i(mNumSamplesLocation, mNumSamples);
        glUniform1i(mMaxDepthLocation, mMaxDepth);
        glUniform1i(mMaxRaycastIterationsLocation, mMaxRaycastIterations);
        glUniform1i(mUseDirectSphereSamplingLocation, mUseDirectSphereSampling);
        glUniform1i(mFrameIndexLocation, mFrameIndex);
        glUniform1f(mUseCubemapSkyboxLocation, mUseCubemapSkybox);
        glUniform3f(mSkyboxColorLocation, mSkyboxColor.r, mSkyboxColor.g, mSkyboxColor.b);

        if (mCubemapSkybox != nullptr)
        {
            glUniform1i(mCubemapSkyboxLocation, 0);
            mCubemapSkybox->bind(0);
        }

        //Options
        glUniform1i(mUseSoftShadowsLocation, mUseSoftShadows);
        glUniform1i(mMaxShadowIterationsLocation, mMaxShadowIterations);

        //Lighting
        glUniform1i(mLightNumberLocation, mLightNumber);
        glUniform3fv(mLightPosLocation, 4, glm::value_ptr(mLightPosition[0]));
        glUniform3fv(mLightColorLocation, 4, glm::value_ptr(mLightColor[0]));
        glUniform1fv(mLightIntensityLocation, 4, &mLightIntensity[0]);
        glUniform1fv(mLightRadiusLocation, 4, &mLightRadius[0]);

        //Material
        glUniform1f(mMetallicLocation, mMetallic);
        glUniform1f(mRoughnessLocation, mRoughness);
        glUniform3f(mAlbedoLocation, mAlbedo.x, mAlbedo.y, mAlbedo.z);
        glUniform3f(mF0Location, mF0.x, mF0.y, mF0.z);

    }

    unsigned int mSceneOctreeSSBO;

private:
    unsigned int mOctreeSSBO;

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

    sdflib::Timer timer;
    unsigned int timeLocation;

    unsigned int mEpsilonLocation;
    float mEpsilon = 0.0001f;

    // Global Illumination Settings
    unsigned int mUseIndirectLocation;
    bool mUseIndirect = false;

    unsigned int mNumSamplesLocation;
    int mNumSamples;

    unsigned int mMaxDepthLocation;
    int mMaxDepth;

    unsigned int mMaxRaycastIterationsLocation;
    int mMaxRaycastIterations;

    unsigned int mUseDirectSphereSamplingLocation;
    bool mUseDirectSphereSampling = false;

    unsigned int mFrameIndexLocation;
    int mFrameIndex;

    unsigned int mUseCubemapSkyboxLocation;
    bool mUseCubemapSkybox = false;

    unsigned int mSkyboxColorLocation;
    glm::vec3 mSkyboxColor;

    unsigned int mCubemapSkyboxLocation;
    std::shared_ptr<Cubemap> mCubemapSkybox;

    //Options
    unsigned int mMaxShadowIterationsLocation;
    int mMaxShadowIterations = 512;

    unsigned int mUseSoftShadowsLocation;
    bool mUseSoftShadows = false;

    //Lighting
    unsigned int mLightNumberLocation;
    unsigned int mLightPosLocation;
    unsigned int mLightColorLocation;
    unsigned int mLightIntensityLocation;
    unsigned int mLightRadiusLocation;

    unsigned int mSimpleLocation;
    bool mSimple = false;

    int mLightNumber = 1;
    glm::vec3 mLightPosition[4] =
    {
        glm::vec3 (1.0f, 2.0f, 1.0f),
        glm::vec3 (-1.0f, 2.0f, 1.0f),
        glm::vec3 (1.0f, 2.0f, -1.0f),
        glm::vec3 (-1.0f, 2.0f, -1.0f)
    };

    glm::vec3 mLightColor[4] =
    {
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 0.0f, 1.0f)
    };

    float mLightIntensity[4] = 
    {
        10.0f,
        10.0f,
        10.0f,
        10.0f
    };

    float mLightRadius[4] =
    {
        0.1f,
        0.1f,
        0.1f,
        0.1f
    };

    //Material
    unsigned int mMetallicLocation;
    unsigned int mRoughnessLocation;
    unsigned int mAlbedoLocation;
    unsigned int mF0Location;

    float mMetallic = 0.0f;
    float mRoughness = 0.5f;
    glm::vec3 mAlbedo = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 mF0 = glm::vec3(0.07f, 0.07f, 0.07f);

    std::string getFragmentShaderHeader(sdflib::IOctreeSdf& octreeSdf)
    {
        switch (octreeSdf.getFormat())
        {
        case sdflib::IOctreeSdf::TRILINEAR_OCTREE:
            return "#define USE_TRILINEAR_INTERPOLATION\n\n";
            break;
        case sdflib::IOctreeSdf::TRICUBIC_OCTREE:
            return "#define USE_TRICUBIC_INTERPOLATION\n\n";
            break;
        default:
            assert(false);
            break;
        }
        return "";
    }
};

#endif