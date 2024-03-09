#pragma once

#include <array>
#include <memory>
#include <glm/glm.hpp>

#include "SdfLib/utils/Mesh.h"

struct Triangle
{
    size_t v0;
    size_t v1;
    size_t v2;
};

struct OctreeNode
{
    enum class Type
    {
        Black, // Contains triangles, last depth
        White, // Empty, last depth
        Gray   // Contains triangles, not last depth
    };

    glm::vec3 center;
    float halfSize;
    int depth;
    Type type;

    glm::vec3 color{0.0f};

    std::vector<size_t> triangles;
    std::array<std::unique_ptr<OctreeNode>, 8> children;
};

static uint32_t LEAF_MASK = 0x80000000;
static uint32_t INDEX_MASK = 0x7FFFFFFF;

struct ShaderOctreeNode
{
    // 1 bit: isLeaf
    // 31 bits: index to pos in array where children are 
    uint32_t value = 0;

    float _padding1[3];

    glm::vec3 min;
    float _padding2;
    glm::vec3 max;
    float _padding3;

    glm::vec4 color{};

    void setIsLeaf() 
    {
        value = value | LEAF_MASK;
    }

    void setIndex(uint32_t index)
    {
        value = (value & LEAF_MASK) | (index & INDEX_MASK);
    }
};

class SceneOctree
{
public:
    SceneOctree(const sdflib::Mesh &mesh, int maxDepth);
    ~SceneOctree() = default;

    const std::unique_ptr<OctreeNode> &getRoot() { return mRoot; } 
    const std::vector<ShaderOctreeNode> &getShaderOctreeData() { return mShaderOctreeData; }

private:
    struct RenderConfig
    {
        int maxDepth;
    };
    RenderConfig mRenderConfig;

    std::unique_ptr<OctreeNode> mRoot;
    std::vector<ShaderOctreeNode> mShaderOctreeData;

    std::vector<Triangle> mTriangles;
    std::vector<glm::vec3> mVertices;

    void renderNode(std::unique_ptr<OctreeNode> &node, const sdflib::Mesh& mesh);
    void slowTriangleIntersectionTest(sdflib::BoundingBox bbox, const std::vector<size_t> &triangles, std::vector<size_t> &outTriangles);
};
