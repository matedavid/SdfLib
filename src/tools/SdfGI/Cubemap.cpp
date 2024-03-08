#include "Cubemap.h"

#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>

Cubemap::Cubemap() {
    glGenTextures(1, &mHandle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mHandle);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

Cubemap::Cubemap(const Components& faces, bool flip) : Cubemap() {
    stbi_set_flip_vertically_on_load(flip);

    load_face_path(faces.right, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    load_face_path(faces.left, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
    load_face_path(faces.top, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
    load_face_path(faces.bottom, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
    load_face_path(faces.front, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
    load_face_path(faces.back, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

Cubemap::~Cubemap() {
    glDeleteTextures(1, &mHandle);
}

void Cubemap::bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mHandle);
}

void Cubemap::unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Cubemap::load_face_path(const std::string& path, uint32_t face) const {
    auto extension = std::filesystem::path(path).extension();

    int width, height, num_channels;
    auto* data = stbi_load(path.c_str(), &width, &height, &num_channels, 0);
    if (!data) {
        std::cout << "Failed to load skybox texture " << path << "\n";
        return;
    }

    glTexImage2D(face, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
}