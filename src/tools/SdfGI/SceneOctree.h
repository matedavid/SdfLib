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
        Gray  = 0, // Contains triangles, not last depth
        Black = 1, // Contains triangles, last depth
        White = 2, // Empty, last depth
    };

    glm::vec3 center;
    float halfSize;
    int depth;
    Type type;

    // glm::vec3 color{0.0f};
    sdflib::MaterialProperties material;

    std::vector<size_t> triangles;
    std::array<std::unique_ptr<OctreeNode>, 8> children;
};

constexpr uint32_t SHADER_LEAF_MASK  = 0xC0000000; // 0x80000000;
constexpr uint32_t SHADER_CHILD_MASK = 0x3FFFFFFF; // 0x7fffffff;

struct ShaderOctreeNode
{
    // 32 bits
    // - bit 31,30: node type
    // - bit 29-0:  children idx
    uint32_t data = 0; float _padding1[3];

    // Bbox
    glm::vec3 min; float _padding2;
    glm::vec3 max; float _padding3;

    glm::vec4 color{};
    // roughness, metallic, -, -
    glm::vec4 materialProperties{};

    // radiance caching (orientations in order: -x, x, -y, y, -z, z)
    glm::vec4 readRadiance[6]{};
    glm::vec4 writeRadiance[6]{};

    void setNodeType(OctreeNode::Type type)
    {
        data |= (static_cast<uint32_t>(type) << 30) & SHADER_LEAF_MASK;
    }

    bool getIsLeaf() const
    {
        return (data & SHADER_LEAF_MASK) != 0;
    }

    void setIndex(uint32_t idx)
    {
        data = (data & SHADER_LEAF_MASK) | (idx & SHADER_CHILD_MASK);
    }

    uint32_t getIndex() const
    {
        return data & SHADER_CHILD_MASK;
    }
};

class SceneOctree
{
public:
    struct RenderConfig
    {
        int maxDepth;
        int startDepth;
    };

    SceneOctree(const sdflib::Mesh &mesh, RenderConfig config);
    ~SceneOctree() = default;

    [[nodiscard]] int getStartGridSize() { return std::exp2(mRenderConfig.startDepth); }

    const std::unique_ptr<OctreeNode> &getRoot() { return mRoot; }
    const std::vector<ShaderOctreeNode> &getShaderOctreeData() { return mShaderOctreeData; }
    // const std::vector<uint32_t> &getLeafIndices() const { return mLeafIndices; }

private:
    RenderConfig mRenderConfig;

    std::unique_ptr<OctreeNode> mRoot;
    std::vector<ShaderOctreeNode> mShaderOctreeData;

    // std::vector<uint32_t> mLeafIndices;

    std::vector<Triangle> mTriangles;
    std::vector<glm::vec3> mVertices;

    void renderNode(std::unique_ptr<OctreeNode> &node, const sdflib::Mesh &mesh);
    void slowTriangleIntersectionTest(sdflib::BoundingBox bbox, const std::vector<size_t> &triangles, std::vector<size_t> &outTriangles);

    uint32_t generateShaderOctreeData(const OctreeNode* node);
};
