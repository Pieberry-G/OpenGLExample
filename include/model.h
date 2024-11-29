#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include <mesh.h>
#include <shader.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

unsigned int TextureFromFile(unsigned char* data, int width, int height);

class Model
{
public:
    /*
        Model data
    */
    std::vector<uint32_t> images;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;

public:
    Model(std::string filename)
    {
        loadglTFFile(filename);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader& shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader, images, textures, materials);
    }

    void loadglTFFile(std::string filename)
    {
        tinygltf::Model glTFInput;
        tinygltf::TinyGLTF gltfContext;
        std::string error, warning;

        //bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);
        bool fileLoaded = gltfContext.LoadBinaryFromFile(&glTFInput, &error, &warning, filename);

        if (fileLoaded) {
            loadImages(glTFInput);
            loadMaterials(glTFInput);
            loadTextures(glTFInput);
            const tinygltf::Scene& scene = glTFInput.scenes[0];
            for (size_t i = 0; i < scene.nodes.size(); i++) {
                const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
                loadNode(node, glTFInput);
            }
        }
        else {
            return;
        }
    }

    void loadImages(tinygltf::Model& input)
    {
        // Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
        // loading them from disk, we fetch them from the glTF loader and upload the buffers
        images.resize(input.images.size());
        for (size_t i = 0; i < input.images.size(); i++) {
            tinygltf::Image& glTFImage = input.images[i];
            // Get the image data from the glTF loader
            unsigned char* buffer = nullptr;
            int32_t bufferSize = 0;
            bool deleteBuffer = false;
            // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
            if (glTFImage.component == 3) {
                bufferSize = glTFImage.width * glTFImage.height * 4;
                buffer = new unsigned char[bufferSize];
                unsigned char* rgba = buffer;
                unsigned char* rgb = &glTFImage.image[0];
                for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
                    memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                    rgba += 4;
                    rgb += 3;
                }
                deleteBuffer = true;
            }
            else {
                buffer = &glTFImage.image[0];
                bufferSize = glTFImage.image.size();
            }
            // Load texture from image buffer
            images[i] = TextureFromFile(buffer, glTFImage.width, glTFImage.height);
            if (deleteBuffer) {
                delete[] buffer;
            }
        }
    }

    void loadTextures(tinygltf::Model& input)
    {
        textures.resize(input.textures.size());
        for (size_t i = 0; i < input.textures.size(); i++) {
            textures[i].imageIndex = input.textures[i].source;
        }
    }

    void loadMaterials(tinygltf::Model& input)
    {
        materials.resize(input.materials.size());
        for (size_t i = 0; i < input.materials.size(); i++) {
            // We only read the most basic properties required for our sample
            tinygltf::Material glTFMaterial = input.materials[i];
            // Get the base color factor
            if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
                materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
            }
            // Get base color texture index
            if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
                materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
            }
        }
    }

    void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input)
    {
        //VulkanglTFModel::Node* node = new VulkanglTFModel::Node{};
        //node->matrix = glm::mat4(1.0f);
        //node->parent = parent;

        //// Get the local node matrix
        //// It's either made up from translation, rotation, scale or a 4x4 matrix
        //if (inputNode.translation.size() == 3) {
        //    node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
        //}
        //if (inputNode.rotation.size() == 4) {
        //    glm::quat q = glm::make_quat(inputNode.rotation.data());
        //    node->matrix *= glm::mat4(q);
        //}
        //if (inputNode.scale.size() == 3) {
        //    node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
        //}
        //if (inputNode.matrix.size() == 16) {
        //    node->matrix = glm::make_mat4x4(inputNode.matrix.data());
        //};

        // Load node's children
        if (inputNode.children.size() > 0) {
            for (size_t i = 0; i < inputNode.children.size(); i++) {
                loadNode(input.nodes[inputNode.children[i]], input);
            }
        }

        // If the node contains mesh data, we load vertices and indices from the buffers
        // In glTF this is done via accessors and buffer views
        if (inputNode.mesh > -1) {
            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            // Iterate through all primitives of this node's mesh
            for (size_t i = 0; i < mesh.primitives.size(); i++) {

                vector<Vertex> vertices;
                vector<unsigned int> indices;

                const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
                uint32_t indexCount = 0;
                // Vertices
                {
                    const float* positionBuffer = nullptr;
                    const float* normalsBuffer = nullptr;
                    const float* texCoordsBuffer = nullptr;
                    size_t vertexCount = 0;

                    // Get buffer data for vertex positions
                    if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;
                    }
                    // Get buffer data for vertex normals
                    if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }
                    // Get buffer data for vertex texture coordinates
                    // glTF supports multiple sets, we only load the first one
                    if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    // Append data to model's vertex buffer
                    for (size_t v = 0; v < vertexCount; v++) {
                        Vertex vert{};
                        vert.Position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                        vert.Normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.TexCoords = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        //vert.color = glm::vec3(1.0f);
                        vertices.push_back(vert);
                    }
                }
                // Indices
                {
                    const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
                    const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                    indexCount += static_cast<uint32_t>(accessor.count);

                    // glTF supports different component types of indices
                    switch (accessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indices.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indices.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indices.push_back(buf[index]);
                        }
                        break;
                    }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                    }
                }
                meshes.push_back(Mesh(vertices, indices, glTFPrimitive.material));
            }
        }
    }
};

unsigned int TextureFromFile(unsigned char* data, int width, int height)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    if (data)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        std::cout << "Texture failed to load texture." << std::endl;
    }

    return textureID;
}

#endif