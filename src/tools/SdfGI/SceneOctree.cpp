#include "SceneOctree.h"

#include <numeric>
#include <iostream>
#include <stack>
#include <queue>

SceneOctree::SceneOctree(const sdflib::Mesh &mesh, RenderConfig config) : mRenderConfig(config)
{
    mVertices = mesh.getVertices();

    mTriangles = std::vector<Triangle>();
    for (size_t i = 0; i < mesh.getIndices().size(); i += 3)
    {
        const auto t = Triangle{
            .v0 = mesh.getIndices()[i],
            .v1 = mesh.getIndices()[i + 1],
            .v2 = mesh.getIndices()[i + 2],
        };

        mTriangles.push_back(t);
    }

    const auto bbox = mesh.getBoundingBox();
    const auto maxSize = std::max({bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y, bbox.max.z - bbox.min.z});

    const auto center = (bbox.min + bbox.max) / 2.0f;
    const auto halfSize = maxSize / 2.0f;

    std::vector<size_t> triangleIndices(mTriangles.size());
    std::iota(triangleIndices.begin(), triangleIndices.end(), 0);

    // assert(mTriangles.size() == mesh.getColorPerTriangle().size());
    assert(mTriangles.size() == mesh.getMaterialPerTriangle().size());

    mRoot = std::make_unique<OctreeNode>(OctreeNode{
        .center = center,
        .halfSize = halfSize,
        .depth = 0,
        .type = OctreeNode::Type::Gray,
        .triangles = triangleIndices,
    });

    renderNode(mRoot, mesh);

    // Create ShaderOctreeNode array
    {
        const auto& node = mRoot;
        assert(node->type == OctreeNode::Type::Gray);

        ShaderOctreeNode shaderNode{};
        shaderNode.min = node->center - glm::vec3(node->halfSize);
        shaderNode.max = node->center + glm::vec3(node->halfSize);
        // shaderNode.ref = node.get();

        mShaderOctreeData.push_back(shaderNode);

        const uint32_t firstChild = generateShaderOctreeData(mRoot);
        assert(firstChild == 1);

        mShaderOctreeData[0].setIndex(firstChild); 
    }

    // Check shader octree is correct
    /*
    {
        for (size_t i = 0; i < mShaderOctreeData.size(); ++i)
        {
            const auto& node = mShaderOctreeData[i];

            assert(node.ref->type != OctreeNode::Type::Black || node.getIsLeaf());
            assert(node.ref->type != OctreeNode::Type::White || node.getIsLeaf());

            if (node.getIsLeaf())
            {
                std::cout << "node idx: " << i << " is leaf\n";
                continue;
            }

            auto childIdx = node.getIndex();
            for (size_t j = 0; j < 8; ++j)
            {
                std::cout << "node idx: " << i << " children number: " << j << " position: " << (childIdx+j) << "\n";
                std::cout << "\t" << node.ref->children[j].get() << " " << mShaderOctreeData[childIdx+j].ref << "\n";
                assert(node.ref->children[j].get() == mShaderOctreeData[childIdx+j].ref);
            }
        }
    }
    */
}

void SceneOctree::renderNode(std::unique_ptr<OctreeNode> &node, const sdflib::Mesh &mesh)
{
    if (node->depth >= mRenderConfig.maxDepth)
    {
        node->type = OctreeNode::Type::Black;

        // Set node material
        {
            node->material = sdflib::MaterialProperties{};
            for (const auto &ti : node->triangles)
            {
                const auto mat =  mesh.getMaterialPerTriangle()[ti];
                node->material.albedo += mat.albedo;
                node->material.roughness += mat.roughness;
                node->material.metallic += mat.metallic;

            }

            // node->color /= static_cast<float>(node->triangles.size());

            node->material.albedo /= static_cast<float>(node->triangles.size());
            node->material.roughness /= static_cast<float>(node->triangles.size());
            node->material.metallic /= static_cast<float>(node->triangles.size());
        }

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

uint32_t SceneOctree::generateShaderOctreeData(const std::unique_ptr<OctreeNode> &node)
{
    const uint32_t nodeIdx = mShaderOctreeData.size();

    for (size_t i = 0; i < 8; ++i)
    {
        const auto& child = node->children[i];

        ShaderOctreeNode shaderNode{};
        shaderNode.min = child->center - glm::vec3(child->halfSize);
        shaderNode.max = child->center + glm::vec3(child->halfSize);
        // shaderNode.ref = child.get();

        mShaderOctreeData.push_back(shaderNode);
    }

    for (size_t i = 0; i < 8; ++i) 
    {
        const auto& child = node->children[i];

        if (child->type == OctreeNode::Type::Black)
        {
            const auto mat = child->material;
            mShaderOctreeData[nodeIdx+i].setIsLeaf();

            mShaderOctreeData[nodeIdx+i].color = glm::vec4(mat.albedo, 1.0f);
            mShaderOctreeData[nodeIdx+i].materialProperties = glm::vec4(mat.roughness, mat.metallic, 0.0f, 0.0f);
        }
        else if (child->type == OctreeNode::Type::White)
        {
            mShaderOctreeData[nodeIdx+i].setIsLeaf();

            mShaderOctreeData[nodeIdx+i].color = glm::vec4(0.0f);
            mShaderOctreeData[nodeIdx+i].materialProperties = glm::vec4(0.0f);
        }
        else if (child->type == OctreeNode::Type::Gray)
        {
            const uint32_t childIdx = generateShaderOctreeData(child);
            mShaderOctreeData[nodeIdx+i].setIndex(childIdx);

            assert(mShaderOctreeData[nodeIdx+i].getIndex() == childIdx);
            // assert(mShaderOctreeData[mShaderOctreeData[nodeIdx+i].getIndex()].ref == child->children[0].get());
        }
    }

    return nodeIdx;
}