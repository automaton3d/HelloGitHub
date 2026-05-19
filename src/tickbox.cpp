#include <iostream>
#include "tickbox.h"

const int BOX_SIZE = 18;

// Strong colors for dark/blue background
// Static variable definitions - DARK THEME
glm::vec3 Tickbox::borderColor_  = glm::vec3(0.45f, 0.45f, 0.52f);
glm::vec3 Tickbox::labelColor_   = glm::vec3(0.95f, 0.95f, 0.98f);
glm::vec3 Tickbox::fillOn_       = glm::vec3(0.10f, 0.45f, 0.80f);
glm::vec3 Tickbox::fillOff_      = glm::vec3(0.09f, 0.09f, 0.14f);
float Tickbox::fontScale_        = 0.35f;

void Tickbox::setColors(const glm::vec3& border,
                        const glm::vec3& labelC,
                        const glm::vec3& fillOn,
                        const glm::vec3& fillOff)
{
    borderColor_ = border;
    labelColor_  = labelC;
    fillOn_      = fillOn;
    fillOff_     = fillOff;
}

void Tickbox::draw(TextRenderer& renderer) const {
    const glm::mat4 P = ProjectionManager::instance().get2DOrtho();

    int screenW = gViewport[2];
    int screenH = gViewport[3];
    float dynamicScale = fontScale_;

    float boxY = y_;

    // Box background
    drawQuad2D(x_, boxY, x_ + BOX_SIZE, boxY + BOX_SIZE,
               state_ ? fillOn_ : fillOff_, P);

    // Border
    std::vector<glm::vec2> border = {
        {x_, boxY},
        {x_ + BOX_SIZE, boxY},
        {x_ + BOX_SIZE, boxY + BOX_SIZE},
        {x_, boxY + BOX_SIZE}
    };
    drawLineLoop2D(border, borderColor_, P, 2.0f);

    // Check mark (bold white)
    if (state_) {
        float m = 4.0f;
        std::vector<glm::vec2> check = {
            {x_ + m, boxY + BOX_SIZE * 0.5f},
            {x_ + BOX_SIZE * 0.5f, boxY + BOX_SIZE - m},
            {x_ + BOX_SIZE - m, boxY + m}
        };
        drawLineLoop2D(check, glm::vec3(0.98f, 0.98f, 1.0f), P, 4.0f);   // ← 4.0f em vez de 3.5f
    }

    // Texto
    float boxCenterY = y_ + BOX_SIZE * 0.5f;
    float textBaseline = screenH - boxCenterY - 5.0f;

    renderer.RenderText(label_,
                        x_ + BOX_SIZE + 8,
                        textBaseline,
                        dynamicScale,
                        labelColor_,
                        screenW, screenH);
}

bool Tickbox::contains(int mx, int my) const {
    return mx >= x_ && mx <= x_ + BOX_SIZE &&
           my >= y_ && my <= y_ + BOX_SIZE;
}

void Tickbox::onClick(int mx, int my) {
    if (contains(mx, my)) {
        state_ = !state_;
        if (onToggle) onToggle(state_);
    }
}

void Tickbox::setState(bool s) {
    if (state_ != s) {
        state_ = s;
        if (onToggle) onToggle(s);
    }
}

bool Tickbox::getState() const { return state_; }
void Tickbox::toggle() { setState(!state_); }