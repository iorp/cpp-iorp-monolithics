#include <iostream>
#include <vector>
#include <GL/glew.h>
// render jpg
#include <GLFW/glfw3.h>
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
        int nrChannels;
        unsigned char* data = stbi_load(imagePath.c_str(), &imageWidth, &imageHeight, &nrChannels, STBI_rgb_alpha);
        if (!data) {
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

        // Load texture
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);

        modelLoc = glGetUniformLocation(shaderProgram, "model");
    }

    // Function to compile shaders
    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader Compilation Error: " << infoLog << std::endl;
        }
        return shader;
    }

    // Function to create shader program
    GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        GLint success;
        GLchar infoLog[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "Program Linking Error: " << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    }

    void render() {
        glUseProgram(shaderProgram);
        glm::mat4 model = m_camera->getTransform();
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void destroy() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteProgram(shaderProgram);
        glDeleteTextures(1, &textureID);
    }
};

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set GLFW options
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 800, "OpenGL", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return -1;
    }

    // Setup viewport
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    Camera camera;
    Texture texture;
    texture.init(&camera, "../../data/test/test.jpg");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        camera.processKeyboardInput(window);
        texture.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    texture.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
