// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/rendering/gl/OpenGLRenderer.hpp"

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

#include "finaleffect/platform/glfw/GlfwWindow.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace fe::rendering::gl {
namespace {

// Minimal OpenGL type set. Declared locally so that no GL header and no generated
// loader is required: only glfwGetProcAddress is used.
using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLsizei = int;
using GLuint = unsigned int;
using GLfloat = float;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

constexpr GLenum kGLColorBufferBit = 0x00004000U;
constexpr GLenum kGLDepthBufferBit = 0x00000100U;
constexpr GLenum kGLDepthTest = 0x0B71U;
constexpr GLenum kGLLeQual = 0x0203U;
constexpr GLenum kGLTriangles = 0x0004U;
constexpr GLenum kGLUnsignedInt = 0x1405U;
constexpr GLenum kGLFloat = 0x1406U;
constexpr GLenum kGLVertexShader = 0x8B31U;
constexpr GLenum kGLFragmentShader = 0x8B30U;
constexpr GLenum kGLCompileStatus = 0x8B81U;
constexpr GLenum kGLLinkStatus = 0x8B82U;
constexpr GLenum kGLInfoLogLength = 0x8B84U;
constexpr GLenum kGLArrayBuffer = 0x8892U;
constexpr GLenum kGLElementArrayBuffer = 0x8893U;
constexpr GLenum kGLStaticDraw = 0x88E4U;
constexpr GLenum kGLRgba = 0x1908U;
constexpr GLenum kGLUnsignedByte = 0x1401U;
constexpr GLenum kGLPackAlignment = 0x0D05U;

constexpr GLuint kAttributePosition = 0U;
constexpr GLuint kAttributeNormal = 1U;
constexpr GLsizei kVertexStride = static_cast<GLsizei>(sizeof(float) * 6U);

const char* kVertexShaderSource = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uViewProjection;

out vec3 vWorldNormal;

void main() {
    vWorldNormal = mat3(uModel) * aNormal;
    gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0);
}
)GLSL";

const char* kFragmentShaderSource = R"GLSL(#version 330 core
in vec3 vWorldNormal;

uniform vec3 uColor;

out vec4 fragColor;

void main() {
    vec3 normal = normalize(vWorldNormal);
    float lambert = max(0.0, dot(normal, normalize(vec3(0.35, 0.85, 0.40))));
    float intensity = 0.22 + 0.78 * lambert;
    fragColor = vec4(uColor * intensity, 1.0);
}
)GLSL";

}  // namespace

