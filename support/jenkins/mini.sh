#!/usr/bin/env bash

set -o errexit -o nounset -o pipefail -o verbose

CURRENT_DIR="$(cd "$(dirname "$0")"; pwd -P)"
SUPPORT_DIR="${CURRENT_DIR}/.."

: ${USERNAME:?"Environment variable 'USERNAME' must be set to the username of the 'Mesos DockerBot' Docker hub account."}
: ${PASSWORD:?"Environment variable 'PASSWORD' must be set to the password of the 'Mesos DockerBot' Docker hub account."}

DOCKER_IMAGE_MINI=${DOCKER_IMAGE_MINI:-"mesos/mesos-mini"}
DOCKER_IMAGE_PACKAGING=${DOCKER_IMAGE_PACKAGING:-"mesos/mesos-centos-packaging"}
DOCKER_IMAGE_DISTRO=${DOCKER_IMAGE_DISTRO:-"mesos/mesos-centos"}
DOCKER_IMAGE_TAG=`date +%F`

function cleanup {
  docker rmi $(docker images -q ${DOCKER_IMAGE_PACKAGING}:${DOCKER_IMAGE_TAG}) || true
  docker rmi $(docker images -q ${DOCKER_IMAGE_DISTRO}:${DOCKER_IMAGE_TAG}) || true
  docker rmi $(docker images -q ${DOCKER_IMAGE_MINI}:${DOCKER_IMAGE_TAG}) || true
}

trap cleanup EXIT

DOCKER_IMAGE_MINI=${DOCKER_IMAGE_MINI} \
DOCKER_IMAGE_PACKAGING=${DOCKER_IMAGE_PACKAGING} \
DOCKER_IMAGE_DISTRO=${DOCKER_IMAGE_DISTRO} \
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG} \
"${SUPPORT_DIR}/mesos-mini/build.sh"

docker login -u ${USERNAME} -p ${PASSWORD}
docker push ${DOCKER_IMAGE_MINI}:${DOCKER_IMAGE_TAG}
