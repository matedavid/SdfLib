#include <iostream>
#include <random>
#include <algorithm>
#include "render_engine/MainLoop.h"
#include "render_engine/NavigationCamera.h"
#include "render_engine/RenderMesh.h"
#include "render_engine/shaders/NormalsShader.h"
#include "render_engine/shaders/SdfPlaneShader.h"
#include "render_engine/shaders/BasicShader.h"
#include "render_engine/shaders/NormalsSplitPlaneShader.h"
#include "render_engine/Window.h"
#include "utils/Mesh.h"
#include "utils/TriangleUtils.h"
#include "utils/PrimitivesFactory.h"
#include "utils/Timer.h"

#include <spdlog/spdlog.h>
#include <args.hxx>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

class MyScene : public Scene
{
public:
	void start() override
	{
		Window::getCurrentWindow().setBackgroudColor(glm::vec4(0.9, 0.9, 0.9, 1.0));

		auto camera = std::make_shared<NavigationCamera>();
		camera->start();
		setMainCamera(camera);
		addSystem(camera);

		mPlaneRenderer = std::make_shared<RenderMesh>();
		mPlaneRenderer->start();

		// Plane
		std::shared_ptr<Mesh> plane = PrimitivesFactory::getPlane();

		mPlaneRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, plane->getVertices().data(), plane->getVertices().size());

		mPlaneRenderer->setIndexData(plane->getIndices());

		Mesh sphereMesh("../models/sphere.glb");
		BoundingBox box = sphereMesh.getBoudingBox();
		box.addMargin(0.5f);

		Timer timer; timer.start();
		UniformGridSdf sdfGrid(sphereMesh, box, 0.1f);
		SPDLOG_INFO("Uniform grid generation time: {}s", timer.getElapsedSeconds());

		mGizmoStartMatrix = glm::translate(glm::mat4x4(1.0f), sdfGrid.getGridBoundingBox().getCenter()) *
							glm::scale(glm::mat4x4(1.0f), 2.0f * sdfGrid.getGridBoundingBox().getSize());
		mGizmoMatrix = mGizmoStartMatrix;
		mPlaneRenderer->setTransform(mGizmoMatrix);

		mPlaneShader = std::unique_ptr<SdfPlaneShader> (new SdfPlaneShader(sdfGrid));
		mPlaneRenderer->setShader(mPlaneShader.get());
		addSystem(mPlaneRenderer);
		mPlaneRenderer->callDrawGui = false;

