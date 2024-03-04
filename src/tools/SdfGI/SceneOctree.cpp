#include "SceneOctree.h"

#include <numeric>
#include <iostream>
#include <stack>

SceneOctree::SceneOctree(const sdflib::Mesh &mesh, int maxDepth)
{
    mRenderConfig = {
        .maxDepth = maxDepth,
    };

    mVertices = mesh.getVertices();

    mTriangles = std::vector<Triangle>(mesh.getIndices().size());
    for (size_t i = 0; i < mesh.getIndices().size(); i += 3)
    {
        const auto t = Triangle{
            .v0 = mesh.getIndices()[i],
            .v1 = mesh.getIndices()[i + 1],
            .v2 = mesh.getIndices()[i + 2],
        };

        mTriangles[i] = t;
    }

    const auto bbox = mesh.getBoundingBox();
    const auto maxSize = std::max({bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y, bbox.max.z - bbox.min.z});

    const auto center = (bbox.min + bbox.max) / 2.0f;
    const auto halfSize = maxSize / 2.0f;

    std::vector<size_t> triangleIndices(mTriangles.size());
    std::iota(triangleIndices.begin(), triangleIndices.end(), 0);

    mRoot = std::make_unique<OctreeNode>(OctreeNode{
        .center = center,
        .halfSize = halfSize,
        .depth = 0,
        .type = OctreeNode::Type::Gray,
        .triangles = triangleIndices,
    });

    renderNode(mRoot, mesh);
}

void SceneOctree::renderNode(const std::unique_ptr<OctreeNode> &node, const sdflib::Mesh& mesh)
{
    if (node->depth >= mRenderConfig.maxDepth)
    {
        node->type = OctreeNode::Type::Black;
        return;
    }

    const auto nodeBBox = sdflib::BoundingBox{
        node->center - glm::vec3(node->halfSize),
        node->center + glm::vec3(node->halfSize),
    };

    // 1. Create children
    node->children = std::array<std::unique_ptr<OctreeNode>, 8>();

    // 2. For each child, check which triangles are inside the cube
    std::array<sdflib::BoundingBox, 8> childrenBoundingBoxes;
    {
        // -x -y -z
        childrenBoundingBoxes[0] = sdflib::BoundingBox{nodeBBox.min, node->center};
        // +x -y -z
        childrenBoundingBoxes[1] = sdflib::BoundingBox{
            glm::vec3(node->center.x, nodeBBox.min.y, nodeBBox.min.z),
            glm::vec3(nodeBBox.max.x, node->center.y, node->center.z)};
        // -x -y +z
        childrenBoundingBoxes[2] = sdflib::BoundingBox{
            glm::vec3(nodeBBox.min.x, nodeBBox.min.y, node->center.z),
            glm::vec3(node->center.x, node->center.y, nodeBBox.max.z)};
        // +x -y +z
        childrenBoundingBoxes[3] = sdflib::BoundingBox{
            glm::vec3(node->center.x, nodeBBox.min.y, node->center.z),
            glm::vec3(nodeBBox.max.x, node->center.y, nodeBBox.max.z)};
        // -x +y -z
        childrenBoundingBoxes[4] = sdflib::BoundingBox{
            glm::vec3(nodeBBox.min.x, node->center.y, nodeBBox.min.z),
            glm::vec3(node->center.x, nodeBBox.max.y, node->center.z)};
        // +x +y -z
        childrenBoundingBoxes[5] = sdflib::BoundingBox{
            glm::vec3(node->center.x, node->center.y, nodeBBox.min.z),
            glm::vec3(nodeBBox.max.x, nodeBBox.max.y, node->center.z)};
        // -x +y +z
        childrenBoundingBoxes[6] = sdflib::BoundingBox{
            glm::vec3(nodeBBox.min.x, node->center.y, node->center.z),
            glm::vec3(node->center.x, nodeBBox.max.y, nodeBBox.max.z)};
        // +x +y +z
        childrenBoundingBoxes[7] = sdflib::BoundingBox{node->center, nodeBBox.max};
    }

    for (size_t i = 0; i < 8; ++i)
    {
        const auto childBBox = childrenBoundingBoxes[i];

        node->children[i] = std::make_unique<OctreeNode>(OctreeNode{
            .center = (childBBox.min + childBBox.max) / 2.0f,
            .halfSize = ((childBBox.max - childBBox.min) / 2.0f).x,
            .depth = node->depth + 1,
            .type = OctreeNode::Type::White,
        });

        slowTriangleIntersectionTest(childBBox, node->triangles, node->children[i]->triangles);

        node->children[i]->type = node->children[i]->triangles.size() == 0
                                      ? OctreeNode::Type::White
                                      : OctreeNode::Type::Gray;

        if (node->children[i]->type == OctreeNode::Type::Gray)
            renderNode(node->children[i], mesh);
    }
}

void SceneOctree::slowTriangleIntersectionTest(sdflib::BoundingBox bbox, const std::vector<size_t> &triangles, std::vector<size_t> &outTriangles)
{
    const auto triangle_intersects = [&](size_t triangle, const sdflib::BoundingBox &bbox)
    {
        const auto t = mTriangles[triangle];

        const auto v0 = mVertices[t.v0];
        const auto v1 = mVertices[t.v1];
        const auto v2 = mVertices[t.v2];

        const auto triBBox = sdflib::BoundingBox{
            glm::min(glm::min(v0, v1), v2),
            glm::max(glm::max(v0, v1), v2),
        };

        if (triBBox.max.x < bbox.min.x || triBBox.min.x > bbox.max.x)
            return false;
        if (triBBox.max.y < bbox.min.y || triBBox.min.y > bbox.max.y)
            return false;
        if (triBBox.max.z < bbox.min.z || triBBox.min.z > bbox.max.z)
            return false;

        return true;
    };

    for (const auto triangle : triangles)
    {
        if (triangle_intersects(triangle, bbox))
            outTriangles.push_back(triangle);
    }
}

void SceneOctree::print() {
    std::stack<OctreeNode*> stack;
    stack.push(mRoot.get());

    while (!stack.empty()) {
        auto& node = stack.top();
        stack.pop();

        if (node->type == OctreeNode::Type::Black)
            std::cout << "Node: " << node->triangles.size() << " triangles, depth: " << node->depth << std::endl;

        for (const auto &child : node->children) {
            if (node->type == OctreeNode::Type::Gray) {
                stack.push(child.get());
            }
        }
    }
}