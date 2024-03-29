#include "_2018_03_04_PhysicsEngineDemonstrationApp.h"
#include "Gizmos.h"
#include "Input.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Physics\Sphere.h"
#include "Physics\Plane.h"
#include "Physics\Scene.h"
#include "Physics\AABB.h"
#include "Camera\Camera.h"
#include "Physics\Spring.h"
#include "PhysebsUtility_Funcs.h"
#include "tinyxml2/tinyxml2.h"
#include <algorithm>
#include <iostream>
#include <imgui.h>

using aie::Gizmos;
using namespace Physebs;
using namespace tinyxml2;

_2018_03_04_PhysicsEngineDemonstrationApp::_2018_03_04_PhysicsEngineDemonstrationApp() {

}

_2018_03_04_PhysicsEngineDemonstrationApp::~_2018_03_04_PhysicsEngineDemonstrationApp() {

}

bool _2018_03_04_PhysicsEngineDemonstrationApp::startup() {

	setBackgroundColour(0.25f, 0.25f, 0.25f);

	// initialise gizmo primitive counts
	Gizmos::create(100000, 100000, 100000, 100000);

	// Custom camera calibration
	m_camera = new Camera();
	m_camera->SetProjection(glm::radians(45.0f), (float)getWindowWidth() / (float)getWindowHeight(), CAMERA_NEAR, CAMERA_FAR);
	m_camera->SetPosition(glm::vec3(10, 10, 10));
	m_camera->Lookat(glm::vec3(0, 0, 0));

	// Make a scene (ha-ha)
	m_scene = new Scene();
	m_scene->SetGlobalForce(glm::vec3(0.f, 0, 0));
	*m_scene->GetIsPartitionedRef() = false;

	m_scene->LoadScene("./default.scene");

	// Enable v sync for more consistent frames per second in order to analyse performance
	setVSync(true);

	return true;
}

void _2018_03_04_PhysicsEngineDemonstrationApp::shutdown() {

	delete m_camera;
	delete m_scene;

	Gizmos::destroy();
}

void _2018_03_04_PhysicsEngineDemonstrationApp::update(float deltaTime) {

	// wipe the gizmos clean for this frame
	Gizmos::clear();

	// quit if we press escape
	aie::Input* input = aie::Input::getInstance();

#pragma region IMGUI
	/// FPS Window
	ImGui::SetNextWindowSize(ImVec2(200, 50), ImGuiSetCond_Once);
	ImGui::SetNextWindowPos(ImVec2(1100, 0), ImGuiSetCond_Once);
	
	ImGui::Begin("FPS Calculator");

	unsigned int currentFPS = getFPS();
	ImVec4 fpsColor;

	// Set color of FPS depending on what range it's in

	// Healthy FPS [60-infinity] = green
	if (currentFPS >= 60) {
		fpsColor = ImVec4(0, 1, 0, 1);
	}
	// Moderate FPS [25-60) = yellow
	if (currentFPS > 25 && currentFPS < 60) {
		fpsColor = ImVec4(1, 1, 0, 1);
	}
	// Poor FPS [0-25] = red
	if (currentFPS >= 0 && currentFPS <= 25) {
		fpsColor = ImVec4(1, 0, 0, 1);
	}

	ImGui::TextColored(fpsColor, "FPS: %i", currentFPS);
	ImGui::End();

	// Ensure size and position is only set once and ignores whatever is in imgui.cfg
	ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiSetCond_Once);
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Once);
	ImGui::Begin("Physics Engine Interface");

