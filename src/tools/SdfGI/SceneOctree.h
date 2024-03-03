#pragma once

#include <array>
#include <memory>

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

    std::vector<size_t> triangles;
    // Children order: x first, y second, z third
    std::array<std::unique_ptr<OctreeNode>, 8> children;
};

class SceneOctree
{
public:
    SceneOctree(const sdflib::Mesh &mesh, int maxDepth);
    ~SceneOctree() = default;

    void print();
    const std::unique_ptr<OctreeNode> &getRoot() { return mRoot; } 

private:
    struct RenderConfig
    {
        int maxDepth;
    };
    RenderConfig mRenderConfig;

    std::unique_ptr<OctreeNode> mRoot;

    std::vector<Triangle> mTriangles;
    std::vector<glm::vec3> mVertices;

    void renderNode(const std::unique_ptr<OctreeNode> &node);
    void slowTriangleIntersectionTest(sdflib::BoundingBox bbox, const std::vector<size_t> &triangles, std::vector<size_t> &outTriangles);
};
