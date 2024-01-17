/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-01 11:41:14
 * @LastEditors: 李石
 * @LastEditTime: 2023-11-02 11:17:55
 */
/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-01 09:14:02
 * @LastEditors: 李石
 * @LastEditTime: 2023-11-01 10:57:34
 */
#include "YUVRenderer.h"

#include <X11/Xlib.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <vector>

// Helper function to compile shaders

bool YUVRenderer::getWindowSize(int32_t &width, int32_t &height) {
    Display *display = XOpenDisplay(NULL);  // 打开默认显示器
    if (!display) {
        std::cerr << "Unable to open X display" << std::endl;
        return false;
    }

    // 获取屏幕号
    int screen_num = DefaultScreen(display);

    // 获取屏幕宽度和高度
    width = DisplayWidth(display, screen_num);
    height = DisplayHeight(display, screen_num);
    XCloseDisplay(display);  // 关闭显示器
    return true;
}

// Init Shader
void YUVRenderer::InitShaders() {
    GLint vertCompiled, fragCompiled, linked;

    GLint v, f;
    const char *vs =
        "#version 330\n"  // 这里是 GLSL 1.20
                          // 的版本声明，你可以根据需要调整版本号
        "attribute vec4 vertexIn;\n"
        "attribute vec2 textureIn;\n"
        "varying vec2 textureOut;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vertexIn;\n"
        "    textureOut = textureIn;\n"
        "}\n";

    const char *fs =
        "#version 330\n"  // 这里是 GLSL 1.20
                          // 的版本声明，你可以根据需要调整版本号
        "varying vec2 textureOut;\n"
        "uniform sampler2D tex_y;\n"
        "uniform sampler2D tex_u;\n"
        "uniform sampler2D tex_v;\n"
        "void main(void)\n"
        "{\n"
        "    vec3 yuv;\n"
        "    vec3 rgb;\n"
        "    yuv.x = texture2D(tex_y, textureOut).r;\n"
        "    yuv.y = texture2D(tex_u, textureOut).r - 0.5;\n"
        "    yuv.z = texture2D(tex_v, textureOut).r - 0.5;\n"
        "    rgb = mat3( 1,       1,         1,\n"
        "                0,       -0.39465,  2.03211,\n"
        "                1.13983, -0.58060,  0) * yuv;\n"
        "    gl_FragColor = vec4(rgb, 1);\n"
        "}\n";

    // Shader: step1
    v = glCreateShader(GL_VERTEX_SHADER);
    f = glCreateShader(GL_FRAGMENT_SHADER);
    // Get source code
    // Shader: step2
    glShaderSource(v, 1, &vs, NULL);
    glShaderSource(f, 1, &fs, NULL);
    // Shader: step3
    glCompileShader(v);
    // Debug
    glGetShaderiv(v, GL_COMPILE_STATUS, &vertCompiled);
    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &fragCompiled);

    // Program: Step1
    program = glCreateProgram();
    // Program: Step2
    glAttachShader(program, v);
    glAttachShader(program, f);

    glBindAttribLocation(program, ATTRIB_VERTEX, "vertexIn");
    glBindAttribLocation(program, ATTRIB_TEXTURE, "textureIn");
    // Program: Step3
    glLinkProgram(program);
    // Debug
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    // Program: Step4
    glUseProgram(program);
}

void YUVRenderer::initTextures() {
    // Get Uniform Variables Location
    textureY = glGetUniformLocation(program, "tex_y");
    textureU = glGetUniformLocation(program, "tex_u");
    textureV = glGetUniformLocation(program, "tex_v");

#if TEXTURE_ROTATE
    static const GLfloat vertexVertices[] = {
        -1.0f, -0.5f, 0.5f, -1.0f, -0.5f, 1.0f, 1.0f, 0.5f,
    };
#else
    static const GLfloat vertexVertices[] = {
        -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
    };
#endif

#if TEXTURE_HALF
    static const GLfloat textureVertices[] = {
        0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f,
    };
#else
    static const GLfloat textureVertices[] = {
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };
#endif
    // Set Arrays
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertexVertices);
    // Enable it
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    // Init Texture
    glGenTextures(1, &id_y);
    glBindTexture(GL_TEXTURE_2D, id_y);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &id_u);
    glBindTexture(GL_TEXTURE_2D, id_u);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &id_v);
    glBindTexture(GL_TEXTURE_2D, id_v);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

