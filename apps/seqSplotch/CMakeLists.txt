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
common_application(seqSplotch)
target_include_directories(seqSplotch
  PRIVATE ${PROJECT_SOURCE_DIR}/examples/eqPly ${CMAKE_CURRENT_BINARY_DIR})
