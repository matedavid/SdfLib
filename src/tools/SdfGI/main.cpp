#include "SdfLib/utils/Mesh.h"
#include "SdfLib/utils/PrimitivesFactory.h"
#include "SdfLib/utils/Timer.h"
#include "render_engine/shaders/SdfOctreeGIShader.h"
#include "render_engine/shaders/ScreenPlaneShader.h"
#include "render_engine/shaders/BasicShader.h"
#include "render_engine/MainLoop.h"
#include "render_engine/NavigationCamera.h"
#include "render_engine/RenderMesh.h"
#include "render_engine/Window.h"
#include <spdlog/spdlog.h>
#include <args.hxx>
#include <imgui.h>

#include <filesystem>

#include "MitsubaExporter.h"

#include "Texture.h"
#include "Framebuffer.h"

using namespace sdflib;

class MyScene : public Scene
{
public:
    MyScene(std::string modelPath, std::string sdfPath, bool normalizeModel) : mModelPath(modelPath), mSdfPath(sdfPath), mNormalizeModel(normalizeModel) {}

    void start() override
    {
        Window::getCurrentWindow().setBackgroudColor(glm::vec4(0.9, 0.9, 0.9, 1.0));

        auto windowWidth = (uint)Window::getCurrentWindow().getWindowSize().x;
        auto windowHeight = (uint)Window::getCurrentWindow().getWindowSize().y;

        // Depth only pass
        {
            mDepthTexture = std::make_shared<Texture>(Texture::Description{
                .width = windowWidth,
                .height = windowHeight,
                .internalFormat = GL_DEPTH_COMPONENT,
                .format = GL_DEPTH_COMPONENT,
                .pixelDataType = GL_FLOAT,
            });

            mDepthFramebuffer = std::make_shared<Framebuffer>();
            mDepthFramebuffer->bind();

            mDepthFramebuffer->attach(*mDepthTexture, GL_DEPTH_ATTACHMENT);

            mDepthFramebuffer->unbind();

            assert(mDepthFramebuffer->bake());
        }

        // Color pass
        {
            mColorTexture = std::make_shared<Texture>(Texture::Description{
                .width = windowWidth,
                .height = windowHeight,
                .internalFormat = GL_RGB32F,
                .format = GL_RGB,
                .pixelDataType = GL_FLOAT,
            });

            mColorFramebuffer = std::make_shared<Framebuffer>();
            mColorFramebuffer->bind();

            mColorFramebuffer->attach(*mColorTexture, GL_COLOR_ATTACHMENT0);
            mColorFramebuffer->attach(*mDepthTexture, GL_DEPTH_ATTACHMENT);

            mColorFramebuffer->unbind();

            assert(mColorFramebuffer->bake());
        }

        Mesh mesh(mModelPath);
        mModelBBox = mesh.getBoundingBox();

        if (mNormalizeModel)
        {
            // Normalize model units
            const glm::vec3 boxSize = mesh.getBoundingBox().getSize();
            glm::vec3 center = mesh.getBoundingBox().getCenter();
            mesh.applyTransform(glm::scale(glm::mat4(1.0), glm::vec3(2.0f / glm::max(glm::max(boxSize.x, boxSize.y), boxSize.z))) *
                                glm::translate(glm::mat4(1.0), -mesh.getBoundingBox().getCenter()));
        }

        // Load Sdf
        std::unique_ptr<SdfFunction> sdfUnique = SdfFunction::loadFromFile(mSdfPath);
        std::shared_ptr<SdfFunction> sdf = std::move(sdfUnique);
        std::shared_ptr<IOctreeSdf> octreeSdf = std::dynamic_pointer_cast<IOctreeSdf>(sdf);
        if (octreeSdf->hasSdfOnlyAtSurface())
        {
            std::cerr << "The octrees with the isosurface termination rule are not supported in this application" << std::endl;
            exit(1);
        }

        mOctreeGIShader = std::make_unique<SdfOctreeGIShader>(*octreeSdf);
        mScreenPlaneShader = std::make_unique<ScreenPlaneShader>();

        // Model Render
        {
            mModelRenderer = std::make_shared<RenderMesh>();
            mModelRenderer->systemName = "Object Mesh";
            mModelRenderer->start();
            mesh.computeNormals();
            mModelRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                              RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                          mesh.getVertices().data(), mesh.getVertices().size());

            mModelRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                              RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                          mesh.getNormals().data(), mesh.getNormals().size());

