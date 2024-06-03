#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define WINDOW_WIDTH 800.0f
#define WINDOW_HEIGHT 600.0f
#define CAMERA_STEP 2.0f

// Shader
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;

    uniform sampler2D texture1;
    void main()
    {
        vec4 texColor = texture(texture1, TexCoord);
        FragColor = texColor;
    }
)glsl";

GLuint shaderProgram;
class Camera* camera;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint loadShader(GLenum type, const char* source);
GLuint loadTexture(const std::string& path);

class Model
{
    std::vector<float> vertices;
    std::vector<float> texcoords;
    std::vector<unsigned int> indices;
    GLuint vao, vbo, ebo;
    GLuint textureID;

    bool loadModel(const std::string& path, const std::string& texturePath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
        {
            std::cerr << "Error al cargar/parsear el archivo .obj: " << warn << err << std::endl;
            return false;
        }

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);
                if (!attrib.texcoords.empty()) {
                    texcoords.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                    texcoords.push_back(attrib.texcoords[2 * index.texcoord_index + 1]);
                }
                indices.push_back(indices.size());
            }
        }

        setUpVao();

        textureID = loadTexture(texturePath);

        return true;
    }

    void setUpVao()
    {
        std::cout << "Vertices : " << vertices.size() << std::endl;
        std::cout << "Indices: " << indices.size() << std::endl;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        if (!texcoords.empty())
        {
            GLuint texVBO;
            glGenBuffers(1, &texVBO);
            glBindBuffer(GL_ARRAY_BUFFER, texVBO);
            glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(float), texcoords.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

public:
    Model(const std::vector<float>& _vertices, const std::vector<unsigned int>& _indices, const std::vector<float>& _texCoord = {}, GLuint _textureID = -1) :
        vertices(_vertices), texcoords(_texCoord), indices(_indices), textureID(_textureID)
    {
        setUpVao();
    }

    Model(const std::string& path, const std::string& texturePath)
    {
        bool build = loadModel(path, texturePath);
        if (!build)
            exit(1);
    }

    void draw()
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// Model loadCubeModel()
// {
//     std::vector<float> vertices = {
//         -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
//         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
//         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
//         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
//         -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
//         -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
//     };
//     std::vector<unsigned int> indices = {
//         0, 1, 3,  // Cara frontal
//         1, 2, 3,  // Cara frontal
//     };

//     return Model(vertices, indices);
// }

class Object
{
    glm::vec4 position;
    glm::mat4x4 transformation;
    Model* model;

public:
    Object(Model* _model, const glm::mat4x4& _transformation) :
        transformation(_transformation), model(_model)
    {
        position = transformation * glm::vec4(0.0f);
    }

    void draw()
    {
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(transformation));
        model->draw();
    }
};

class Camera
{
    glm::vec3 position;
    glm::vec3 center;

    glm::mat4x4 viewMatrix;
    glm::mat4x4 projMatrix;

    float fovy, aspect, near, far;

    void updateViewMatrix()
    {
        viewMatrix = glm::lookAt(position, center, glm::vec3(0.0f, 0.1f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
    }

public:
    Camera(const glm::vec3& _position, const glm::vec3& _center, float _fovy, float _aspect, float _near, float _far) :
        position(_position), center(_center), fovy(_fovy), aspect(_aspect), near(_near), far(_far)
    {
        updateViewMatrix();
        projMatrix = glm::perspective(fovy, aspect, near, far);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projMatrix));
    }

    void move(const glm::vec3& amount)
    {
        center += amount;
        position += amount;

        updateViewMatrix();
    }

    void turn(const glm::vec3& amount)
    {
        center += amount;

        updateViewMatrix();
    }

    glm::vec3 getCenter()
    {
        return center;
    }

    glm::vec3 getPosition()
    {
        return position;
    }
};

int main()
{
    // Inicializar GLFW
    if (!glfwInit()) {
        std::cerr << "Error al inicializar GLFW" << std::endl;
        return -1;
    }

    // Crear ventana
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Cargador de múltiples OBJ", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Error al crear la ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, processKeyInput);

    // Inicializar GLAD
    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cout << "Error al inicializar GLAD.\n";
        glfwTerminate();
        return -1;
    }

    // Compilar shaders
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Comprobar errores de enlace
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Relative Path
	std::filesystem::path p = std::filesystem::current_path();
	int levels_path = 1;
	std::filesystem::path p_current;
	p_current = p.parent_path();

	for (int i = 0; i < levels_path; i++)
	{
		p_current = p_current.parent_path();
	}

	std::string vs_path, fs_path;

	std::stringstream ss;
	ss << std::quoted(p_current.string());
	std::string out;
	ss >> std::quoted(out);

	std::cout << "\nCurrent path: " << out << "\n";

    // Cargar modelos
    std::vector<Model> models;
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\Lowpoly_Tree.obj", out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\wall.jpg"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\Hogar.obj", out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\wall.jpg"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\10438_Circular_Grass_Patch_v1_iterations-2.obj", out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\10438_Circular_Grass_Patch_v1_Diffuse.jpg"));
    models.push_back(Model(out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\Lowpoly_Fox.obj", out + "\\glfw-master\\OwnProjects\\Project_13\\Models\\wall.jpg"));

    // Crear objetos
    std::vector<Object> objects = {
        Object(&models[0], // Tree1
            glm::scale(
                glm::translate(
                    glm::mat4x4(1.0f),
                    glm::vec3(-100.0f, 0.0f, 0.0f)
                ),
                glm::vec3(0.9f)
            )
        ),
        Object(&models[0], // Tree2
            glm::scale(
                glm::translate(
                    glm::mat4x4(1.0f),
                    glm::vec3(50.0f, 0.0f, 0.0f)
                ),
                glm::vec3(0.9f)
            )
        ),
        Object(&models[1], // House
            glm::scale(
                glm::translate(
                    glm::mat4x4(1.0f),
                    glm::vec3(0.0f, 0.0f, -50.0f)
                ), 
                glm::vec3(0.9f)
            )
        ),
        Object(&models[2], // Grass
            glm::rotate(
                glm::translate(
                    glm::mat4x4(1.0f),
                    glm::vec3(0.0f, -10.0f, 0.0f)
                ),
                glm::radians(-90.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)
            )
        ),
        Object(&models[3], // Fox
            glm::scale(
                glm::translate(
                    glm::mat4x4(1.0f),
                    glm::vec3(0.0f, 0.0f, 50.0f)
                ),
                glm::vec3(0.2f)
            )
        )
    };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glUseProgram(shaderProgram);

    camera = new Camera(glm::vec3(0.0f, 10.0f, 10.0f), glm::vec3(0.0f, 10.0f, 0.0f), glm::radians(45.0f), WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 1000.0f);

    // Bucle de renderizado
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Renderizar
        for (size_t i = 0; i < objects.size(); ++i)
        {
            objects[i].draw();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, true);

    if (action == GLFW_PRESS && key == GLFW_KEY_LEFT)
        camera->move(-1.0f * CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
    if (action == GLFW_PRESS && key == GLFW_KEY_RIGHT)
        camera->move(CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
    if (action == GLFW_PRESS && key == GLFW_KEY_UP)
        camera->move(CAMERA_STEP * glm::normalize(camera->getCenter() - camera->getPosition()));
    if (action == GLFW_PRESS && key == GLFW_KEY_DOWN)
        camera->move(-1.0f * CAMERA_STEP * glm::normalize(camera->getCenter() - camera->getPosition()));
    
    if (action == GLFW_PRESS && key == GLFW_KEY_A)
        camera->turn(-0.5f * CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
    if (action == GLFW_PRESS && key == GLFW_KEY_D)
        camera->turn(0.5f * CAMERA_STEP * glm::normalize(glm::cross(camera->getCenter() - camera->getPosition(), glm::vec3(0.0f, 1.0f, 0.0f))));
}

GLuint loadShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);


    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint loadTexture(const std::string& path)
{
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Error al cargar la textura: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
