#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <numbers>

class OrbitCamera {
public:
  OrbitCamera(const glm::vec3 center, const float radius);

  [[nodiscard]]
  float radius() const;
  void set_radius(const float r);
  void add_radius(const float dr);

  [[nodiscard]]
  float rotation_x() const;
  [[nodiscard]]
  float rotation_z() const;
  [[nodiscard]]
  std::pair<float, float> rotation() const;
  void set_rotation(const float rx, const float rz);
  void add_rotation(const float drx, const float drz);

  [[nodiscard]]
  glm::mat4 camera() const;
  [[nodiscard]]
  glm::vec3 position() const;
  [[nodiscard]]
  glm::mat4 view() const;

private:
  void phi_clamp();

  glm::vec3 m_center;
  float m_radius;
  float m_phi;   // x rotation
  float m_theta; // z rotation

  struct phi_limits {
    static constexpr float offset = 0.05f * std::numbers::pi_v<float>;
    float min{0 + offset};
    float max{std::numbers::pi_v<float> - offset};
  } m_phi_limits;
};

OrbitCamera::OrbitCamera(const glm::vec3 center, const float radius)
    : m_center(center), m_radius(radius), m_phi(std::numbers::pi_v<float> * 0.25f),
      m_theta(0) {}

void OrbitCamera::phi_clamp() {
  m_phi = std::clamp(m_phi, m_phi_limits.min, m_phi_limits.max);
}

float OrbitCamera::radius() const { return m_radius; }

void OrbitCamera::set_radius(const float r) { m_radius = r; }

void OrbitCamera::add_radius(const float dr) { m_radius += dr; }

float OrbitCamera::rotation_x() const { return m_phi; }

float OrbitCamera::rotation_z() const { return m_theta; }

std::pair<float, float> OrbitCamera::rotation() const {
  return {rotation_x(), rotation_z()};
}

void OrbitCamera::set_rotation(const float rx, const float rz) {
  m_phi = rx;
  phi_clamp();
  m_theta = rz;
}

void OrbitCamera::add_rotation(const float drx, const float drz) {
  m_phi = m_phi + drx;
  phi_clamp();
  m_theta += drz;
}

glm::vec3 OrbitCamera::position() const {
  const float camX = m_radius * std::sin(m_phi) * std::cos(m_theta);
  const float camY = m_radius * std::cos(m_phi);
  const float camZ = m_radius * std::sin(m_phi) * std::sin(m_theta);
  return glm::vec3(camX, camY, camZ);
}

glm::mat4 OrbitCamera::view() const {
  const auto up = glm::vec3(0.0, 1.0, 0.0);
  return glm::lookAt(position(), m_center, up);
}
