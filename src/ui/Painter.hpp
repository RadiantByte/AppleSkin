#pragma once

#include <appleskin/Native.hpp>
#include <array>

namespace appleskin {
class Painter {
public:
    explicit Painter(void* context);
    bool available() const;
    void start();
    void hunger(Vec2 position, float size, bool half, bool background = false);
    void saturation(Vec2 position, float size, float first, float last, float width = 1);
    void rectangle(const Rect& rectangle);
    void finish(Color color, float alpha);
private:
    void sprite(const Texture& source, Vec2 position, Vec2 size, Vec2 uv, Vec2 uvSize);
    void* context;
    Texture texture;
    Texture white;
    bool pending{};
};
}