		auto sphereMeshRenderer = std::make_shared<RenderMesh>();
		sphereMeshRenderer->start();
		sphereMeshRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, sphereMesh.getVertices().data(), sphereMesh.getVertices().size());
							
		sphereMeshRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, sphereMesh.getNormals().data(), sphereMesh.getNormals().size());

		sphereMeshRenderer->setIndexData(sphereMesh.getIndices());
		sphereMeshRenderer->setShader(Shader<NormalsShader>::getInstance());
		addSystem(sphereMeshRenderer);
		sphereMeshRenderer->drawSurface(false);

		std::shared_ptr<Mesh> cubeMesh = PrimitivesFactory::getCube();
		mCubeRenderer = std::make_shared<RenderMesh>();
		mCubeRenderer->start();
		mCubeRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, cubeMesh->getVertices().data(), cubeMesh->getVertices().size());

		cubeMesh->computeNormals();	
		mCubeRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, cubeMesh->getNormals().data(), cubeMesh->getNormals().size());

		mCubeRenderer->setIndexData(cubeMesh->getIndices());

		mCubeShader = std::unique_ptr<NormalsSplitPlaneShader>(new NormalsSplitPlaneShader(glm::vec4(0.0, 0.0, 1.0, 0.0)));

		mCubeRenderer->setShader(mCubeShader.get());

		mCubeRenderer->setTransform(glm::translate(glm::mat4x4(1.0f), sdfGrid.getGridBoundingBox().getCenter()) *
					   				glm::scale(glm::mat4x4(1.0f), sdfGrid.getGridBoundingBox().getSize()));
		addSystem(mCubeRenderer);
	}

	void update(float deltaTime) override
	{
		Scene::update(deltaTime);

		ImGuiIO& io = ImGui::GetIO();
    	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

		if(Window::getCurrentWindow().isKeyPressed(GLFW_KEY_1))
		{
			mGizmoMatrix = mGizmoStartMatrix;
		} 
		else if(Window::getCurrentWindow().isKeyPressed(GLFW_KEY_2))
		{
			mGizmoMatrix = glm::rotate(glm::mat4x4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * mGizmoStartMatrix;
		} 
		else if(Window::getCurrentWindow().isKeyPressed(GLFW_KEY_3))
		{
			mGizmoMatrix = glm::rotate(glm::mat4x4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * mGizmoStartMatrix;
		}

		if(Window::getCurrentWindow().isKeyPressed(GLFW_KEY_LEFT_ALT))
		{
			ImGuizmo::Manipulate(glm::value_ptr(getMainCamera()->getViewMatrix()), 
							 glm::value_ptr(getMainCamera()->getProjectionMatrix()),
							 ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(mGizmoMatrix));
		}
		else
		{
			ImGuizmo::Manipulate(glm::value_ptr(getMainCamera()->getViewMatrix()), 
							 glm::value_ptr(getMainCamera()->getProjectionMatrix()),
							 ImGuizmo::OPERATION::TRANSLATE_Z, ImGuizmo::MODE::LOCAL, glm::value_ptr(mGizmoMatrix));
		}
		
		mPlaneRenderer->setTransform(mGizmoMatrix);
		glm::vec3 planeNormal = glm::normalize(glm::vec3(mGizmoMatrix * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
		glm::vec3 planePoint = glm::vec3(mGizmoMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		mCubeShader->setCutPlane(glm::vec4(planeNormal.x, planeNormal.y, planeNormal.z, -glm::dot(planeNormal, planePoint)));
		mPlaneShader->setNormal(planeNormal);
	}
private:
	std::unique_ptr<SdfPlaneShader> mPlaneShader;
	std::unique_ptr<NormalsSplitPlaneShader> mCubeShader;

	std::shared_ptr<RenderMesh> mPlaneRenderer; 
	std::shared_ptr<RenderMesh> mCubeRenderer;

	glm::mat4x4 mGizmoStartMatrix;
	glm::mat4x4 mGizmoMatrix;
};

class TestScene : public Scene
{
	void start() override
	{
		Window::getCurrentWindow().setBackgroudColor(glm::vec4(0.9, 0.9, 0.9, 1.0));

		auto camera = std::make_shared<NavigationCamera>();
		camera->start();
		setMainCamera(camera);
		addSystem(camera);

		std::shared_ptr<Mesh> cubeMesh = PrimitivesFactory::getCube();
		mCubeRenderer = std::make_shared<RenderMesh>();
		mCubeRenderer->start();
		mCubeRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, cubeMesh->getVertices().data(), cubeMesh->getVertices().size());

		cubeMesh->computeNormals();	
		mCubeRenderer->setVertexData(std::vector<RenderMesh::VertexParameterLayout> {
									RenderMesh::VertexParameterLayout(GL_FLOAT, 3)
							}, cubeMesh->getNormals().data(), cubeMesh->getNormals().size());

		mCubeRenderer->setIndexData(cubeMesh->getIndices());
		mCubeRenderer->setShader(Shader<NormalsShader>::getInstance());
		addSystem(mCubeRenderer);
	}

	void update(float deltaTime) override
	{
		Scene::update(deltaTime);

		ImGuiIO& io = ImGui::GetIO();
    	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

		if(Window::getCurrentWindow().isKeyPressed(GLFW_KEY_LEFT_ALT))
		{
			ImGuizmo::Manipulate(glm::value_ptr(getMainCamera()->getViewMatrix()), 
							 glm::value_ptr(getMainCamera()->getProjectionMatrix()),
							 ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(mGizmoMatrix));
		}
		else
		{
			ImGuizmo::Manipulate(glm::value_ptr(getMainCamera()->getViewMatrix()), 
							 glm::value_ptr(getMainCamera()->getProjectionMatrix()),
							 ImGuizmo::OPERATION::TRANSLATE_Z, ImGuizmo::MODE::LOCAL, glm::value_ptr(mGizmoMatrix));
		}
		
		mCubeRenderer->setTransform(mGizmoMatrix);
	}

private:
	glm::mat4x4 mGizmoMatrix = glm::mat4(1.0f);
	std::shared_ptr<RenderMesh> mCubeRenderer;
};

int main()
{
	spdlog::set_pattern("[%^%l%$] [%s:%#] %v");


    MyScene scene;
	// TestScene scene;
    MainLoop loop;
    loop.start(scene);
}