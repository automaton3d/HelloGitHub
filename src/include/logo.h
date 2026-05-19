/*
 * logo.h
 */

#ifndef LOGO_H_
#define LOGO_H_

#include <glad/glad.h>
#include <string>


extern int textureMvpLoc;
extern unsigned int textureProgram2D;

namespace framework
{

  class Logo
  {
  public:
    explicit Logo(const std::string& path = "logo.png");
    ~Logo();

    Logo(const Logo&)            = delete;
    Logo& operator=(const Logo&) = delete;
    Logo(Logo&&)                 = default;
    Logo& operator=(Logo&&)      = default;

    // Draw in pixel coordinates (top-origin)
    void draw(int x, int y, float scale = 1.0f) const;

    int width()  const noexcept { return mWidth_;  }
    int height() const noexcept { return mHeight_; }
    bool valid() const noexcept { return mTexture_ != 0; }

  private:
    GLuint mTexture_ = 0;
    int    mWidth_   = 0;
    int    mHeight_  = 0;
  };
} // namespace framework

#endif /* LOGO_H_ */
