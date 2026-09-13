# GLSL shader'ları build sırasında SPIR-V'ye derler.
# Öncelik glslc (shaderc), yoksa glslangValidator (glslang-tools).

find_program(METRO_GLSLC_EXE glslc)
find_program(METRO_GLSLANG_EXE glslangValidator)

if(METRO_GLSLC_EXE)
  set(METRO_SHADER_COMPILER "${METRO_GLSLC_EXE}")
  set(METRO_SHADER_FLAGS --target-env=vulkan1.3)
elseif(METRO_GLSLANG_EXE)
  set(METRO_SHADER_COMPILER "${METRO_GLSLANG_EXE}")
  set(METRO_SHADER_FLAGS -V --target-env vulkan1.3)
else()
  message(FATAL_ERROR "Shader derleyicisi yok: glslc (shaderc) veya glslangValidator (glslang-tools) kurun.")
endif()

function(metro_add_shader_target)
  set(outputs)
  foreach(src IN LISTS ARGN)
    get_filename_component(abs "${src}" ABSOLUTE)
    get_filename_component(name "${abs}" NAME)
    set(out "${CMAKE_BINARY_DIR}/shaders/${name}.spv")
    add_custom_command(
      OUTPUT "${out}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
      COMMAND "${METRO_SHADER_COMPILER}" ${METRO_SHADER_FLAGS} "${abs}" -o "${out}"
      DEPENDS "${abs}"
      COMMENT "SPIR-V: ${name}"
      VERBATIM)
    list(APPEND outputs "${out}")
  endforeach()
  add_custom_target(metro_shaders DEPENDS ${outputs})
endfunction()