/// Resolved OpenGL entry points. Owned by the renderer, never global.
struct OpenGLRenderer::GLApi {
    void (*clearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (*clear)(GLbitfield) = nullptr;
    void (*viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (*enable)(GLenum) = nullptr;
    void (*disable)(GLenum) = nullptr;
    void (*depthFunc)(GLenum) = nullptr;
    void (*drawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;
    GLenum (*getError)() = nullptr;

    GLuint (*createShader)(GLenum) = nullptr;
    void (*shaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (*compileShader)(GLuint) = nullptr;
    void (*getShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*getShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*deleteShader)(GLuint) = nullptr;

    GLuint (*createProgram)() = nullptr;
    void (*attachShader)(GLuint, GLuint) = nullptr;
    void (*linkProgram)(GLuint) = nullptr;
    void (*getProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*getProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*deleteProgram)(GLuint) = nullptr;
    void (*useProgram)(GLuint) = nullptr;
    GLint (*getUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (*uniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
    void (*uniform3fv)(GLint, GLsizei, const GLfloat*) = nullptr;

    void (*genVertexArrays)(GLsizei, GLuint*) = nullptr;
    void (*bindVertexArray)(GLuint) = nullptr;
    void (*deleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
    void (*genBuffers)(GLsizei, GLuint*) = nullptr;
    void (*bindBuffer)(GLenum, GLuint) = nullptr;
    void (*bufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (*deleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (*enableVertexAttribArray)(GLuint) = nullptr;
    void (*vertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;

    void (*readPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
    void (*pixelStorei)(GLenum, GLint) = nullptr;

    /// Loads every entry point. Returns the name of the first missing function,
    /// or an empty string when the context provides all of them.
    std::string load() {
        const auto resolve = [](const char* name) { return glfwGetProcAddress(name); };
        std::string missing;

        const auto assign = [&missing, &resolve](const char* name, auto& target) {
            if (!missing.empty()) {
                return;
            }
            const GLFWglproc proc = resolve(name);
            if (proc == nullptr) {
                missing = name;
                return;
            }
            target = reinterpret_cast<std::decay_t<decltype(target)>>(proc);
        };

        assign("glClearColor", clearColor);
        assign("glClear", clear);
        assign("glViewport", viewport);
        assign("glEnable", enable);
        assign("glDisable", disable);
        assign("glDepthFunc", depthFunc);
        assign("glDrawElements", drawElements);
        assign("glGetError", getError);
        assign("glCreateShader", createShader);
        assign("glShaderSource", shaderSource);
        assign("glCompileShader", compileShader);
        assign("glGetShaderiv", getShaderiv);
        assign("glGetShaderInfoLog", getShaderInfoLog);
        assign("glDeleteShader", deleteShader);
        assign("glCreateProgram", createProgram);
        assign("glAttachShader", attachShader);
        assign("glLinkProgram", linkProgram);
        assign("glGetProgramiv", getProgramiv);
        assign("glGetProgramInfoLog", getProgramInfoLog);
        assign("glDeleteProgram", deleteProgram);
        assign("glUseProgram", useProgram);
        assign("glGetUniformLocation", getUniformLocation);
        assign("glUniformMatrix4fv", uniformMatrix4fv);
        assign("glUniform3fv", uniform3fv);
        assign("glGenVertexArrays", genVertexArrays);
        assign("glBindVertexArray", bindVertexArray);
        assign("glDeleteVertexArrays", deleteVertexArrays);
        assign("glGenBuffers", genBuffers);
        assign("glBindBuffer", bindBuffer);
        assign("glBufferData", bufferData);
        assign("glDeleteBuffers", deleteBuffers);
        assign("glEnableVertexAttribArray", enableVertexAttribArray);
        assign("glVertexAttribPointer", vertexAttribPointer);
        assign("glReadPixels", readPixels);
        assign("glPixelStorei", pixelStorei);
        return missing;
    }
};

OpenGLRenderer::OpenGLRenderer() : api_(nullptr) {
    capabilities_.backendName = "opengl-3.3-core";
    capabilities_.offscreenOnly = false;
    capabilities_.supportsReadback = true;
    capabilities_.supportsDepthTest = true;
}

OpenGLRenderer::~OpenGLRenderer() {
    if (initialized_) {
        (void)shutdown();
    }
}

core::Status OpenGLRenderer::attachWindow(platform::GlfwWindow& window) {
    if (window.nativeHandle() == nullptr || !window.hasGraphicsContext()) {
        return core::makeError(core::ErrorCode::GraphicsUnavailable,
                               "window has no OpenGL context to attach to",
                               "opengl renderer");
    }
    window_ = &window;
    return core::successStatus();
}

core::Status OpenGLRenderer::initialize(const RendererInitInfo& info) {
    if (initialized_) {
        return core::makeError(core::ErrorCode::AlreadyExists, "renderer is already initialized",
                               "opengl renderer");
    }
    if (window_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized,
                               "attachWindow must be called before initialize",
                               "opengl renderer");
    }
    if (info.width == 0 || info.height == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "framebuffer size must be non-zero",
                               "opengl renderer");
    }

    const core::Status contextStatus = window_->makeContextCurrent();
    if (!contextStatus.ok()) {
        return contextStatus;
    }

    api_ = std::make_unique<GLApi>();
    const std::string missing = api_->load();
    if (!missing.empty()) {
        api_.reset();
        return core::makeError(core::ErrorCode::GraphicsUnavailable,
                               "OpenGL 3.3 entry point is unavailable: " + missing,
                               "opengl renderer");
    }

    std::string log;
    const core::Status programStatus = buildProgram(log);
    if (!programStatus.ok()) {
        api_.reset();
        return programStatus;
    }

    api_->enable(kGLDepthTest);
    api_->depthFunc(kGLLeqqual);
    width_ = info.width;
    height_ = info.height;
    api_->viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    initialized_ = true;
    return core::successStatus();
}

core::Status OpenGLRenderer::buildProgram(std::string& log) {
    const auto compileShader = [this](GLenum type, const char* source) -> core::Result<GLuint> {
        const GLuint shader = api_->createShader(type);
        if (shader == 0U) {
            return core::makeError(core::ErrorCode::InitializationFailed, "glCreateShader failed",
                                   "opengl renderer");
        }
        api_->shaderSource(shader, 1, &source, nullptr);
        api_->compileShader(shader);
        GLint status = 0;
        api_->getShaderiv(shader, kGLCompileStatus, &status);
        if (status == 0) {
            std::vector<GLchar> buffer(2048, '\0');
            GLsizei written = 0;
            api_->getShaderInfoLog(shader, static_cast<GLsizei>(buffer.size()), &written, buffer.data());
            log.assign(buffer.data(), static_cast<std::size_t>(written > 0 ? written : 0));
            api_->deleteShader(shader);
            return core::makeError(core::ErrorCode::InitializationFailed,
                                   "shader compilation failed: " + log,
                                   "opengl renderer");
        }
        return shader;
    };

    const core::Result<GLuint> vertexShader = compileShader(kGLVertexShader, kVertexShaderSource);
    if (!vertexShader.ok()) {
        return vertexShader.error();
    }
    const core::Result<GLuint> fragmentShader = compileShader(kGLFragmentShader, kFragmentShaderSource);
    if (!fragmentShader.ok()) {
        api_->deleteShader(vertexShader.value());
        return fragmentShader.error();
    }

    const GLuint program = api_->createProgram();
    if (program == 0U) {
        api_->deleteShader(vertexShader.value());
        api_->deleteShader(fragmentShader.value());
        return core::makeError(core::ErrorCode::InitializationFailed, "glCreateProgram failed",
                               "opengl renderer");
    }
    api_->attachShader(program, vertexShader.value());
    api_->attachShader(program, fragmentShader.value());
    api_->linkProgram(program);

    GLint linked = 0;
    api_->getProgramiv(program, kGLLinkStatus, &linked);
    if (linked == 0) {
        std::vector<GLchar> buffer(2048, '\0');
        GLsizei written = 0;
        api_->getProgramInfoLog(program, static_cast<GLsizei>(buffer.size()), &written, buffer.data());
        log.assign(buffer.data(), static_cast<std::size_t>(written > 0 ? written : 0));
        api_->deleteProgram(program);
        api_->deleteShader(vertexShader.value());
        api_->deleteShader(fragmentShader.value());
        return core::makeError(core::ErrorCode::InitializationFailed,
                               "program link failed: " + log,
                               "opengl renderer");
    }
    api_->deleteShader(vertexShader.value());
    api_->deleteShader(fragmentShader.value());

    program_ = program;
    uniformModel_ = api_->getUniformLocation(program_, "uModel");
    uniformViewProjection_ = api_->getUniformLocation(program_, "uViewProjection");
    uniformColor_ = api_->getUniformLocation(program_, "uColor");
    if (uniformModel_ < 0 || uniformViewProjection_ < 0 || uniformColor_ < 0) {
        return core::makeError(core::ErrorCode::InitializationFailed,
                               "shader uniforms were optimized out or renamed",
                               "opengl renderer");
    }
    return core::successStatus();
}

core::Status OpenGLRenderer::uploadMesh(const MeshData& mesh, const GpuMesh*& out) {
    for (const auto& entry : meshLookup_) {
        if (entry.first == &mesh) {
            out = &gpuMeshes_[entry.second];
            return core::successStatus();
        }
    }
    if (mesh.vertices.empty() || !mesh.isValid()) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               "mesh '" + mesh.name + "' has no usable geometry",
                               "opengl renderer");
    }

    std::vector<GLfloat> interleaved;
    interleaved.reserve(mesh.vertices.size() * 6U);
    for (const Vertex& vertex : mesh.vertices) {
        interleaved.push_back(vertex.position.x);
        interleaved.push_back(vertex.position.y);
        interleaved.push_back(vertex.position.z);
        interleaved.push_back(vertex.normal.x);
        interleaved.push_back(vertex.normal.y);
        interleaved.push_back(vertex.normal.z);
    }

    GpuMesh gpu;
    api_->genVertexArrays(1, &gpu.vertexArray);
    api_->bindVertexArray(gpu.vertexArray);

    api_->genBuffers(1, &gpu.vertexBuffer);
    api_->bindBuffer(kGLArrayBuffer, gpu.vertexBuffer);
    api_->bufferData(kGLArrayBuffer,
                     static_cast<GLsizeiptr>(interleaved.size() * sizeof(GLfloat)),
                     interleaved.data(),
                     kGLStaticDraw);

    api_->genBuffers(1, &gpu.indexBuffer);
    api_->bindBuffer(kGLElementArrayBuffer, gpu.indexBuffer);
    api_->bufferData(kGLElementArrayBuffer,
                     static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                     mesh.indices.data(),
                     kGLStaticDraw);

    api_->enableVertexAttribArray(kAttributePosition);
    api_->vertexAttribPointer(kAttributePosition, 3, kGLFloat, 0, kVertexStride,
                              reinterpret_cast<const void*>(static_cast<std::size_t>(0)));
    api_->enableVertexAttribArray(kAttributeNormal);
    api_->vertexAttribPointer(kAttributeNormal, 3, kGLFloat, 0, kVertexStride,
                              reinterpret_cast<const void*>(sizeof(GLfloat) * 3U));
    api_->bindVertexArray(0);

    gpu.indexCount = static_cast<std::int32_t>(mesh.indices.size());
    gpuMeshes_.push_back(gpu);
    meshLookup_.emplace_back(&mesh, gpuMeshes_.size() - 1U);
    out = &gpuMeshes_.back();
    return core::successStatus();
}

core::Status OpenGLRenderer::resize(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "framebuffer size must be non-zero",
                               "opengl renderer");
    }
    width_ = width;
    height_ = height;
    if (api_ != nullptr) {
        api_->viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    }
    return core::successStatus();
}

core::Status OpenGLRenderer::beginFrame(const FrameInfo& frame) {
    if (!initialized_ || api_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "renderer was not initialized",
                               "opengl renderer");
    }
    api_->clearColor(frame.clearColor.x, frame.clearColor.y, frame.clearColor.z, 1.0F);
    api_->clear(kGLColorBufferBit | kGLDepthBufferBit);
    return core::successStatus();
}

core::Status OpenGLRenderer::submit(const RenderView& view) {
    if (!initialized_ || api_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "renderer was not initialized",
                               "opengl renderer");
    }
    if (width_ == 0 || height_ == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "framebuffer size is zero",
                               "opengl renderer");
    }
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    const math::Mat4 viewProjection = view.camera.viewProjectionMatrix(aspect);

    api_->useProgram(program_);
    api_->uniformMatrix4fv(uniformViewProjection_, 1, 0, viewProjection.data());

    for (const RenderItem& item : view.items) {
        if (!item.visible || item.mesh == nullptr) {
            continue;
        }
        const GpuMesh* gpu = nullptr;
        const core::Status status = uploadMesh(*item.mesh, gpu);
        if (!status.ok()) {
            return status;
        }
        if (gpu == nullptr || gpu->indexCount <= 0) {
            continue;
        }
        api_->uniformMatrix4fv(uniformModel_, 1, 0, item.worldMatrix.data());
        const GLfloat color[3] = {item.color.x, item.color.y, item.color.z};
        api_->uniform3fv(uniformColor_, 1, color);

        api_->bindVertexArray(gpu->vertexArray);
        api_->drawElements(kGLTriangles, gpu->indexCount, kGLUnsignedInt,
                           reinterpret_cast<const void*>(static_cast<std::size_t>(0)));
    }
    api_->bindVertexArray(0);
    return core::successStatus();
}

core::Status OpenGLRenderer::endFrame() {
    if (!initialized_ || api_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "renderer was not initialized",
                               "opengl renderer");
    }
    return core::successStatus();
}

core::Status OpenGLRenderer::readbackImage(ImageBuffer& out) const {
    if (!initialized_ || api_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "renderer was not initialized",
                               "opengl renderer");
    }
    ImageBuffer image;
    image.width = width_;
    image.height = height_;
    image.rgba.assign(image.pixelCount() * 4U, 0U);

