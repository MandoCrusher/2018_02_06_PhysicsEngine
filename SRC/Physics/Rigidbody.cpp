#include "Physics\Rigidbody.h"
#include <Gizmos.h>
#include <glm/vec4.hpp>
#include <glm/ext.hpp>

using namespace Physebs;



Rigidbody::Rigidbody(
	const glm::vec3 & a_pos, float a_mass, float a_frict, 
	bool a_dynamic, const glm::vec4& a_color
) :
	m_pos(a_pos), m_mass(a_mass), m_frict(a_frict), 
	b_dynamic(a_dynamic), m_color(a_color)
{
	// Initialise to null vec3 (0, 0, 0)
	m_vel	= glm::vec3();
	m_accel = glm::vec3();
}

Rigidbody::~Rigidbody()
{
}

/**
*	@brief Modify acceleration with a given force
*	@param a_force is the vector 3 force being applied
*	@return void.
*/
void Rigidbody::ApplyForce(const glm::vec3& a_force)
{
	// f = m * a | a = f / m
	m_accel += a_force / m_mass;
}

/**
*	@brief Instantly apply movement for object instead of using deltaTime and acceleration.
*	@param a_force is the impulse force to enact on the object.
*	@return void.
*/
void Rigidbody::ApplyImpulseForce(const glm::vec3 & a_force)
{
	m_vel += a_force;
}

void Rigidbody::Update(float a_dt)
{
	// Static rigidbodies do not move
	if (b_dynamic) {
		// Apply dampening (negative velocity scaled by friction)
		ApplyForce(-m_vel * m_frict);

		// Calculate velocity
		m_vel += m_accel * a_dt;

		// Truncate velocity with an epsilon
		if (glm::length(m_vel) < EPSILON) {
			// Zero out velocity to avoid floating point errors
			m_vel = glm::vec3();
		}

		// Calculate position
		m_pos += m_vel * a_dt;

		// Reset acceleration so it gets re-calculated
		m_accel = glm::vec3();
	}
}
