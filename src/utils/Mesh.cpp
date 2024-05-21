#include "SdfLib/utils/Mesh.h"
#include <iostream>
#include <assert.h>
#include <spdlog/spdlog.h>
#include <filesystem>

#include "stb/stb_image.h"

struct TextureInfo {
    stbi_uc* data;
    int width;
    int height;
    int channels;
};

namespace sdflib
{
#ifdef SDFLIB_ASSIMP_AVAILABLE
Mesh::Mesh(std::string filePath)
{
    Assimp::Importer import;
    const aiScene *scene = import.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes);
    
    if(!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        SPDLOG_ERROR("Error with Assimp: {}", import.GetErrorString());
        return;
    }

    if(!scene->HasMeshes())
    {
        SPDLOG_ERROR("The model {} does not have any mesh assosiated", filePath);
    }

    SPDLOG_INFO("Num meshes: {}", scene->mNumMeshes);
    // initMesh(scene->mMeshes[0]);

    std::unordered_map<std::string, TextureInfo> texture_cache;

    for (std::size_t m = 0; m < scene->mNumMeshes; m++)
    {
        const uint32_t vertexStartPos = mVertices.size();

        const auto* mesh = scene->mMeshes[m];
        if(!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
            SPDLOG_ERROR("The model must be a triangle mesh");

        if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
            continue;

        for (std::size_t v = 0; v < mesh->mNumVertices; ++v) 
        {
            mVertices.push_back(glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z));

            if (mesh->HasNormals())
                mNormals.push_back(glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z));
        }

        for (std::size_t f = 0; f < mesh->mNumFaces; ++f) 
        {
            const auto& face = mesh->mFaces[f];
            assert(face.mNumIndices == 3);
            mIndices.push_back(face.mIndices[0]+vertexStartPos);
            mIndices.push_back(face.mIndices[1]+vertexStartPos);
            mIndices.push_back(face.mIndices[2]+vertexStartPos);
        }

        // material
        if (mesh->mMaterialIndex < 0) 
        {
            for (std::size_t f = 0; f < mesh->mNumFaces; ++f) 
            {
                mMaterialPropertiesPerTriangle.push_back(MaterialProperties{});
            }

            continue;
        }

        const auto material = scene->mMaterials[mesh->mMaterialIndex];

        MaterialProperties props{};
        props.metallic = 0.5f;
        props.roughness = 0.5f;

        for (std::size_t f = 0; f < mesh->mNumFaces; ++f) 
        {
            const auto& face = mesh->mFaces[f];
            assert(face.mNumIndices == 3);

            const auto v0 = face.mIndices[0];
            const auto v1 = face.mIndices[1];
            const auto v2 = face.mIndices[2];

            glm::vec2 uv0, uv1, uv2;
            if (mesh->HasTextureCoords(0)) 
            {
                uv0 = {mesh->mTextureCoords[0][v0].x, 1.0 - mesh->mTextureCoords[0][v0].y};
                uv1 = {mesh->mTextureCoords[0][v1].x, 1.0 - mesh->mTextureCoords[0][v1].y};
                uv2 = {mesh->mTextureCoords[0][v2].x, 1.0 - mesh->mTextureCoords[0][v2].y};
            }

            aiColor3D aiColor;
            if (material->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == aiReturn_SUCCESS)
                props.albedo = glm::vec3(aiColor.r, aiColor.g, aiColor.b);

            ai_real metallic;
            if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) 
                props.metallic = metallic;

            ai_real roughness;
            if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) 
                props.roughness = roughness;

            aiString texturePath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) 
            {
                auto path = std::string{texturePath.C_Str()};
                auto pp = path.find('\\');
                if (pp != std::string::npos) {
                    path = path.replace(pp, 1, "/");
                }

                auto texturePath = std::filesystem::path(filePath).parent_path() / path;

                int width, height, channels;
                stbi_uc* pixels;

                auto it = texture_cache.find(texturePath.string());
                if (it != texture_cache.end()) 
                {
                    pixels = it->second.data;
                    width = it->second.width;
                    height = it->second.height;
                    channels = it->second.channels;
                }
                else 
                {
                    pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
                    assert(pixels != nullptr && "Could not open texture");

                    texture_cache.insert({texturePath.string(), TextureInfo{
                        .data = pixels,
                        .width = width,
                        .height = height,
                        .channels = channels,
                    }});
                }

                if (mesh->HasTextureCoords(0)) 
                {
                    auto center = (uv0 + uv1 + uv2) / 3.0f;
                    center.x *= (width-1);
                    center.y *= (height-1);

                    int pos = int(center.y) * width * 4 + int(center.x) * 4;

                    if (pos < width*height*4) 
                    {
                        glm::vec3 color = { pixels[pos+0], pixels[pos+1], pixels[pos+2] };
                        props.albedo = glm::vec3(color) / 255.0f;
                    } 
                    else 
                    {
                        std::cout << "Texture coords not correct:\n";
                        std::cout << "\t" << uv0.x << " " << uv0.y << "\n";
                        std::cout << "\t" << uv1.x << " " << uv1.y << "\n";
                        std::cout << "\t" << uv2.x << " " << uv2.y << "\n";
                        std::cout << "\n";
                    }
                }
            }

            props.metallic = glm::clamp(props.metallic, 0.1f, 1.0f);
            props.roughness = glm::clamp(props.roughness, 0.1f, 1.0f);

            mMaterialPropertiesPerTriangle.push_back(props);
        }
    }

    // Calculate bounding box
    computeBoundingBox();
    SPDLOG_INFO("BB min: {}, {}, {}", mBBox.min.x, mBBox.min.y, mBBox.min.z);
    SPDLOG_INFO("BB max: {}, {}, {}", mBBox.max.x, mBBox.max.y, mBBox.max.z);

    // Indices
    SPDLOG_INFO("Model num faces: {}", mIndices.size() / 3);

    // Clean texture cache
    for (const auto& info : texture_cache) {
        stbi_image_free(info.second.data);
    }
}