    api_->pixelStorei(kGLPackAlignment, 1);
    api_->readPixels(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_), kGLRgba, kGLUnsignedByte,
                     image.rgba.data());

    // OpenGL returns rows bottom-up; flip them so the image matches the CPU renderer.
    std::vector<std::uint8_t> flipped(image.rgba.size());
    const std::size_t rowBytes = static_cast<std::size_t>(width_) * 4U;
    for (std::uint32_t row = 0; row < height_; ++row) {
        const std::size_t source = static_cast<std::size_t>(height_ - 1U - row) * rowBytes;
        std::memcpy(flipped.data() + static_cast<std::size_t>(row) * rowBytes, image.rgba.data() + source,
                    rowBytes);
    }
    image.rgba = std::move(flipped);
    out = std::move(image);
    return core::successStatus();
}

core::Status OpenGLRenderer::shutdown() {
    if (api_ != nullptr) {
        releaseResources();
    }
    api_.reset();
    window_ = nullptr;
    initialized_ = false;
    width_ = 0;
    height_ = 0;
    return core::successStatus();
}

void OpenGLRenderer::releaseResources() {
    if (api_ == nullptr) {
        return;
    }
    for (const GpuMesh& mesh : gpuMeshes_) {
        if (mesh.indexBuffer != 0U) {
            api_->deleteBuffers(1, &mesh.indexBuffer);
        }
        if (mesh.vertexBuffer != 0U) {
            api_->deleteBuffers(1, &mesh.vertexBuffer);
        }
        if (mesh.vertexArray != 0U) {
            api_->deleteVertexArrays(1, &mesh.vertexArray);
        }
    }
    gpuMeshes_.clear();
    meshLookup_.clear();
    if (program_ != 0U) {
        api_->deleteProgram(program_);
        program_ = 0U;
    }
}

const RendererCapabilities& OpenGLRenderer::capabilities() const noexcept {
    return capabilities_;
}

bool OpenGLRenderer::isInitialized() const noexcept {
    return initialized_;
}

std::uint32_t OpenGLRenderer::width() const noexcept {
    return width_;
}

std::uint32_t OpenGLRenderer::height() const noexcept {
    return height_;
}

}  // namespace fe::rendering::gl