#pragma region Scene Options
	/// Saving and loading
	static char fileName[256] = "default.scene";
	ImGui::InputText("File Name", fileName, 256);

	// Add path to folder where application is being run
	char filePath[256];
	sprintf_s(filePath, "./%s", fileName);

	// Store results of saving and loading as an XML error to precisely identify what went wrong
	XMLError eResult;

	if (ImGui::SmallButton("Save Scene")) {
		
		eResult = m_scene->SaveScene(filePath);
		assert(eResult == XML_SUCCESS && "Failed to Save Scene");
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("Load Scene")) {

		eResult = m_scene->LoadScene(filePath);
		assert(eResult == XML_SUCCESS && "Failed to Load Scene");
	}

	ImGui::NewLine();

	/// Global forces
	ImGui::Text("Global Forces");

	static float	globalForce[3] = { 0.f, 0.f, 0.f };
	static float	gravity = DEFAULT_GRAVITY;

	ImGui::InputFloat3("Scene Global Force", globalForce, 2);
	glm::vec3 currentGlobalForce = glm::vec3(globalForce[0], globalForce[1], globalForce[2]);

	ImGui::InputFloat("Scene Gravity", &gravity, 1.f, 0.f, 3);
	glm::vec3 currentGravityForce = glm::vec3(0.f, gravity, 0.f);

	ImGui::NewLine();

	/// Simulation options
	ImGui::Text("Simulation Options");

	static float	simulationOrigin[3] = { 0.f, 0.f, 0.f };
	static float	simulationExtents[3] = { DEFAULT_SIMULATION_HALFEXTENTS.x, DEFAULT_SIMULATION_HALFEXTENTS.y, DEFAULT_SIMULATION_HALFEXTENTS.z };

	static float	minCellSize[3] = MIN_VOLUME_SIZE;

	ImGui::SliderFloat("Fixed Time Step", m_scene->GetTimeStepRef(), 0.001f, 1.f);

	ImGui::Checkbox("Use Octal Space Partitioning", m_scene->GetIsPartitionedRef());

	// Simulation is using partitioning, show partition options
	if (*(m_scene->GetIsPartitionedRef())) {		// De-reference to get bool

		ImGui::InputFloat3("Simulation Origin", simulationOrigin, 2);
		ImGui::InputFloat3("Simulation Size", simulationExtents, 2);
		m_scene->GetPartitionTree()->SetVolume(simulationOrigin, simulationExtents);

		ImGui::InputFloat3("Minimum Collision Volume", minCellSize, 2);
		m_scene->GetPartitionTree()->SetMinCell(minCellSize);
	}

#pragma endregion