            mModelRenderer->setIndexData(mesh.getIndices());
            mModelRenderer->setShader(mOctreeGIShader.get());
            mModelRenderer->callDraw = false; // Disable the automatic call because we already call the function
            mModelRenderer->callDrawGui = false;
            mModelRenderer->systemName = "Mesh Model";
            addSystem(mModelRenderer);
        }

        // Plane Render
        {
            mPlaneRenderer = std::make_shared<RenderMesh>();
            mPlaneRenderer->start();

            // Plane
            std::shared_ptr<Mesh> plane = PrimitivesFactory::getPlane();
            plane->computeNormals();

            mPlaneRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                              RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                          plane->getVertices().data(), plane->getVertices().size());

            mPlaneRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                              RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                          plane->getNormals().data(), plane->getNormals().size());

            mPlaneRenderer->setIndexData(plane->getIndices());
            mPlaneRenderer->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(mesh.getBoundingBox().getCenter().x, mesh.getBoundingBox().min.y, mesh.getBoundingBox().getCenter().z)) *
                                         glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                                         glm::scale(glm::mat4(1.0f), glm::vec3(64.0f)));
            mPlaneRenderer->setShader(mOctreeGIShader.get());
            mPlaneRenderer->callDraw = false; // Disable the automatic call because we already call the function
            mPlaneRenderer->callDrawGui = false;
            mPlaneRenderer->systemName = "Mesh Plane";
            mPlaneRenderer->drawSurface(false);
            addSystem(mPlaneRenderer);
        }

        // Light Renderer
        {
            mLightRenderer = std::make_shared<RenderMesh>();
            mLightRenderer->start();

            std::shared_ptr<Mesh> cube = PrimitivesFactory::getCube();
            cube->computeNormals();

            mLightRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                              RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                          cube->getVertices().data(), cube->getVertices().size());

            mLightRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                              RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                          cube->getNormals().data(), cube->getNormals().size());

            mLightRenderer->setIndexData(cube->getIndices());

            mLightRenderer->setShader(mOctreeGIShader.get());
            mLightRenderer->callDraw = false; // Disable the automatic call because we already call the function
            mLightRenderer->callDrawGui = false;
            mLightRenderer->systemName = "Light Plane";
            addSystem(mLightRenderer);
        }

        // Full screen plane
        {
            std::shared_ptr<Mesh> planeMesh = PrimitivesFactory::getPlane();
            planeMesh->applyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)));

            mScreenPlane = std::make_unique<RenderMesh>();
            mScreenPlane->start();
            mScreenPlane->setIndexData(planeMesh->getIndices());
            mScreenPlane->setVertexData(std::vector<RenderMesh::VertexParameterLayout>{
                                            RenderMesh::VertexParameterLayout(GL_FLOAT, 3)},
                                        planeMesh->getVertices().data(), planeMesh->getVertices().size());

            mScreenPlaneShader->setInputTexture(mColorTexture->id());
            mScreenPlane->setShader(mScreenPlaneShader.get());
        }

        // Create camera
        auto camera = std::make_shared<NavigationCamera>();
        camera->callDrawGui = false;
        // Move camera in the z-axis to be able to see the whole model
        BoundingBox BB = mesh.getBoundingBox();
        float zMovement = 0.5f * glm::max(BB.getSize().x, BB.getSize().y) / glm::tan(glm::radians(0.5f * camera->getFov()));
        // camera->setPosition(glm::vec3(0.0f, 0.0f, 0.1f * BB.getSize().z + zMovement));
        camera->setPosition(glm::vec3(0.8f, 0.6f, 0.0f));
        camera->start();
        setMainCamera(camera);
        addSystem(camera);
        mCamera = camera;
    }

    void update(float deltaTime) override
    {
        auto &wnd = Window::getCurrentWindow();
        const bool shouldStopIndirect = wnd.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

        if (shouldStopIndirect)
            mUseIndirect = false;

        Scene::update(deltaTime);
    }

    void drawModel()
    {
        // Model
        mOctreeGIShader->setMaterial(mAlbedo, mRoughness, mMetallic, mF0);
        mOctreeGIShader->setLightNumber(mLightNumber);

        mOctreeGIShader->setUseSoftShadows(mUseSoftShadows);
        mOctreeGIShader->setMaxShadowIterations(mMaxShadowIterations);

        mOctreeGIShader->setUseIndirect(mUseIndirect);
        mOctreeGIShader->setNumSamples(mNumSamples);
        mOctreeGIShader->setMaxDepth(mMaxDepth);
        mOctreeGIShader->setMaxRaycastIterations(mMaxRaycastIterations);

        mModelRenderer->draw(getMainCamera());
    }

    bool mDrawGui = true;
    virtual void draw() override
    {

        // Depth only pass
        {
            glDepthMask(GL_TRUE);
            mDepthFramebuffer->bind();
            glClear(GL_DEPTH_BUFFER_BIT);

            mOctreeGIShader->setSimple(true);

            drawModel();

            mDepthFramebuffer->unbind();

            glDepthMask(GL_FALSE);
        }

        // Color pass
        {
            mColorFramebuffer->bind();

            glClearColor(0.9, 0.9, 0.9, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);

            glDepthFunc(GL_LEQUAL);

            mOctreeGIShader->setSimple(false);

            // Lights
            for (int i = 0; i < mLightNumber; i++)
            {
                mOctreeGIShader->setLightInfo(i, mLightPosition[i], mLightColor[i], mLightIntensity[i], mLightRadius[i]);

                auto transform = glm::mat4(1.0f);
                transform = glm::translate(transform, mLightPosition[i]);
                transform = glm::scale(transform, glm::vec3(0.10f));

                mLightRenderer->setTransform(transform);
                mOctreeGIShader->setMaterial(mLightColor[i], 0.1, 0.1, glm::vec3(0.04));

                mLightRenderer->draw(getMainCamera());
            }

            drawModel();

            // Plane
            // mOctreeGIShader->setMaterial(glm::vec3(0.7), 0.1, 0.1, glm::vec3(0.07));
            // mPlaneRenderer->draw(getMainCamera());

            glDepthFunc(GL_LESS);
            mColorFramebuffer->unbind();
        }

        // Final pass
        {
            mScreenPlaneShader->bind();
            mScreenPlane->draw(getMainCamera());

            if (mDrawGui)
                drawGui();
            Scene::draw();
        }
    }

    void drawGui()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Scene"))
            {
                ImGui::MenuItem("Show scene settings", NULL, &mShowSceneGUI);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        mCamera->drawGuiWindow();

        if (mShowSceneGUI)
        {
            ImGui::Begin("Scene");
            // Print light settings
            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text("Global Illumination Settings");
            ImGui::Checkbox("Use Indirect", &mUseIndirect);
            ImGui::InputInt("Num Samples", &mNumSamples);
            ImGui::InputInt("Max Depth", &mMaxDepth);
            ImGui::InputInt("Max Raycast Iterations", &mMaxRaycastIterations);

            if (ImGui::Button("Export scene"))
            {
                MitsubaExporter exporter;

                exporter.setCamera(*mCamera);

                const auto modelPath = std::filesystem::current_path() / mModelPath;
                const auto bboxSize = mModelBBox.getSize();

                const auto scale = 2.0f / glm::max(glm::max(bboxSize.x, bboxSize.y), bboxSize.z);
                const auto translate = -mModelBBox.getCenter();

                exporter.addModel({
                    .path = modelPath,
                    .translate = translate,
                    .scale = glm::vec3(scale),
                });

                for (std::size_t i = 0; i < mLightNumber; ++i)
                {
                    exporter.addEmitter({
                        .pos = mLightPosition[i],
                        .intensity = mLightIntensity[i],
                        .radius = mLightRadius[i],
                    });
                }

                exporter.save("mitsuba_scene.xml");
                takeScreenshot();
                std::cout << "Screenshot taken!\n";
            }

            ImGui::Text("Lighting settings");
            ImGui::SliderInt("Lights", &mLightNumber, 1, 4);

            for (int i = 0; i < mLightNumber; ++i)
            { // DOES NOT WORK, PROBLEM WITH REFERENCES
                ImGui::Text("Light %d", i);
                std::string pos = "Position##" + std::to_string(i + 48);
                std::string col = "Color##" + std::to_string(i + 48);
                std::string intens = "Intensity##" + std::to_string(i + 48);
                std::string radius = "Radius##" + std::to_string(i + 48);
                ImGui::InputFloat3(pos.c_str(), reinterpret_cast<float *>(&mLightPosition[i]));
                ImGui::ColorEdit3(col.c_str(), reinterpret_cast<float *>(&mLightColor[i]));
                ImGui::SliderFloat(intens.c_str(), &mLightIntensity[i], 0.0f, 20.0f);
                ImGui::SliderFloat(radius.c_str(), &mLightRadius[i], 0.01f, 1.0f);
            }

            // Print meterial settings
            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text("Material settings");
            ImGui::SliderFloat("Metallic", &mMetallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &mRoughness, 0.0f, 1.0f);
            ImGui::ColorEdit3("Albedo", reinterpret_cast<float *>(&mAlbedo));
            ImGui::ColorEdit3("F0", reinterpret_cast<float *>(&mF0));

            // Print algorithm settings
            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text("Algorithm Settings");
            ImGui::InputInt("Max Shadow Iterations", &mMaxShadowIterations);
            ImGui::Checkbox("Soft Shadows", &mUseSoftShadows);

            // Print model GUI
            ImGui::PushID(mModelRenderer->getSystemId());
            mModelRenderer->drawGui();
            ImGui::PopID();
            ImGui::PushID(mPlaneRenderer->getSystemId());
            mPlaneRenderer->drawGui();
            ImGui::PopID();

            ImGui::End();
        }
    }

    void takeScreenshot()
    {
        auto &window = Window::getCurrentWindow();

        const auto &[width, height] = [&]()
        {
            const auto size = window.getWindowSize();
            return std::make_pair(size.x, size.y);
        }();

        constexpr int screenshotWidth = 1920;
        constexpr int screenshotHeight = 1080;
        window.setWindowSize(glm::ivec2(screenshotWidth, screenshotHeight));

        resize(glm::ivec2(screenshotWidth, screenshotHeight));
        glViewport(0, 0, screenshotWidth, screenshotHeight);
        window.swapBuffers();
        window.update();

        bool prevUseIndirect = mUseIndirect;
        mUseIndirect = true;
        uint32_t prevNumSamples = mNumSamples;
        mNumSamples = 7;
        uint32_t prevMaxDepth = mMaxDepth;
        mMaxDepth = 2;

        mDrawGui = false;

        draw();
        glFinish();

        mDrawGui = true;

        mUseIndirect = prevUseIndirect;
        mNumSamples = prevNumSamples;
        mMaxDepth = prevMaxDepth;

        {
            unsigned char *pixels = new unsigned char[screenshotWidth * screenshotHeight * 3];

            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, screenshotWidth, screenshotHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);

            std::ofstream file("screenshot.ppm");

            file << "P3\n"
                 << screenshotWidth << " " << screenshotHeight << "\n255\n";

            for (int row = screenshotHeight - 1; row >= 0; --row)
            {
                for (int col = 0; col < screenshotWidth; ++col)
                {
                    const auto r = pixels[(row * screenshotWidth + col) * 3 + 0];
                    const auto g = pixels[(row * screenshotWidth + col) * 3 + 1];
                    const auto b = pixels[(row * screenshotWidth + col) * 3 + 2];

                    file << static_cast<int>(r) << " " << static_cast<int>(g) << " " << static_cast<int>(b) << "\n";
                }
            }

            file.close();
        }

        window.setWindowSize(glm::ivec2(width, height));
        resize(glm::ivec2(width, height));
        glViewport(0, 0, width, height);
    }

