#include "radio.h"

// Static definitions - DARK THEME COLORS

glm::vec3 Radio::outlineColor_   = glm::vec3(0.45f, 0.45f, 0.52f);
glm::vec3 Radio::fillColor_      = glm::vec3(0.10f, 0.45f, 0.80f);
glm::vec3 Radio::labelColor_     = glm::vec3(0.95f, 0.95f, 0.98f);
glm::vec3 Radio::centerDotColor_ = glm::vec3(0.98f, 0.98f, 1.0f);
float Radio::fontScale_          = 0.35f;

std::vector<glm::vec2> Radio::makeCircle(int cx, int cy, float radius)
{
    std::vector<glm::vec2> verts;
    verts.reserve(64);
    for (int i = 0; i < 64; ++i)
    {
        float a = i / 64.0f * 2.0f * PI;
        verts.emplace_back(cx + radius * std::cos(a),
                           cy + radius * std::sin(a));
    }
    return verts;
}

void Radio::setColors(const glm::vec3& outline,
                      const glm::vec3& fill,
                      const glm::vec3& label,
                      const glm::vec3& centerDot)
{
    outlineColor_   = outline;
    fillColor_      = fill;
    labelColor_     = label;
    centerDotColor_ = centerDot;
}

void Radio::draw(TextRenderer& renderer) const {
    drawAt(renderer, x_, y_);
}

bool Radio::contains(int mouseX, int mouseY) const {
    float dx = mouseX - x_;
    float dy = mouseY - y_;
    float dist = std::sqrt(dx*dx + dy*dy);

    if (dist <= RADIO_RADIUS + 6.0f) return true;
    if (mouseX >= x_ && mouseX <= x_ + 120 &&
        std::abs(mouseY - y_) <= 12) return true;

    return false;
}

void Radio::drawAt(TextRenderer& renderer, int xPos, int yPos) const {
    const glm::mat4& P = proj();

    // Outer circle (border)
    auto outline = makeCircle(xPos, yPos, RADIO_RADIUS);
    drawLineLoop2D(outline, outlineColor_, P, 2.0f);

    // Inner fill when selected
    if (selected_) {
        auto inner = makeCircle(xPos, yPos, RADIO_RADIUS * 0.65f);
        std::vector<glm::vec2> fan;
        fan.reserve(inner.size() + 2);
        fan.emplace_back(xPos, yPos);
        fan.insert(fan.end(), inner.begin(), inner.end());
        fan.push_back(inner[0]);
        drawTriangleFan2D(fan, fillColor_, P);
    }

    // Central dot
    drawQuad2D(xPos - 1.5f, yPos - 1.5f,
               xPos + 1.5f, yPos + 1.5f,
               centerDotColor_, P);

    // Label
    int screenH = gViewport[3];
    float dynamicScale = fontScale_;
    float baselineY = screenH - yPos - RADIO_RADIUS * 0.3f;

    renderer.RenderText(label_,
                        xPos + 18,
                        baselineY,
                        dynamicScale,
                        labelColor_,
                        gViewport[2], screenH);
}