#pragma region Object Creator
	if (ImGui::CollapsingHeader("Object Creator")) {
		/// Make variables static so they're only defined once so their values at their addresses can be changed after by user input
		// Display shape options and store index data
		static int shape = SPHERE;
		ImGui::RadioButton("Sphere", &shape, 0);
		ImGui::RadioButton("Plane", &shape, 1);
		ImGui::RadioButton("AABB", &shape, 2);

		// Has the user created an object this frame
		static bool b_createdObj = false;

		// Universal Rigidbody options
		ImGui::NewLine();
		ImGui::Text("Universal Rigidbody Options");

		static float	pos[3] = { 0.f, 0.f, 0.f };
		static float	force[3] = { 0.f, 0.f, 0.f };
		static float	mass = DEFAULT_MASS;
		static float	friction = DEFAULT_FRICTION;
		static float	restitution = DEFAULT_RESTITUTION;
		static float	color[4] = { 0.f, 0.f, 0.f, 1.f };
		static bool		b_dynamic = true;
		static bool		b_impulse = true;

		ImGui::InputFloat3("Position", pos, 2);
		ImGui::InputFloat3("Starting Force", force, 2);
		ImGui::InputFloat("Mass", &mass, 1.f, 0.f, 2);
		ImGui::InputFloat("Friction", &friction, 1.f, 0.f, 2);
		ImGui::InputFloat("Restitution", &restitution, 1.f, 0.f, 2);
		ImGui::ColorEdit4("Color", color);
		ImGui::Checkbox("Is Dynamic", &b_dynamic);
		ImGui::Checkbox("Velocity is impulse", &b_impulse);

		// Convert input into vectors where necessary
		glm::vec3 currentPos = glm::vec3(pos[0], pos[1], pos[2]);
		glm::vec3 currentForce = glm::vec3(force[0], force[1], force[2]);
		glm::vec4 currentColor = glm::vec4(color[0], color[1], color[2], color[3]);

		ImGui::NewLine();

		/// Create a sphere
		if (shape == SPHERE) {
			// Sphere options
			ImGui::Text("Sphere Options");

			static float dim[2] = { (float)DEFAULT_SPHERE.x, (float)DEFAULT_SPHERE.y };
			static float radius = DEFAULT_MASS;

			ImGui::InputFloat("Radius", &radius, 1.f, 0.f, 2);
			ImGui::InputFloat2("Dimensions", dim, 2);

			/// Create outline of projected sphere from current selected variables
			aie::Gizmos::addSphere(currentPos, radius, DEFAULT_SPHERE.x, DEFAULT_SPHERE.y, glm::vec4(currentColor.r, currentColor.g, currentColor.b, 0.25));

			if (ImGui::SmallButton("Spawn Sphere")) {
				glm::vec2 currentDim = glm::vec2(dim[0], dim[1]);

				m_scene->AddObject(new Sphere(radius, currentDim, currentPos, mass, friction, b_dynamic, currentColor, restitution));
				b_createdObj = true;
			}
		}

		/// Create a plane
		if (shape == PLANE) {
			// Plane options
			ImGui::Text("Plane Options");

			static float normal[3] = { DEFAULT_PLANE_NORMAL.x, DEFAULT_PLANE_NORMAL.y, DEFAULT_PLANE_NORMAL.z };
			static float dist = 0;

			ImGui::InputFloat3("Normal", normal, 2);
			ImGui::InputFloat("Distance From Origin", &dist);

			if (ImGui::SmallButton("Spawn Plane")) {
				glm::vec3 currentNormal = glm::vec3(normal[0], normal[1], normal[2]);
				glm::vec3 currentPlanePos = currentNormal * dist;					// Ignore user input for position and create it from normal and distance

				m_scene->AddObject(new Plane(currentNormal, dist, currentPlanePos, mass, friction, b_dynamic, currentColor, restitution));
				b_createdObj = true;
			}
		}

		/// Create an AABB
		if (shape == AA_BOX) {
			// AABB options
			ImGui::Text("AABB Options");

			static float extents[3] = { DEFAULT_AABB.x, DEFAULT_AABB.y, DEFAULT_AABB.z };
			ImGui::InputFloat3("Extents", extents, 2);

			glm::vec3 currentExtents = glm::vec3(extents[0], extents[1], extents[2]);

			/// Create outline of projected AABB from selected variables
			aie::Gizmos::addAABB(currentPos, currentExtents / 2.f, currentColor);		// Bootstrap treats AABB extents as half extents

			if (ImGui::SmallButton("Spawn AABB")) {
				m_scene->AddObject(new AABB(currentExtents, currentPos, mass, friction, b_dynamic, currentColor, restitution));
				b_createdObj = true;
			}
		}

		// Apply appropriate starting force to last object (if one was added this frame)
		if (b_createdObj) {
			Rigidbody* lastAddedObj = m_scene->GetObjects().back();

			// Apply force instantly
			if (b_impulse) {
				lastAddedObj->ApplyImpulseForce(currentForce);
			}
			// Apply force over time
			else {
				lastAddedObj->ApplyForce(currentForce);
			}

			// Make sure force doesn't keep getting applied
			b_createdObj = false;
		}

	}
#pragma endregion

