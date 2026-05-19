/*
 * radio.h – adjusted version (uses ProjectionManager)
 */

#ifndef RADIO_H_
#define RADIO_H_

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cmath>
#include "text_renderer.h"
#include "draw_utils.h"
#include "projection_manager.h"
#include "globals.h"

class Radio
{
private:
    bool selected_ = false;
    std::string label_;
    int x_ = 0, y_ = 0;

    // Font scale shared by all objects
    static float fontScale_;

    static constexpr float PI = 3.14159265359f;
    static constexpr float RADIO_RADIUS = 8.0f;

    // Shared colors for all radios
    static glm::vec3 outlineColor_;
    static glm::vec3 fillColor_;
    static glm::vec3 labelColor_;
    static glm::vec3 centerDotColor_;

    // Helper: current 2D projection matrix
    const glm::mat4& proj() const { return ProjectionManager::instance().get2DOrtho(); }

    // Generate circle vertices
    static std::vector<glm::vec2> makeCircle(int cx, int cy, float radius);

public:
    Radio() = default;
    Radio(int x, int y, std::string label)
        : label_(std::move(label)), x_(x), y_(y) {}

    // Global font scale
    static void setFontScale(float s) { fontScale_ = s; }

    // Static method to set colors
    static void setColors(const glm::vec3& outline = glm::vec3(1.0f),
                          const glm::vec3& fill = glm::vec3(0.0f, 0.7f, 1.0f),
                          const glm::vec3& label = glm::vec3(1.0f),
                          const glm::vec3& centerDot = glm::vec3(1.0f));

    // DRAW
    void draw(TextRenderer& renderer) const;
    void drawAt(TextRenderer& renderer, int xPos, int yPos) const;

    // INPUT – mouse in top-left coordinates
    bool contains(int mouseX, int mouseY) const;

    // STATE
    void setSelected(bool s) { selected_ = s; }
    bool isSelected() const  { return selected_; }
    void toggle()            { selected_ = !selected_; }

    const std::string& getLabel() const { return label_; }
    int getX() const { return x_; }
    int getY() const { return y_; }

    void setPosition(int x, int y) { x_ = x; y_ = y; }
    void setLabel(const std::string& l) { label_ = l; }

    // GROUP MANAGEMENT
    static void selectOne(std::vector<Radio>& group, size_t index);
    static int getSelectedIndex(const std::vector<Radio>& group);
};

#endif // RADIO_H_
