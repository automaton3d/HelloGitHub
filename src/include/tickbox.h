/*
 * tickbox.h
 */

#ifndef TICKBOX_H_
#define TICKBOX_H_

#include <glad/glad.h>
#include <string>
#include <iostream>
#include <functional>
#include "text_renderer.h"
#include "draw_utils.h"
#include "projection_manager.h"
#include "globals.h"

class Tickbox
{
private:
    static constexpr int BOX_SIZE       = 18;
    static constexpr int LABEL_OFFSET_X = 26;
    static constexpr int LABEL_OFFSET_Y = 2;

    bool state_ = false;
    std::string label_;
    int x_ = 0, y_ = 0;           // top-left corner

    // Cores compartilhadas
    static glm::vec3 borderColor_;
    static glm::vec3 labelColor_;
    static glm::vec3 fillOn_;
    static glm::vec3 fillOff_;

    // Shared font scale
    static float fontScale_;

public:
    std::function<void(bool)> onToggle;

    Tickbox() = default;
    Tickbox(int x, int y, const std::string& label, bool initial = false)
        : state_(initial), label_(label), x_(x), y_(y) {}

    void setPosition(int x, int y) { x_ = x; y_ = y; }

    // Global scale
    static void setFontScale(float s) { fontScale_ = s; }

    // Cores globais
    static void setColors(const glm::vec3& border,
                          const glm::vec3& labelC,
                          const glm::vec3& fillOn,
                          const glm::vec3& fillOff);

    void draw(TextRenderer& renderer) const;
    bool contains(int mx, int my) const;
    void onClick(int mx, int my);
    void setState(bool s);
    bool getState() const;
    void toggle();

    int getX() const { return x_; }
    int getY() const { return y_; }
};

#endif // TICKBOX_H_