#pragma region Object Selector
	if (ImGui::CollapsingHeader("Object Selector")) {
		static int selectedObjIndex = 0;		// Always start off looking at the first object

												// There are objects in the scene to select
		if (!m_scene->GetObjects().empty()) {
			// Clamp index with vector constraints to ensure there is no overflow when an object is deleted
			selectedObjIndex = Physebs::Clamp<int>(selectedObjIndex, int(m_scene->GetObjects().size() - 1), 0);

			Rigidbody* currentObj = m_scene->GetObjects()[selectedObjIndex];

			ImGui::Text("OBJECT #%i", selectedObjIndex + 1);

			// Plug references to current universal rigidbody variables into input fields so they update in real-time
			ImGui::Text("Universal Rigidbody Variables");

			ImGui::InputFloat3("Current Position", currentObj->GetPosRef(), 2);
			ImGui::InputFloat("Current Mass", currentObj->GetMassRef(), 1.f, 0.f, 2);
			ImGui::InputFloat("Current Friction", currentObj->GetFrictRef(), 1.f, 0.f, 2);
			ImGui::InputFloat("Current Restitution", currentObj->GetRestitutionRef(), 1.f, 0.f, 2);
			ImGui::ColorEdit4("Current Color", currentObj->GetColorRef());
			ImGui::Checkbox("Current Is Dynamic", currentObj->GetIsDynamicRef());

			ImGui::NewLine();

			float selectionRadius;

			/// Object is sphere, display relevant information
			if (currentObj->GetShape() == SPHERE) {
				ImGui::Text("Sphere Variables");

				Sphere* currentSphere = static_cast<Sphere*>(currentObj);
				selectionRadius = currentSphere->GetRadius() * 2;

				ImGui::InputInt2("Current Dimensions", currentSphere->GetDimensionsRef());
				ImGui::InputFloat("Current Radius", currentSphere->GetRadiusRef(), 1.f, 0.f, 2);
			}

			/// Object is plane, display relevant information
			if (currentObj->GetShape() == PLANE) {
				ImGui::Text("Plane Variables");

				Plane*	currentPlane = static_cast<Plane*>(currentObj);
				selectionRadius = DEFAULT_SELECTION_RADIUS;

				ImGui::InputFloat3("Current Normal", currentPlane->GetNormalRef(), 2);
				ImGui::InputFloat("Current Distance From Origin", currentPlane->GetDistRef(), 1);
			}

			/// Object is AABB, display relevant information
			if (currentObj->GetShape() == AA_BOX) {
				ImGui::Text("AABB Variables");

				AABB*	currentAABB = static_cast<AABB*>(currentObj);
				selectionRadius = glm::length(currentAABB->GetExtents()) / 2.f;

				ImGui::InputFloat3("Current Extents", currentAABB->GetExtentsRef(), 2);
			}

			/// Indicate selection via low-poly sphere at selected object position
			aie::Gizmos::addSphere(currentObj->GetPos(), selectionRadius, DEFAULT_SELECTION_SPHERE.x, DEFAULT_SELECTION_SPHERE.y, DEFAULT_SELECTION_COLOR);

			// User has selected previous object
			if (ImGui::Button("Prev Object")) {

				--selectedObjIndex;
			}

			// Ensures next item is on the same line as the previous item
			ImGui::SameLine();

			// User has selected next object
			if (ImGui::Button("Next Object")) {

				++selectedObjIndex;
			}

			// User wants to delete selected object
			if (ImGui::Button("Delete Object")) {
				// Remove from scene (NOTE: DOES NOT DELETE THE MEMORY)
				m_scene->RemoveObject(currentObj);

				// Delete object allocated memory
				delete currentObj;
			}

		}



	}
#pragma endregion

