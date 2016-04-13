# Copyright (c) 2016 Daniel.Nachbaur@epfl.ch

include(StringifyShaders)

stringify_shaders(
  FBO_Passthrough.frag
  FBO_Passthrough.vert
  FBO_ToneMap.frag
  FBO_ToneMap.vert
  PP_FBO.frag
  PP_FBO.geom
  PP_FBO.vert
  blur.frag
  blur_pass.vert
  particles.frag
  particles.vert
)

set(SEQSPLOTCH_HEADERS
  application.h
  renderer.h
)

set(SEQSPLOTCH_SOURCES
  application.cpp
  renderer.cpp
  main.cpp
  ${SHADER_SOURCES}
)

file(COPY particle.tga DESTINATION ${CMAKE_BINARY_DIR}/share/seqSplotch/data)
install(FILES particle.tga DESTINATION share/seqSplotch/data COMPONENT apps)

set(SEQSPLOTCH_LINK_LIBRARIES Sequel splotch)
if(TARGET splotchCUDA)
  common_find_package(CUDA)
  set(CUDA_HOST_COMPILER "/usr/bin/gcc")
  list(APPEND CUDA_NVCC_FLAGS "-std=c++11 -arch=sm_30 -dc")
  #set(CUDA_PROPAGATE_HOST_FLAGS OFF)
  cuda_add_executable(seqSplotch ${SEQSPLOTCH_HEADERS} ${SEQSPLOTCH_SOURCES})
  target_compile_definitions(seqSplotch PRIVATE CUDA=1)
  target_link_libraries(seqSplotch ${SEQSPLOTCH_LINK_LIBRARIES} splotchCUDA)
  set_property(GLOBAL APPEND PROPERTY ${PROJECT_NAME}_ALL_DEP_TARGETS seqSplotch)
else()
  common_application(seqSplotch)
endif()
target_include_directories(seqSplotch PRIVATE ${CMAKE_CURRENT_BINARY_DIR})