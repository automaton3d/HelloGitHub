/*
 * text_renderer.cpp
 */
//#pragma message("ft2build.h included from: " __FILE__)

#include <ft2build.h>
#include FT_FREETYPE_H
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "text_renderer.h"

bool TextRenderer::init(const std::string& fontPath, int fontSize, unsigned int shader)
{
	shaderID = shader;

	    FT_Library ft;
	    if (FT_Init_FreeType(&ft)) {
	        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library\n";
	        return false;
	    }

	    FT_Face face;
	    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
	        std::cerr << "ERROR::FREETYPE: Failed to load font '" << fontPath << "'\n";
	        FT_Done_FreeType(ft);
	        return false;
	    }

	    FT_Set_Pixel_Sizes(face, 0, fontSize);
	    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	    // Store global font metrics (once)
	    fontSizePx = fontSize;
	    float scale = static_cast<float>(fontSize) / static_cast<float>(face->units_per_EM);
	    ascenderPx  = face->ascender  * scale;
	    descenderPx = face->descender * scale;

	    // Carregar os glyphs
	    for (unsigned char c = 0; c < 128; ++c) {
	        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
	            std::cerr << "ERROR::FREETYPE: Failed to load glyph '" << c << "'\n";
	            continue;
	        }

	        unsigned int texture;
	        glGenTextures(1, &texture);
	        glBindTexture(GL_TEXTURE_2D, texture);
	        glTexImage2D(
	            GL_TEXTURE_2D,
	            0,
	            GL_RED,
	            face->glyph->bitmap.width,
	            face->glyph->bitmap.rows,
	            0,
	            GL_RED,
	            GL_UNSIGNED_BYTE,
	            face->glyph->bitmap.buffer
	        );

	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	        GLint swizzleMask[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
	        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

	        Character character = {
	            texture,
	            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
	            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
	            static_cast<unsigned int>(face->glyph->advance.x)
	        };
	        Characters.emplace(c, character);
	    }

	    FT_Done_Face(face);
	    FT_Done_FreeType(ft);

    // -------------------------------------------------------------
    //  VAO / VBO for rendering quads
    // -------------------------------------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void TextRenderer::RenderText(const std::string& text,
                              float x, float y,
                              float scale,
                              glm::vec3 color,
                              int screenWidth, int screenHeight)
{
    glm::mat4 projection = glm::ortho(0.0f,
                                      static_cast<float>(screenWidth),
                                      0.0f,
                                      static_cast<float>(screenHeight));

    glUseProgram(shaderID);
    glDisable(GL_DEPTH_TEST);  // No depth testing for text
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(shaderID, "textColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shaderID, "text"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    for (char c : text)
    {
        // Skip characters that weren't loaded
        auto it = Characters.find(c);
        if (it == Characters.end()) continue;
        const Character& ch = it->second;

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
        float w    = ch.Size.x * scale;
        float h    = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h, 0.0f, 0.0f },
            { xpos,     ypos,     0.0f, 1.0f },
            { xpos + w, ypos,     1.0f, 1.0f },

            { xpos,     ypos + h, 0.0f, 0.0f },
            { xpos + w, ypos,     1.0f, 1.0f },
            { xpos + w, ypos + h, 1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;   // advance cursor
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);  // ✅ Add this
    glUseProgram(0);  // ✅ Add this if not presen
}

void TextRenderer::RenderText(const std::string& text,
                              float x, float y,
                              float scale,
                              glm::vec3 color)
{
    RenderText(text, x, y, scale, color, screenW, screenH);
}

void TextRenderer::RenderText(const std::string& text, float x, float y)
{
    RenderText(text, x, y, 1.0f, glm::vec3(1.0f), screenW, screenH);
}
// ADDED: Implementation for the missing function
float TextRenderer::measureTextWidth(const std::string& text, float scale)
{
    float width = 0.0f;
    for (char c : text)
    {
        // Check if the character is loaded
        if (Characters.count(c) == 0)
        {
            // Handle error or skip character (skipping is common)
            continue;
        }

        const Character& ch = Characters.at(c);

        // The Advance value is stored in 1/64th of a pixel.
        // We use bit shift (>> 6) to divide by 64.0f
        width += (ch.Advance >> 6) * scale;
    }
    return width;
}