#pragma region Constraint Creator
	if (ImGui::CollapsingHeader("Constraint Creator")) {
		// There are at least two objects, a constraint can be created
		if (m_scene->GetObjects().size() > 1) {
			// Display constraint type options
			static int constraintType = SPRING;
			ImGui::RadioButton("Spring", &constraintType, 0);

			/// Universal constraint options
			ImGui::NewLine();
			ImGui::Text("Universal Constraint Options");

			static float constraintColor[4] = { DEFAULT_CONSTRAINT_COLOR.r, DEFAULT_CONSTRAINT_COLOR.g, DEFAULT_CONSTRAINT_COLOR.b, DEFAULT_CONSTRAINT_COLOR.a };
			ImGui::ColorEdit4("Constraint Color", constraintColor);

			glm::vec4 currentColor = glm::vec4(constraintColor[0], constraintColor[1], constraintColor[2], constraintColor[3]);

			static int attachedActorIndex = 0;
			static int attachedOtherIndex = 1;		// Starts at one past the actor by default

			ImGui::NewLine();

			ImGui::Text("Rigidbodies to Attach");
#pragma region Mini Actor Selector
			attachedActorIndex = Physebs::Clamp<int>(attachedActorIndex, int(m_scene->GetObjects().size() - 1), 0);		// Ensure selected actor doesn't overflow
			Rigidbody* selectedActor = m_scene->GetObjects()[attachedActorIndex];

			std::string actorShape;

			float actorSelectionRadius;

			if (selectedActor->GetShape() == SPHERE) {
				actorShape = "SPHERE";

				actorSelectionRadius = static_cast<Sphere*>(selectedActor)->GetRadius() * 2.f;
			}
			if (selectedActor->GetShape() == PLANE) {
				actorShape = "PLANE";

				actorSelectionRadius = DEFAULT_SELECTION_RADIUS;
			}
			if (selectedActor->GetShape() == AA_BOX) {
				actorShape = "AABB";

				actorSelectionRadius = glm::length(static_cast<AABB*>(selectedActor)->GetExtents()) / 2.f;
			}

			/// Indicate selection via low-poly sphere at selected actor position
			aie::Gizmos::addSphere(selectedActor->GetPos(), actorSelectionRadius, DEFAULT_SELECTION_SPHERE.x, DEFAULT_SELECTION_SPHERE.y, DEFAULT_ACTOR_SELECTION_COLOR);

			ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s #%i", actorShape.c_str(), attachedActorIndex + 1);
			ImGui::TextColored(ImVec4(1, 0, 0, 1),
				"Actor Position: %f, %f, %f", selectedActor->GetPos().x, selectedActor->GetPos().y, selectedActor->GetPos().z);
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Actor Color: ");

			ImGui::SameLine();

			ImGui::TextColored(
				ImVec4(selectedActor->GetColor().r, selectedActor->GetColor().g, selectedActor->GetColor().b, selectedActor->GetColor().a),
				"%f, %f, %f", selectedActor->GetColor().x, selectedActor->GetColor().y, selectedActor->GetColor().z
			);
			ImGui::TextColored(ImVec4(1, 0, 0, 1),
				selectedActor->GetIsDynamic() ? "Actor Is Dynamic: TRUE" : "Actor Is Dynamic: FALSE");

			// Cycle to previous actor
			if (ImGui::SmallButton("Prev Actor")) {
				--attachedActorIndex;
			}

			ImGui::SameLine();

			// Cycle to next actor
			if (ImGui::SmallButton("Next Actor")) {
				++attachedActorIndex;
			}
#pragma endregion

#pragma region Mini Other Selector
			/// Other selector
			attachedOtherIndex = Physebs::Clamp<int>(attachedOtherIndex, int(m_scene->GetObjects().size() - 1), 0);		// Ensure selected actor doesn't overflow

			Rigidbody* selectedOther = m_scene->GetObjects()[attachedOtherIndex];

			std::string otherShape;

			float otherSelectionRadius;

			if (selectedOther->GetShape() == SPHERE) {
				otherShape = "SPHERE";

				otherSelectionRadius = static_cast<Sphere*>(selectedOther)->GetRadius() * 2.f;
			}
			if (selectedOther->GetShape() == PLANE) {
				otherShape = "PLANE";

				otherSelectionRadius = DEFAULT_SELECTION_RADIUS;
			}
			if (selectedOther->GetShape() == AA_BOX) {
				otherShape = "AABB";

				otherSelectionRadius = glm::length(static_cast<AABB*>(selectedOther)->GetExtents()) / 2.f;
			}

			/// Indicate selection via low-poly sphere at selected actor position
			aie::Gizmos::addSphere(selectedOther->GetPos(), otherSelectionRadius, DEFAULT_SELECTION_SPHERE.x, DEFAULT_SELECTION_SPHERE.y, DEFAULT_OTHER_SELECTION_COLOR);

			ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s #%i", otherShape.c_str(), attachedOtherIndex + 1);
			ImGui::TextColored(ImVec4(0, 0, 1, 1),
				"Other Position: %f, %f, %f", selectedOther->GetPos().x, selectedOther->GetPos().y, selectedOther->GetPos().z);
			ImGui::TextColored(ImVec4(0, 0, 1, 1), "Other Color: ");

			ImGui::SameLine();

			ImGui::TextColored(
				ImVec4(selectedOther->GetColor().r, selectedOther->GetColor().g, selectedOther->GetColor().b, selectedOther->GetColor().a),
				"%f, %f, %f", selectedOther->GetColor().x, selectedOther->GetColor().y, selectedOther->GetColor().z);
			ImGui::TextColored(ImVec4(0, 0, 1, 1),
				selectedOther->GetIsDynamic() ? "Other Is Dynamic: TRUE" : "Other Is Dynamic: FALSE");

			// Cycle to previous other
			if (ImGui::SmallButton("Prev Other")) {
				--attachedOtherIndex;
			}

			ImGui::SameLine();

			// Cycle to next actor
			if (ImGui::SmallButton("Next Other")) {
				++attachedOtherIndex;
			}
#pragma endregion

			/// Constraint-type specific attributes
			ImGui::NewLine();

			// Display spring attributes
			if (constraintType == SPRING) {
				ImGui::Text("Spring Options");

				static float springiness = DEFAULT_SPRINGINESS;
				static float restLength = DEFAULT_SPRING_LENGTH;
				static float dampening = DEFAULT_FRICTION;

				ImGui::InputFloat("Springiness", &springiness, 1.f);
				ImGui::InputFloat("Rest Length", &restLength, 1.f);
				ImGui::InputFloat("Dampening", &dampening, 1.f);

				// User wants to create spring
				if (ImGui::SmallButton("Attach Spring")) {
					m_scene->AddConstraint(new Spring(selectedActor, selectedOther, currentColor, springiness, restLength, dampening));
				}
			}
		}



	}

