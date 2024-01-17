/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-01 09:13:43
 * @LastEditors: 李石
 * @LastEditTime: 2023-11-02 11:17:15
 */

#ifndef YUV_RENDERER_H
#define YUV_RENDERER_H

#include <iostream>

#include "GL/glew.h"
#include "GL/glut.h"

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4
// Rotate the texture
#define TEXTURE_ROTATE 0
// Show half of the Texture
#define TEXTURE_HALF 0

class YUVRenderer {
public:
    YUVRenderer(const int32_t width, const int32_t height, const int32_t x = -1,
                const int32_t y = -1, std::string title = "YUV Renderer");
    ~YUVRenderer();

    void setYUVData(const uint8_t *pData, int width, int height);
    void render();
    static bool getWindowSize(int32_t &width, int32_t &height);

private:
    bool bInit;
    int32_t gl_width;
    int32_t gl_height;
    uint8_t *pYUVData;
    GLuint program;
    GLuint id_y, id_u, id_v;  // Texture id
    GLuint textureY, textureU, textureV;

    void InitShaders();
    void initTextures();
};

#endif  // YUV_RENDERER_H
