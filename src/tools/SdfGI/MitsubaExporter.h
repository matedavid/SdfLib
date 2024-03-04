#pragma once

#include <filesystem>
#include <vector>

#include "render_engine/Camera.h"

class MitsubaExporter
{
public:
    struct Emitter
    {
        glm::vec3 pos;
        float intensity;
        glm::vec3 color;
        float radius;
    };

    struct Model
    {
        std::filesystem::path path;
        glm::vec3 translate;
        glm::vec3 scale;
    };

    MitsubaExporter() = default;
    ~MitsubaExporter() = default;

    void addEmitter(const Emitter &emitter);
    void addModel(const Model &model);
    void setCamera(const Camera &camera);

    void save(const std::filesystem::path &path) const;

private:
    std::vector<Emitter> mEmitters;
    std::vector<Model> mModels;
    Camera mCamera;
};