#pragma endregion

#pragma region Constraint Selector
	if (ImGui::CollapsingHeader("Constraint Selector")) {

		static int selectedConstraintIndex = 0;

		// There are constraints in the scene to select
		if (m_scene->GetConstraints().size() != 0) {
			// Clamp index by vector constraints
			selectedConstraintIndex = Physebs::Clamp<int>(selectedConstraintIndex, int(m_scene->GetConstraints().size() - 1), 0);
			ImGui::Text("CONSTRAINT #%i", selectedConstraintIndex + 1);

			Constraint* currentConstraint = m_scene->GetConstraints()[selectedConstraintIndex];

			/// Create selection spheres at the position of each attached Rigidbody
			float attachedActorSelectionRadius = DEFAULT_SELECTION_RADIUS;
			float attachedOtherSelectionRadius = DEFAULT_SELECTION_RADIUS;

			if (currentConstraint->GetAttachedActor()->GetShape() == SPHERE) {
				attachedActorSelectionRadius = static_cast<Sphere*>(currentConstraint->GetAttachedActor())->GetRadius() * 2.f;
			}
			if (currentConstraint->GetAttachedOther()->GetShape() == SPHERE) {
				attachedOtherSelectionRadius = static_cast<Sphere*>(currentConstraint->GetAttachedOther())->GetRadius() * 2.f;
			}

			if (currentConstraint->GetAttachedActor()->GetShape() == AA_BOX) {
				attachedActorSelectionRadius = glm::length(static_cast<AABB*>(currentConstraint->GetAttachedActor())->GetExtents()) / 2.f;
			}
			if (currentConstraint->GetAttachedOther()->GetShape() == AA_BOX) {
				attachedOtherSelectionRadius = glm::length(static_cast<AABB*>(currentConstraint->GetAttachedOther())->GetExtents()) / 2.f;
			}

			aie::Gizmos::addSphere(currentConstraint->GetAttachedActor()->GetPos(),
				attachedActorSelectionRadius, DEFAULT_SELECTION_SPHERE.x, DEFAULT_SELECTION_SPHERE.x, DEFAULT_CONSTRAINT_SELECTION_COLOR);
			aie::Gizmos::addSphere(currentConstraint->GetAttachedOther()->GetPos(),
				attachedOtherSelectionRadius, DEFAULT_SELECTION_SPHERE.x, DEFAULT_SELECTION_SPHERE.x, DEFAULT_CONSTRAINT_SELECTION_COLOR);

			// Display editable properties of selected constraint
			/// Universal
			ImGui::Text("Universal Constraint Variables");

			ImGui::ColorEdit4("Current Constraint Color", currentConstraint->GetColorRef());

			/// Specific to type
			ImGui::NewLine();

			if (currentConstraint->GetType() == SPRING) {
				ImGui::Text("Spring Variables");

				Spring* currentSpring = static_cast<Spring*>(currentConstraint);

				ImGui::InputFloat("Current Springiness", currentSpring->GetSpringinessRef(), 1.f);
				ImGui::InputFloat("Current Rest Length", currentSpring->GetRestLengthRef(), 1.f);
				ImGui::InputFloat("Current Dampening", currentSpring->GetDampeningRef(), 1.f);
			}

			// Cycle to previous constraint
			if (ImGui::SmallButton("Prev Constraint")) {
				--selectedConstraintIndex;
			}

			ImGui::SameLine();

			// Cycle to next constraint
			if (ImGui::SmallButton("Next Constraint")) {
				++selectedConstraintIndex;
			}

			// User wants to remove constraint from scene
			if (ImGui::SmallButton("Delete Constraint")) {
				m_scene->RemoveConstraint(currentConstraint);

				// Delete dynamically allocated memory to avoid memory leaks
				delete currentConstraint;
			}
		}
	}


