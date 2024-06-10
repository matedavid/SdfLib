#include <spdlog/spdlog.h>
#include <args.hxx>
#include <imgui.h>
#include <stack>
#include <filesystem>
#include <map>
#include <fstream>

#include "SdfLib/utils/Mesh.h"
#include "SdfLib/utils/PrimitivesFactory.h"
#include "render_engine/MainLoop.h"
#include "render_engine/NavigationCamera.h"
#include "render_engine/RenderMesh.h"
#include "render_engine/Window.h"

#include "render_engine/shaders/ColorsShader.h"
#include "SceneOctree.h"

using namespace sdflib;

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
    mesh.computeBoundingBox();

    std::map<std::pair<uint32_t, uint32_t>, int64_t> timeResults;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<int64_t>> timeResultsSamples;
    std::map<std::pair<uint32_t, uint32_t>, double> sizeResults;

    std::ofstream file("octree_results/octree_results_second.csv");

    std::vector<int32_t> maxDepthValues = {7, 8, 9, 10};
    std::vector<int32_t> startDepthValues = {0, 2, 4, 6};

    for (auto maxDepth : maxDepthValues) {
        for (auto startDepth : startDepthValues) {

            const auto config = SceneOctree::RenderConfig{
                .maxDepth = maxDepth,
                .startDepth = startDepth,
            };

            int64_t sumResults = 0;
            constexpr size_t NUM_SAMPLES = 3;
            timeResultsSamples.insert({{maxDepth, startDepth}, std::vector<int64_t>()});
            for (size_t i = 0; i < NUM_SAMPLES; ++i){
                auto begin = std::chrono::high_resolution_clock::now();
                {
                    SceneOctree scene(mesh, config);
                }
                auto end = std::chrono::high_resolution_clock::now();

                auto durationSample = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                sumResults += durationSample;

                timeResultsSamples[{maxDepth, startDepth}].push_back(durationSample);
            }

            int64_t duration = sumResults / NUM_SAMPLES;
            timeResults.insert({{maxDepth, startDepth}, duration});

            SceneOctree scene(mesh, config);
            auto size = scene.getShaderOctreeData().size() * sizeof(ShaderOctreeNode) * 1e-6;

            sizeResults.insert({{maxDepth, startDepth}, size});

            std::cout << "Finished maxDepth=" << maxDepth << ", " << "startDepth=" << startDepth << "\n";
        }
    }

    // Save results
    {
        // Time results
        for (auto maxDepth: maxDepthValues) 
        {
            file << ",";
            file << maxDepth;
        }
        file << "\n";

        for (auto startDepth : startDepthValues)
        {
            file << startDepth;

            for (auto maxDepth : maxDepthValues)
            {
                file << ",";
                file << timeResults[{maxDepth, startDepth}];
            }

            file << "\n";
        }

        file << "\n";

        // Size results
        for (auto maxDepth: maxDepthValues) 
        {
            file << ",";
            file << maxDepth;
        }
        file << "\n";

        for (auto startDepth : startDepthValues)
        {
            file << startDepth;

            for (auto maxDepth : maxDepthValues)
            {
                file << ",";
                file << sizeResults[{maxDepth, startDepth}];
            }

            file << "\n";
        }

        // Time sample results
        file << "\n\n";

        for (const auto& [key, values] : timeResultsSamples) {
            file << key.first << "," << key.second;
            for (const auto val : values) {
                file << "," << val;
            }
            file << "\n";
        }
    }
}