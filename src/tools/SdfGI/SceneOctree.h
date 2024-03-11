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

struct ShaderOctreeNode
{
    uint32_t isLeaf = 0; float _padding1[3];
    glm::uvec4 childrenIndices[8];

    // Bbox
    glm::vec3 min; float _padding2;
    glm::vec3 max; float _padding3;

    glm::vec4 color{};

    void setIsLeaf()
    {
        isLeaf = 1;
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

    void renderNode(std::unique_ptr<OctreeNode> &node, const sdflib::Mesh &mesh);
    void slowTriangleIntersectionTest(sdflib::BoundingBox bbox, const std::vector<size_t> &triangles, std::vector<size_t> &outTriangles);

    uint32_t generateShaderOctreeData(const std::unique_ptr<OctreeNode> &node);
};