Mesh::Mesh(const aiMesh* mesh)
{
    initMesh(mesh);
}
#endif

Mesh::Mesh(glm::vec3* vertices, uint32_t numVertices,
         uint32_t* indices, uint32_t numIndices)
{
    mVertices.resize(numVertices);
    std::memcpy(mVertices.data(), vertices, sizeof(glm::vec3) * numVertices);

    mIndices.resize(numIndices);
    std::memcpy(mIndices.data(), indices, sizeof(uint32_t) * numIndices);
}

#ifdef SDFLIB_ASSIMP_AVAILABLE
void Mesh::initMesh(const aiMesh* mesh)
{
    if(!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
    {
        SPDLOG_ERROR("The model must be a triangle mesh");
    }

    // Copy vertices
    SPDLOG_INFO("Model num vertices: {}", mesh->mNumVertices);
    mVertices.resize(mesh->mNumVertices);
    const size_t vertexSize = sizeof(decltype(mVertices[0]));
    assert(vertexSize == sizeof(decltype(mesh->mVertices[0])));
    std::memcpy(mVertices.data(), mesh->mVertices, mesh->mNumVertices * vertexSize);

    // Calculate bounding box
    computeBoundingBox();
    SPDLOG_INFO("BB min: {}, {}, {}", mBBox.min.x, mBBox.min.y, mBBox.min.z);
    SPDLOG_INFO("BB max: {}, {}, {}", mBBox.max.x, mBBox.max.y, mBBox.max.z);

    // Copy indices
    SPDLOG_INFO("Model num faces: {}", mesh->mNumFaces);
    mIndices.resize(3 * mesh->mNumFaces);

    for(size_t f=0, i=0; f < mesh->mNumFaces; f++, i += 3)
    {
        mIndices[i] = mesh->mFaces[f].mIndices[0];
        mIndices[i + 1] = mesh->mFaces[f].mIndices[1];
        mIndices[i + 2] = mesh->mFaces[f].mIndices[2];
    }

    // Copy normals
	if (mesh->HasNormals())
	{
		mNormals.resize(mesh->mNumVertices);
		const size_t normalSize = sizeof(decltype(mNormals[0]));
		assert(normalSize == sizeof(decltype(mesh->mNormals[0])));
		std::memcpy(mNormals.data(), mesh->mNormals, mesh->mNumVertices * normalSize);
	}
	else
	{
		computeNormals();
	}
}
#endif

void Mesh::computeBoundingBox()
{
    glm::vec3 min(INFINITY);
    glm::vec3 max(-INFINITY);
    for(glm::vec3& vert : mVertices)
    {
        min.x = glm::min(min.x, vert.x);
        max.x = glm::max(max.x, vert.x);

        min.y = glm::min(min.y, vert.y);
        max.y = glm::max(max.y, vert.y);

        min.z = glm::min(min.z, vert.z);
        max.z = glm::max(max.z, vert.z);
    }
    mBBox = BoundingBox(min, max);
}

void Mesh::computeNormals()
{
    mNormals.clear();
    mNormals.assign(mVertices.size(), glm::vec3(0.0f));

    for (int i = 0; i < mIndices.size(); i += 3)
    {
        const glm::vec3 v1 = mVertices[mIndices[i]];
        const glm::vec3 v2 = mVertices[mIndices[i + 1]];
        const glm::vec3 v3 = mVertices[mIndices[i + 2]];
        const glm::vec3 normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));

        mNormals[mIndices[i]] += glm::acos(glm::dot(glm::normalize(v2 - v1), glm::normalize(v3 - v1))) * normal;
        mNormals[mIndices[i + 1]] += glm::acos(glm::dot(glm::normalize(v1 - v2), glm::normalize(v3 - v2))) * normal;
        mNormals[mIndices[i + 2]] += glm::acos(glm::dot(glm::normalize(v1 - v3), glm::normalize(v2 - v3))) * normal;
    }

    for(glm::vec3& n : mNormals)
    {
        n = glm::normalize(n);
    }
}

void Mesh::applyTransform(glm::mat4 trans)
{
    for(glm::vec3& vert : mVertices)
    {
        vert = glm::vec3(trans * glm::vec4(vert, 1.0));
    }

    computeBoundingBox();
}
}
