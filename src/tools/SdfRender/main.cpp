#include "SdfLib/utils/Mesh.h"
#include "SdfLib/utils/PrimitivesFactory.h"
#include "render_engine/RenderSdf.h"
#include "render_engine/MainLoop.h"
#include "render_engine/NavigationCamera.h"
#include "render_engine/RenderMesh.h"
#include "render_engine/Window.h"
#include <spdlog/spdlog.h>
#include <args.hxx>
#include <imgui.h>

using namespace sdflib;

class MyScene : public Scene
{
public:
    MyScene(std::string sdfPath, std::optional<std::string> sdfTricubicPath) : mSdfPath(sdfPath), mSdfTricubicPath(sdfTricubicPath) {}

    void start() override
	{
        Window::getCurrentWindow().setBackgroudColor(glm::vec4(0.9, 0.9, 0.9, 1.0));

        // Create camera
        mCamera = std::make_shared<NavigationCamera>();
        mCamera->start();
        mCamera->callDrawGui = false;
        setMainCamera(mCamera);
        addSystem(mCamera);
		

        BoundingBox sdfBB;

        // Load linear model
        std::unique_ptr<SdfFunction> sdfUnique = SdfFunction::loadFromFile(mSdfPath);
        std::shared_ptr<SdfFunction> sdf = std::move(sdfUnique);
        std::shared_ptr<IOctreeSdf> octreeSdf = std::dynamic_pointer_cast<IOctreeSdf>(sdf);

        // Load tricubic model
        std::shared_ptr<IOctreeSdf> octreeTriSdf(nullptr);
        if(mSdfTricubicPath.has_value())
        {
            std::unique_ptr<SdfFunction> sdfTriUnique = SdfFunction::loadFromFile(mSdfTricubicPath.value());
            std::shared_ptr<SdfFunction> sdfTri = std::move(sdfTriUnique);
            octreeTriSdf = std::dynamic_pointer_cast<IOctreeSdf>(sdfTri);
        }
        
        sdfBB = octreeSdf->getGridBoundingBox();
        glm::vec3 center = sdfBB.getSize();

        SPDLOG_INFO("GridBoundingBox size is {}, {}, {}", center.x, center.y, center.z);

        RenderSdf::Algorithm renderSdfAlgorithm;
        if(!octreeSdf->hasSdfOnlyAtSurface())
        {
            // renderSdfAlgorithm = RenderSdf::Algorithm::SPHERE_TRACING;
            renderSdfAlgorithm = RenderSdf::Algorithm::SPHERE_TRACING_SOLVER;
        }
        else
        {
            renderSdfAlgorithm = RenderSdf::Algorithm::OCTREE_TRAVERSAL_SOLVER;
        }

        mRenderSdf = std::make_shared<RenderSdf>(octreeSdf, renderSdfAlgorithm, octreeTriSdf);
        mRenderSdf->start();
        addSystem(mRenderSdf);

        // Move camera in the z-axis to be able to see the whole model
		{
			float zMovement = 0.5f * glm::max(sdfBB.getSize().x, sdfBB.getSize().y) / glm::tan(glm::radians(0.5f * mCamera->getFov()));
			mCamera->setPosition(glm::vec3(0.0f, 0.0f, 0.1f * sdfBB.getSize().z + zMovement));
		}
    }

    void update(float deltaTime) override
	{
        drawGui();
        Scene::update(deltaTime);
    }

    void drawGui() 
    {
        if (ImGui::BeginMainMenuBar()) 
        {
            if (ImGui::BeginMenu("File")) 
            {
                if (ImGui::MenuItem("Load Sdf")) 
                {
                    strncpy( buf, mSdfPath.c_str(), sizeof(buf)-1 );
                    if(mSdfTricubicPath.has_value()) strncpy(bufTri, mSdfTricubicPath.value().c_str(), sizeof(bufTri) - 1);
                    mShowLoadSdfWindow = true;
                }	
                
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        mCamera->drawGuiWindow();

        if (mShowLoadSdfWindow) {
            ImGui::Begin("Load Sdf");
            ImGui::InputText("Sdf Linear Path", buf, sizeof(buf));
            if(mSdfTricubicPath.has_value()) ImGui::InputText("Sdf Tricubic Path", bufTri, sizeof(bufTri));
            if (ImGui::Button("Load")) 
            {   
                mSdfPath = buf;
                std::unique_ptr<SdfFunction> sdfUnique = SdfFunction::loadFromFile(mSdfPath);
                std::shared_ptr<SdfFunction> sdf = std::move(sdfUnique);
                std::shared_ptr<OctreeSdf> octreeSdf = std::dynamic_pointer_cast<OctreeSdf>(sdf);

                std::shared_ptr<OctreeSdf> octreeTriSdf(nullptr);
                if(mSdfTricubicPath.has_value())
                {
                    mSdfTricubicPath = bufTri;
                    std::unique_ptr<SdfFunction> sdfTriUnique = SdfFunction::loadFromFile(mSdfTricubicPath.value());
                    std::shared_ptr<SdfFunction> sdfTri = std::move(sdfTriUnique);
                    std::shared_ptr<OctreeSdf> octreeTriSdf = std::dynamic_pointer_cast<OctreeSdf>(sdfTri);
                }

                mRenderSdf->setSdf(octreeSdf, RenderSdf::Algorithm::SPHERE_TRACING, octreeTriSdf);
                mShowLoadSdfWindow = false;
            }
            if (ImGui::Button("Cancel")) 
            {       
                mShowLoadSdfWindow = false;
            }
            ImGui::End();
        }
    }

private:
    std::shared_ptr<NavigationCamera> mCamera;
    std::string mSdfPath;
    std::optional<std::string> mSdfTricubicPath;
    std::shared_ptr<RenderSdf> mRenderSdf;
    char buf[255]{};
    char bufTri[255]{};
    bool mShowLoadSdfWindow = false;
    bool mUseIsoSurfaceModels = true;
    int selectedItem = 1;
};

int main(int argc, char** argv)
{
    #ifdef SDFLIB_PRINT_STATISTICS
        spdlog::set_pattern("[%^%l%$] [%s:%#] %v");
    #else
        spdlog::set_pattern("[%^%l%$] %v");
    #endif

    args::ArgumentParser parser("UniformGridViwer reconstructs and draws a uniform grid sdf");
    args::HelpFlag help(parser, "help", "Display help menu", {'h', "help"});
    args::Positional<std::string> modelPathArg(parser, "sdf_path", "The model path");
    args::Positional<std::string> modelTricubicPathArg(parser, "sdftri_path", "The tricubic version of the model");
    
    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch(args::Help)
    {
        std::cerr << parser;
        return 0;
    }

    if(!modelPathArg)
    {
        std::cerr << "Error: No sdf_path specified" << std::endl;
        std::cerr << parser;
        return 1;
    }

    //MyScene scene(args::get(modelPathArg));
    MyScene scene(args::get(modelPathArg), 
                    (modelTricubicPathArg) ? std::optional<std::string>(args::get(modelTricubicPathArg)) : std::optional<std::string>());
    MainLoop loop;
    loop.start(scene, "SdfRender");
}