 
I am working on rendering a large image in tiles using OpenGL. My goal is to render images 
in tiles, but i am strugling  maintain the aspect ratio of the original image when applying a camera transformation. 

I am unable to figure it out myself and id be more than grateful for any hint. 

I am a c51 person used to micro LCD, 7seg display uis, a console person, and c++ is graphic arts and crafts are new to me , for what I in advance apologize for quite a maybe obvious question. 

With that said, i'd really appreciate any hint that could shed some light to the issue. 

### Code Overview

I am using OpenGL along with GLFW for window management and GLEW for handling OpenGL extensions. The image is loaded using the `stb_image` library. Below is a summary of the relevant code in `main.cpp`:


See it in github  [Rendering with camera (aspect ratio error)](https://github.com/iorp/cpp-iorp-monolithics/blob/main/Projects/IorpMonolithics0/src/textures/texture_png_tiled0.main.cpp).


```cpp
// main.cpp 
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio> // Include for printf

const int tileWidth = 256;
const int tileHeight = 256;

// Global Variables for LOD and Mipmap Settings
// Level of Detail (LOD) bias, typically in the range -0.5 to 0.5
float lodBias = 0.0f;
// Mipmap level to use, starting from 0 for the base level
int mipmapLevel = 0;
// Maximum mipmap level to use (adjust based on your needs and texture size)
int maxMipmapLevel = 4;

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
    int numTilesX, numTilesY;

    void init(Camera* camera, const std::string& imagePath) {
        // Assign the camera pointer to the member variable
        m_camera = camera;

        // Load image using stb_image
        int nrChannels;
        // stbi_load loads the image and gives us the width, height, and number of channels
        unsigned char* data = stbi_load(imagePath.c_str(), &imageWidth, &imageHeight, &nrChannels, STBI_rgb_alpha);
        // Check if the image was loaded successfully
        if (!data) {
            std::cerr << "Failed to load texture" << std::endl;
            return;
        }

        // Calculate the number of tiles needed in the X and Y directions
        numTilesX = (imageWidth + tileWidth - 1) / tileWidth;
        numTilesY = (imageHeight + tileHeight - 1) / tileHeight;

        // Calculate the size of the last tile in the last row and last column
        int lastTileWidth = (imageWidth % tileWidth == 0) ? tileWidth : (imageWidth % tileWidth);
        int lastTileHeight = (imageHeight % tileHeight == 0) ? tileHeight : (imageHeight % tileHeight);

        // Adjust if the last tile in the row or column is the full size
        if (numTilesX * tileWidth > imageWidth) {
            lastTileWidth = imageWidth % tileWidth;
            if (lastTileWidth == 0) lastTileWidth = tileWidth;
        }
        if (numTilesY * tileHeight > imageHeight) {
            lastTileHeight = imageHeight % tileHeight;
            if (lastTileHeight == 0) lastTileHeight = tileHeight;
        }

        // Print out details about the image and tiles
        printf("Image size: %d x %d\n", imageWidth, imageHeight);
        printf("Number of tiles (X x Y): %d x %d\n", numTilesX, numTilesY);
        printf("Tile size: %d x %d\n", tileWidth, tileHeight);
        printf("Last row tile size: %d x %d\n", lastTileWidth, tileHeight);
        printf("Last column tile size: %d x %d\n", tileWidth, lastTileHeight);

        // Create and compile shaders, then link them into a program
        shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

        // Setup Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO)
        glGenVertexArrays(1, &VAO);  // Generate VAO
        glGenBuffers(1, &VBO);       // Generate VBO
        glGenBuffers(1, &EBO);       // Generate EBO

        glBindVertexArray(VAO);      // Bind VAO

        glBindBuffer(GL_ARRAY_BUFFER, VBO);         // Bind VBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO); // Bind EBO

        // Set vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); // Position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); // Color
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); // Texture Coordinate
        glEnableVertexAttribArray(2);

        glBindVertexArray(0); // Unbind VAO

        // Load and setup the texture
        glGenTextures(1, &textureID);         // Generate texture ID
        glBindTexture(GL_TEXTURE_2D, textureID); // Bind texture

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Wrapping mode for S coordinate
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Wrapping mode for T coordinate
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Filtering mode for minification
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);    // Filtering mode for magnification

        // Load texture image data into OpenGL
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps

        // Free image memory
        stbi_image_free(data);

        // Get the location of the 'model' uniform in the shader program
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
        glBindVertexArray(VAO);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Render tiles
        for (int tileY = 0; tileY < numTilesY; ++tileY) {
            for (int tileX = 0; tileX < numTilesX; ++tileX) {
                glm::mat4 model = m_camera->getTransform();

                // Calculate tile position and size
                int xOffset = tileX * tileWidth;
                int yOffset = tileY * tileHeight;
                int currentTileWidth = std::min(tileWidth, imageWidth - xOffset);
                int currentTileHeight = std::min(tileHeight, imageHeight - yOffset);

                // Adjust vertex positions based on the current tile size
                float vertices[] = {
                    // Positions          // Colors          // Texture Coords
                    0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  static_cast<float>(xOffset) / imageWidth, 1.0f - static_cast<float>(yOffset) / imageHeight,
                    0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  static_cast<float>(xOffset) / imageWidth, 1.0f - static_cast<float>(yOffset + currentTileHeight) / imageHeight,
                    1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  static_cast<float>(xOffset + currentTileWidth) / imageWidth, 1.0f - static_cast<float>(yOffset + currentTileHeight) / imageHeight,
                    1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  static_cast<float>(xOffset + currentTileWidth) / imageWidth, 1.0f - static_cast<float>(yOffset) / imageHeight
                };

                GLuint indices[] = {
                    0, 1, 2,
                    0, 2, 3
                };

                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

                // Translate to the tile's position
                model = glm::translate(model, glm::vec3(
                    (2.0f * xOffset / static_cast<float>(imageWidth)) - 1.0f,
                    (2.0f * yOffset / static_cast<float>(imageHeight)) - 1.0f,
                    0.0f
                ));
                model = glm::scale(model, glm::vec3(
                    2.0f * currentTileWidth / static_cast<float>(imageWidth),
                    2.0f * currentTileHeight / static_cast<float>(imageHeight),
                    1.0f
                ));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }
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

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Camera camera;
    Texture texture;
    texture.init(&camera, "../../data/test/test_nb.png");

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
```

### Problem

The image renders correctly when the camera transformation is not applied, but when the camera is included, the tiles do not align properly, and the aspect ratio of the image is not maintained. Specifically, the issues are:

1. **Tiles are not positioned correctly**: They appear misaligned or overlapping.
2. **Aspect ratio is distorted**: The image does not maintain its original proportions.

### What I Have Tried

- Ensured that the model matrix is correctly applied for positioning and scaling.
- Verified that the vertex positions and texture coordinates are being calculated correctly.

I have managed to get the image rendering correctly without applying the camera transformation. However, when I include the camera transformation, the issues arise.

See it in github  [Rendering without camera](https://github.com/iorp/cpp-iorp-monolithics/blob/main/Projects/IorpMonolithics0/src/textures/texture_png_tiled0_no_cam.main.cpp).

Or shere is it: 
```cpp
// no cam main.cpp  
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Vertex Shader Source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

// Fragment Shader Source
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D ourTexture;
void main() {
    FragColor = texture(ourTexture, TexCoord);
}
)";

// Compile Shader Function
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

// Create Shader Program Function
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint shaderProgram = glCreateProgram();
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

// Framebuffer Size Callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return -1;
    }

    // Load Texture
    int imageWidth, imageHeight, nrChannels;
    unsigned char* data = stbi_load("../../data/test/test_nb.png", &imageWidth, &imageHeight, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture" << std::endl;
        return -1;
    }

    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    float aspectRatio = static_cast<float>(imageWidth) / imageHeight;
    float windowAspectRatio = 800.0f / 600.0f;
    float scaleX, scaleY;

    if (aspectRatio > windowAspectRatio) {
        scaleX = 1.0f;
        scaleY = windowAspectRatio / aspectRatio;
    }
    else {
        scaleX = aspectRatio / windowAspectRatio;
        scaleY = 1.0f;
    }

    float vertices[] = {
        // positions            // texture coords
        scaleX, -scaleY, 0.0f,  1.0f, 1.0f, // top right
        scaleX,  scaleY, 0.0f,  1.0f, 0.0f, // bottom right
       -scaleX,  scaleY, 0.0f,  0.0f, 0.0f, // bottom left
       -scaleX, -scaleY, 0.0f,  0.0f, 1.0f  // top left 
    };
    GLuint indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    // Set up blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glBindTexture(GL_TEXTURE_2D, texture);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &texture);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
```



### In short

How can I adjust the vertex positions, texture coordinates, and model transformations to ensure that:

- The image maintains its original aspect ratio.
- Tiles are correctly positioned and sized even when applying the camera transformation.

Thank you for your help!
 