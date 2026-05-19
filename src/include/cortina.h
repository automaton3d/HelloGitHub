// cortina.h
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <glad/glad.h>

// Forward declarations from globals.h
class TextRenderer;

class Cortina {
public:
    // Constructor
    Cortina(float x, float y, float width, float height,
             const std::vector<std::string>& options,
             int initialSelection = 0,
             std::function<void(int)> callback = nullptr);

    // Render the Cortina (must be called in the render loop)
    void render(TextRenderer* renderer);

    // Input handling (must be called in the mouse click callback)
    bool handleMouseClick(double xpos, double ypos);
    
    // Mouse wheel handling (scroll)
    void handleMouseScroll(double xpos, double ypos, double yoffset);

    // Get the selected option index
    int getSelectedIndex() const;
    void setSelectedIndex(int idx);
    
    // Get the selected option text
    std::string getSelectedText() const; 

    // State control methods
    void open() { isOpen = true; }
    void close() { isOpen = false; }
    void toggle() { isOpen = !isOpen; }
    bool isExpanded() const { return isOpen; }

    void scroll(int direction);
    void updateHover(int mx, int my);

private:
    // Position and dimensions (screen coordinates)
    float x, y, width, height;
    
    // Options list
    std::vector<std::string> options;
    
    // Cortina state
    bool isOpen = false;
    int selectedIndex;

    int scrollOffset = 0;
    const int maxVisibleItems = 6;
        

    // Mouse state variables for hover (bottom-up)
    int mouseX = 0; 
    int mouseY = 0;
    
    // Callback to be executed on selection
    std::function<void(int)> selectionCallback;

    // Appearance constants
    const glm::vec3 background_color = glm::vec3(0.1f, 0.1f, 0.1f);
    const glm::vec3 border_color     = glm::vec3(0.5f, 0.5f, 0.5f);
    const glm::vec3 hover_color      = glm::vec3(0.3f, 0.3f, 0.3f);
    const glm::vec3 text_color       = glm::vec3(1.0f, 1.0f, 1.0f);
    const float padding_x            = 20.0f;
    const float padding_y            = 15.0f;
    const float border_thickness     = 1.0f;
    const float text_scale           = 0.35f;

    void drawRect(float rx, float ry, float rwidth, float rheight, const glm::vec3& color);
    
    bool isMouseOver(double mx, double my, float rx, float ry, float rwidth, float rheight);
    
public:
    void updateMousePosition(double mx, double my) { mouseX = mx; mouseY = my; }
};