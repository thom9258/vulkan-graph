#!/bin/bash

RESOURCES_DIR="build"

[[ -d ${RESOURCES_DIR} ]] || mkdir ${RESOURCES_DIR}

function compile_vert()
{
  glslc "$1".vert -o ${RESOURCES_DIR}/"$1".vert.spv
  echo "compiled ${SHADER_SOURCE_DIR}/""$1"".vert to ${RESOURCES_DIR}/""$1"".vert.spv"
}

function compile_vert_frag ()
{
  glslc "$1".vert -o ${RESOURCES_DIR}/"$1".vert.spv
  echo "compiled ${SHADER_SOURCE_DIR}/""$1"".vert to ${RESOURCES_DIR}/""$1"".vert.spv"
  glslc "$1".frag -o ${RESOURCES_DIR}/"$1".frag.spv
  echo "compiled ${SHADER_SOURCE_DIR}/""$1"".frag to ${RESOURCES_DIR}/""$1"".frag.spv"
}

compile_vert_frag "geometry"
