#include "MitsubaExporter.h"

#include <fstream>
#include <glm/gtc/matrix_access.hpp>

void MitsubaExporter::addEmitter(const Emitter& emitter)
{
    mEmitters.push_back(emitter);
}

void MitsubaExporter::addModel(const Model& model)
{
    mModels.push_back(model);
}

void MitsubaExporter::setCamera(const Camera& camera)
{
    mCamera = camera;
}

void MitsubaExporter::save(const std::filesystem::path& path) const
{
    std::ofstream f(path, std::ios::out);
    f << "<scene version='3.0.0'>";

    f << R"(
        <default name="spp" value="512"/>
        <default name="width" value="1920"/>
        <default name="height" value="1080"/>
        <default name="max_depth" value="100"/>
        <default name="integrator" value="path"/>

        <integrator type='$integrator'>
            <integer name="max_depth" value="$max_depth"/>
        </integrator>
    )";

    // Camera
    const auto pos = mCamera.getPosition();

    const auto viewMatrix = mCamera.getViewMatrix();
    auto viewDirection = glm::vec3(-viewMatrix[0][2], -viewMatrix[1][2], -viewMatrix[2][2]);
    viewDirection = glm::normalize(viewDirection);

    const auto lookAt = pos + viewDirection * mCamera.getZNear();

    f << "<sensor type='perspective' id='sensor'>";
    f << "<float name='fov' value='" << mCamera.getFov() << "' />";
    f << "<transform name='to_world'>";
    f << "<lookat target='" << lookAt.x << ", " << lookAt.y << ", " << lookAt.z << "' origin='" << pos.x << ", " << pos.y << ", " << pos.z << "' up='0,1,0' />";
    f << "</transform>";
    f << R"(
        <sampler type="independent">
            <integer name="sample_count" value="$spp"/>
        </sampler>
        <film type="hdrfilm">
            <rfilter type="box"/>
            <integer name="width"  value="$width"/>
            <integer name="height" value="$height"/>
        </film>
    )";
    f << "</sensor>";

    // Models
    for (const auto& m : mModels)
    {
        f << "<shape type='obj'>";
        f << "<string name='filename' value='" << m.path.string() << "'/>";
        f << "<transform name='to_world'>";
        f << "<translate x='"<< m.translate.x <<"' y='"<< m.translate.y <<"' z='" << m.translate.z << "' />";
        f << "<scale value='"<< m.scale.x <<"' />";
        f << "</transform>";
        f << R"(
            <bsdf type="diffuse">
                <rgb name="reflectance" value="0.18, 0.18, 0.18"/>
            </bsdf>
        )";
        f << "</shape>";
    }

    // Emitters 
    for (const auto& e : mEmitters)
    {
        f << "<shape type='sphere'>";
        f << "<point name='center' x='"<< e.pos.x <<"' y='"<< e.pos.y <<"' z='"<< e.pos.z <<"' />";
        f << "<float name='radius' value='" << e.radius << "' />";
        f << "<emitter type='area'>";
        f << "<rgb name='radiance' value='"<< e.intensity <<"' />";
        f << "</emitter>";
        f << "</shape>";
        // f << "<emitter type='point'>";
        // f << "<point name='position' x='"<< e.pos.x <<"' y='"<< e.pos.y <<"' z='"<< e.pos.z <<"' />";
        // f << "<rgb name='intensity' value='"<<e.intensity<<"' />";
        // f << "</emitter>";

    }

    f << "</scene>";
}