YUVRenderer::YUVRenderer(const int32_t width, const int32_t height,
                         const int32_t x, const int32_t y, std::string title) {
    bInit = false;
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid width or height!" << std::endl;
        return;
    }
    int32_t xpos = x;
    int32_t ypos = y;
    int32_t screen_w = 0;
    int32_t screen_h = 0;
    int32_t width_win = width;
    int32_t height_win = height;
    gl_width = 0;
    gl_height = 0;

    if (!getWindowSize(screen_w, screen_h)) {
        screen_w = 1920;
        screen_h = 1080;
        std::cerr << "getWindowSize failed!" << std::endl;
    }
    if (width > screen_w || height > screen_h) {
        width_win = screen_w;
        height_win = screen_h;
    } else {
        width_win = width;
        height_win = height;
    }
    if (x < 0 || y < 0) {
        xpos = (screen_w - width_win) / 2;
        ypos = (screen_h - height_win) / 2;
    } else {
        xpos = x;
        ypos = y;
    }
    int argc = 1;
    char argv[1][16] = {0};
    strcpy(argv[0], "YUV Renderer");
    char *pargv[1] = {argv[0]};

    glutInit(&argc, pargv);
    // GLUT_DOUBLE
    glutInitDisplayMode(GLUT_DOUBLE |
                        GLUT_RGBA /*| GLUT_STENCIL | GLUT_DEPTH*/);
    glutInitWindowPosition(xpos, ypos);
    glutInitWindowSize(width_win, height_win);
    if (title.empty())
        glutCreateWindow("YUV Display");
    else
        glutCreateWindow(title.c_str());
    GLenum glewState = glewInit();
    if (GLEW_OK != glewState) {
        std::cerr << "glewInit failed!" << std::endl;
        return;
    }
    InitShaders();
    initTextures();
    pYUVData = nullptr;
    bInit = true;
    const uint8_t *glVersion = glGetString(GL_VERSION);
    std::cout << "opengl(" << glVersion << ") init success!" << std::endl;
}

YUVRenderer::~YUVRenderer() {
    if (!bInit) return;
    glDeleteTextures(1, &textureY);
    glDeleteTextures(1, &textureU);
    glDeleteTextures(1, &textureV);

    glDeleteProgram(program);
    if (pYUVData != nullptr) {
        delete[] pYUVData;
        pYUVData = nullptr;
    }
    bInit = false;
}

void YUVRenderer::setYUVData(const uint8_t *pData, int32_t pixel_w,
                             int32_t pixel_h) {
    // auto start = std::chrono::steady_clock::now();
    if (!bInit) return;
    if (pixel_w <= 0 || pixel_h <= 0) {
        std::cerr << "Invalid width or height!" << std::endl;
        return;
    }

    if (pYUVData == nullptr) {
        gl_width = pixel_w;
        gl_height = pixel_h;
        pYUVData = new uint8_t[pixel_w * pixel_h * 3 / 2];
    } else {
        if (pixel_w != gl_width || pixel_h != gl_height) {
            gl_width = pixel_w;
            gl_height = pixel_h;
            if (pYUVData != nullptr) {
                delete[] pYUVData;
                pYUVData = nullptr;
            }
            pYUVData = new uint8_t[pixel_w * pixel_h * 3 / 2];
        }
    }
    if (pYUVData == nullptr) {
        std::cerr << "new pYUVData failed!" << std::endl;
        return;
    }
    memcpy(pYUVData, pData, pixel_w * pixel_h * 3 / 2);
    // auto end = std::chrono::steady_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end
    // - start).count(); std::cout << "setYUVData cost " << duration << " ms" <<
    // std::endl;
}

void YUVRenderer::render() {
    // auto start = std::chrono::steady_clock::now();
    const unsigned char *pY, *pU, *pV;
    pY = pYUVData;
    pU = pY + gl_width * gl_height;
    pV = pU + gl_width * gl_height / 4;

    // Clear
    glClearColor(0.0, 255, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    // Y
    //
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, id_y);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gl_width, gl_height, 0, GL_RED,
                 GL_UNSIGNED_BYTE, pY);

    glUniform1i(textureY, 0);
    // U
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, id_u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gl_width / 2, gl_height / 2, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pU);
    glUniform1i(textureU, 1);
    // V
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, id_v);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gl_width / 2, gl_height / 2, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pV);
    glUniform1i(textureV, 2);

    // Draw
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // Show
    // Double
    glutSwapBuffers();
    // Single
    // auto end = std::chrono::steady_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end
    // - start).count(); std::cout << "uploadYUVData cost " << duration << " ms"
    // << std::endl;
}