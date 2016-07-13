# Copyright (c) 2016 Daniel.Nachbaur@epfl.ch

include(StringifyShaders)

stringify_shaders(
  blur.frag
  blur.vert
  particles.frag
  particles.vert
)

include(zerobufGenerateCxx)
zerobuf_generate_cxx(SEQSPLOTCH ${CMAKE_CURRENT_BINARY_DIR}/serializables
  viewData.fbs
)

list(APPEND SEQSPLOTCH_HEADERS
  application.h
  model.h
  renderer.h
  viewData.h
)

list(APPEND SEQSPLOTCH_SOURCES
  application.cpp
  main.cpp
  model.cpp
  renderer.cpp
  viewData.cpp
  ${SHADER_SOURCES}
)

set(SEQSPLOTCH_LINK_LIBRARIES Sequel splotch ZeroBuf)

if(OSPRAY_FOUND)
  list(APPEND SEQSPLOTCH_HEADERS osprayRenderer.h)
  list(APPEND SEQSPLOTCH_SOURCES osprayRenderer.cpp)
  list(APPEND SEQSPLOTCH_LINK_LIBRARIES ${OSPRAY_LIBRARIES})
endif()

if(TARGET Qt5::Widgets)
  list(APPEND SEQSPLOTCH_LINK_LIBRARIES Qt5::Widgets)
endif()

if(TARGET ZeroEQ)
  list(APPEND SEQSPLOTCH_LINK_LIBRARIES ZeroEQ)
endif()

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