#pragma endregion

	ImGui::End();
#pragma endregion

	/// Scene
	m_scene->SetGlobalForce(currentGlobalForce);
	m_scene->SetGravity(currentGravityForce);
	m_scene->ApplyGlobalForce();
	m_scene->FixedUpdate(deltaTime);

	/// Camera
	m_camera->Update(deltaTime);

#pragma region 3D Template Code
	// draw a simple grid with gizmos
	glm::vec4 white(1);
	glm::vec4 black(0, 0, 0, 1);
	for (int i = 0; i < 21; ++i) {
		Gizmos::addLine(glm::vec3(-10 + i, 0, 10),
			glm::vec3(-10 + i, 0, -10),
			i == 10 ? white : black);
		Gizmos::addLine(glm::vec3(10, 0, -10 + i),
			glm::vec3(-10, 0, -10 + i),
			i == 10 ? white : black);
	}

	// add a transform so that we can see the axis
	Gizmos::addTransform(glm::mat4(1));

#pragma endregion

	if (input->isKeyDown(aie::INPUT_KEY_ESCAPE))
		quit();
}

void _2018_03_04_PhysicsEngineDemonstrationApp::draw() {

	// wipe the screen to the background colour
	clearScreen();

	/// Scene
	m_scene->Draw();

	Gizmos::draw(m_camera->GetProjectionView());

}