#pragma once

#include <string>
#include <cstdint>

class Cubemap  {
  public:
    struct Components {
        std::string right;
        std::string left;
        std::string top;
        std::string bottom;
        std::string front;
        std::string back;
    };
    Cubemap();
    Cubemap(const Components& faces, bool flip = false);
    ~Cubemap();

    void bind(uint32_t slot) const;
    void unbind() const;

  private:
    unsigned int mHandle;

    void load_face_path(const std::string& path, uint32_t face) const;
};