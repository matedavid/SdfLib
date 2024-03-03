#include <spdlog/spdlog.h>
#include <args.hxx>
#include <imgui.h>
#include <stack>
#include <filesystem>

#include "SdfLib/utils/Mesh.h"
#include "SdfLib/utils/PrimitivesFactory.h"
#include "render_engine/MainLoop.h"
#include "render_engine/NavigationCamera.h"
#include "render_engine/RenderMesh.h"
#include "render_engine/Window.h"

#include "render_engine/shaders/ColorsShader.h"
#include "SceneOctree.h"

using namespace sdflib;

class OctreeViewer : public Scene
{
public:
    OctreeViewer(std::shared_ptr<SceneOctree> octree) : mOctree(octree) {}

    void start() override
    {
        std::stack<OctreeNode *> nodes;
        nodes.push(mOctree->getRoot().get());

        while (!nodes.empty())
        {
            auto node = nodes.top();
            nodes.pop();

            if (node->type == OctreeNode::Type::Black)
            {
                mCubes.push_back({node->center, node->halfSize});
            }
            else if (node->type == OctreeNode::Type::Gray)
            {
                for (const auto &child : node->children)
                {
                    if (child)
                        nodes.push(child.get());
                }
            }
        }

        auto camera = std::make_shared<NavigationCamera>();
        camera->callDrawGui = false;

        camera->start();
        setMainCamera(camera);
        addSystem(camera);
        mCamera = camera;

        mShader = std::make_unique<ColorsShader>();

        auto mesh = PrimitivesFactory::getCube();
        mCubeRenderer = std::make_shared<RenderMesh>();
        mCubeRenderer->start();
        mCubeRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                         RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                     mesh->getVertices().data(), mesh->getVertices().size());

        mCubeRenderer->setIndexData(mesh->getIndices());
        mCubeRenderer->setShader(mShader.get());
        mCubeRenderer->callDraw = false; // Disable the automatic call because we already call the function
        mCubeRenderer->callDrawGui = false;
        mCubeRenderer->systemName = "Cube";
        addSystem(mCubeRenderer);
    }

    void update(float deltaTime) override
    {
        Scene::update(deltaTime);
    }

    void draw() override
    {
        auto loc = glGetUniformLocation(mShader->getProgramId(), "color");

        for (const auto &[center, halfSize] : mCubes)
        {
            glUniform3f(loc, 1.0f, 1.0f, 1.0f);

            const auto size = halfSize* 2.0f;

            auto transform = glm::mat4(1.0f);
            transform = glm::translate(transform, center);
            transform = glm::scale(transform, glm::vec3(size * 0.5));

            mCubeRenderer->setTransform(transform);
            mCubeRenderer->draw(getMainCamera());
        }
    }

private:
    std::shared_ptr<SceneOctree> mOctree;
    std::vector<std::pair<glm::vec3, float>> mCubes;

    std::shared_ptr<NavigationCamera> mCamera;
    std::shared_ptr<RenderMesh> mCubeRenderer;
    std::unique_ptr<ColorsShader> mShader;
};

int main(int argc, char **argv)
{
    args::ArgumentParser parser("SdfLight app for rendering a model using its sdf");
    args::HelpFlag help(parser, "help", "Display help menu", {'h', "help"});
    args::Positional<std::string> modelPathArg(parser, "model_path", "The model path");
    args::Flag normalizeBBArg(parser, "sdf_is_normalized", "Indicates that the sdf model is normalized", {'n', "normalize"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
        std::cerr << parser;
        return 0;
    }

    Mesh mesh(args::get(modelPathArg));

    if (args::get(normalizeBBArg))
    {
        // Normalize model units
        const glm::vec3 boxSize = mesh.getBoundingBox().getSize();
        glm::vec3 center = mesh.getBoundingBox().getCenter();
        mesh.applyTransform(glm::scale(glm::mat4(1.0), glm::vec3(2.0f / glm::max(glm::max(boxSize.x, boxSize.y), boxSize.z))) *
                            glm::translate(glm::mat4(1.0), -mesh.getBoundingBox().getCenter()));
    }

    auto scene = std::make_shared<SceneOctree>(mesh, 7);
    OctreeViewer viewer(scene);

    MainLoop loop;
    loop.start(viewer, "SdfGIOctreeViewer");
}