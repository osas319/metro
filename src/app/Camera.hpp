#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace metro::app {

struct Camera {
  glm::vec3 position{0.0f, 0.0f, 3.0f};
  float pitch = 0.0f;
  float yaw = -90.0f; // -Z'ye baksın

  float moveSpeed = 5.0f;
  float mouseSensitivity = 0.1f;

  glm::vec3 getFront() const {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(front);
  }

  glm::vec3 getRight() const {
    return glm::normalize(glm::cross(getFront(), glm::vec3(0.0f, 1.0f, 0.0f)));
  }

  glm::vec3 getUp() const {
    return glm::normalize(glm::cross(getRight(), getFront()));
  }

  glm::mat4 getViewMatrix() const {
    return glm::lookAt(position, position + getFront(), glm::vec3(0.0f, 1.0f, 0.0f));
  }

  glm::mat4 getProjectionMatrix(float aspect) const {
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
    // Vulkan Y is down, GLM uses OpenGL's Y up
    proj[1][1] *= -1;
    return proj;
  }
};

} // namespace metro::app
