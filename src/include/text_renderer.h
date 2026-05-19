// text_renderer.h

#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <map>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct Character
{
  unsigned int TextureID;
  glm::ivec2 Size;
  glm::ivec2 Bearing;
  unsigned int Advance; // Distance to advance to next glyph
};

// text_renderer.h
class TextRenderer {
public:
    bool init(const std::string& fontPath, int fontSize, unsigned int shader);

    // Full version
    void RenderText(const std::string& text,
                    float x, float y,
                    float scale,
                    glm::vec3 color,
                    int screenWidth, int screenHeight);

    // Convenience version using current window size
    void RenderText(const std::string& text,
                    float x, float y,
                    float scale,
                    glm::vec3 color);

    // Optional: simplest version with scale = 1.0 and white color
    void RenderText(const std::string& text, float x, float y);

    float measureTextWidth(const std::string& text, float scale = 1.0f);
    ~TextRenderer() = default;

    // For the simple version to work, we need to know the window size
    void setScreenSize(int width, int height) { screenW = width; screenH = height; }
    float getAscenderPx() const { return ascenderPx; }
    float getDescenderPx() const { return descenderPx; }
    int   getFontSizePx() const { return fontSizePx; }

private:
    std::map<char, Character> Characters;
    unsigned int VAO = 0, VBO = 0;
    unsigned int shaderID = 0;

    // Current window size (used by simplified versions)
    int screenW = 800, screenH = 600;
    float ascenderPx = 0.0f;
    float descenderPx = 0.0f;
    int fontSizePx = 0;
};

#endif // TEXT_RENDERER_H