private:
    std::shared_ptr<NavigationCamera> mCamera;
    std::string mSdfPath;
    std::string mModelPath;
    bool mNormalizeModel;
    std::shared_ptr<RenderMesh> mModelRenderer;
    std::shared_ptr<RenderMesh> mPlaneRenderer;
    std::shared_ptr<RenderMesh> mLightRenderer;

    // Depth pass
    std::shared_ptr<Texture> mDepthTexture;
    std::shared_ptr<Framebuffer> mDepthFramebuffer;

    // Color pass
    std::shared_ptr<Texture> mColorTexture;
    std::shared_ptr<Framebuffer> mColorFramebuffer;

    std::unique_ptr<RenderMesh> mScreenPlane;

    BoundingBox mModelBBox;

    std::unique_ptr<SdfOctreeGIShader> mOctreeGIShader;
    std::unique_ptr<ScreenPlaneShader> mScreenPlaneShader;

    // Options
    int mMaxShadowIterations = 512;
    bool mUseSoftShadows = true;

    // Global Illumination Settings
    bool mUseIndirect = false;
    int mNumSamples = 2;
    int mMaxDepth = 1;
    int mMaxRaycastIterations = 50;

    // Lighting
    int mLightNumber = 1;
    glm::vec3 mLightPosition[4] =
        {
            glm::vec3(1.0f, 2.0f, 1.0f),
            // glm::vec3 (0.0f, 0.2f, -1.5f),
            glm::vec3(-1.0f, 2.0f, 1.0f),
            glm::vec3(1.0f, 2.0f, -1.0f),
            glm::vec3(-1.0f, 2.0f, -1.0f)};

    glm::vec3 mLightColor[4] =
        {
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(1.0f, 0.0f, 1.0f)};

    float mLightIntensity[4] =
        {
            10.0f,
            10.0f,
            10.0f,
            10.0f};

    float mLightRadius[4] =
        {
            0.1f,
            0.1f,
            0.1f,
            0.1f};

    // Material
    float mMetallic = 0.0f;
    float mRoughness = 0.5f;
    glm::vec3 mAlbedo = glm::vec3(0.3f, 0.3f, 0.3f);
    glm::vec3 mF0 = glm::vec3(0.07f, 0.07f, 0.07f);

    // GUI
    bool mShowSceneGUI = false;
};

int main(int argc, char **argv)
{
#ifdef SDFLIB_PRINT_STATISTICS
    spdlog::set_pattern("[%^%l%$] [%s:%#] %v");
#else
    spdlog::set_pattern("[%^%l%$] %v");
#endif

    args::ArgumentParser parser("SdfLight app for rendering a model using its sdf");
    args::HelpFlag help(parser, "help", "Display help menu", {'h', "help"});
    args::Positional<std::string> modelPathArg(parser, "model_path", "The model path");
    args::Positional<std::string> sdfPathArg(parser, "sdf_path", "The sdf model path");
    args::Flag normalizeBBArg(parser, "sdf_is_normalized", "Indicates that the sdf model is normalized", {'n', "normalize"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
        std::cerr << parser;
        return 0;
    }

    MyScene scene(args::get(modelPathArg), args::get(sdfPathArg), (normalizeBBArg) ? true : false);
    MainLoop loop;
    loop.start(scene, "SdfGI");
}