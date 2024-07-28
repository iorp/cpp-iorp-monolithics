// render tiff 
// Note there is a problem with the aspect ratio, but otherwise works fine.
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <tiffio.h>
#include <cstdint> // For uint32_t
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
    Camera()
        : scale(1.0f), offset(0.0f, 0.0f) {}

    void processKeyboardInput(GLFWwindow* window) {
        float cameraSpeed = 0.01f;  // Adjusted sensitivity
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            offset.y += cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            offset.y -= cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            offset.x -= cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            offset.x += cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            scale *= 1.01f;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            scale *= 0.99f;
    }

    glm::mat4 getTransform() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
        model = glm::translate(model, glm::vec3(offset, 0.0f));
        return model;
    }

private:
    float scale;
    glm::vec2 offset;
};

class Texture {
public:
    // Vertex Shader Source
    const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTex;

out vec3 color;
out vec2 texCoord;

uniform mat4 model;

void main()
{
    gl_Position = model * vec4(aPos, 1.0);
    color = aColor;
    texCoord = aTex;
}
)";

    // Fragment Shader Source
    const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 color;
in vec2 texCoord;

uniform sampler2D tex0;

void main()
{
    FragColor = texture(tex0, texCoord);
}
)";
    GLuint shaderProgram;
    GLint modelLoc;
    GLuint VAO, VBO, EBO;
    GLuint textureID;
    Camera* m_camera = nullptr;
    int imageWidth, imageHeight;

    void init(Camera* camera, const std::string& imagePath) {
        m_camera = camera;

        // Load image
        if (!loadTIFF(imagePath, &imageWidth, &imageHeight)) {
            std::cerr << "Failed to load texture" << std::endl;
            return;
        }

        // Compute aspect ratio
        float aspectRatio = static_cast<float>(imageWidth) / imageHeight;

        // Adjust vertex positions based on aspect ratio
        float vertices[] = {
            // Positions           // Colors           // Texture Coords
            -aspectRatio, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
            -aspectRatio,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
             aspectRatio,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
             aspectRatio, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f
        };

        GLuint indices[] = {
            0, 1, 2,
            0, 2, 3
        };

        // Create shader program
        shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

        // Setup VAO, VBO, EBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        modelLoc = glGetUniformLocation(shaderProgram, "model");
    }

    bool loadTIFF(const std::string& filename, int* width, int* height) {
        TIFF* tif = TIFFOpen(filename.c_str(), "r");
        if (!tif) {
            return false;
        }

        uint32_t w, h;
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
        *width = w;
        *height = h;

        size_t npixels = w * h;
        uint32_t* data = (uint32_t*)_TIFFmalloc(npixels * sizeof(uint32_t));
        if (data == NULL) {
            TIFFClose(tif);
            return false;
        }

        if (TIFFReadRGBAImage(tif, w, h, data, 0)) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else {
            _TIFFfree(data);
            TIFFClose(tif);
            return false;
        }

        _TIFFfree(data);
        TIFFClose(tif);
        return true;
    }

    void render() {
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);

        // Apply camera transformation
        glm::mat4 model = m_camera->getTransform();
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Bind texture and draw
        glBindTexture(GL_TEXTURE_2D, textureID);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
    }

private:
    GLuint createShaderProgram(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        GLint success;
        GLchar infoLog[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    }

    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);

        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        return shader;
    }
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "Texture Viewer", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    Camera camera;
    Texture texture;
    texture.init(&camera, "../../data/test/test.tif");

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        camera.processKeyboardInput(window);
        texture.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
