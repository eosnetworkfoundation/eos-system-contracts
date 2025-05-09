#!/usr/bin/env bash
set -eo pipefail

function usage() {
  printf "Usage: $0 OPTION...
  -c DIR      Path to CDT installation/build directory. (Optional if using CDT installled at standard system location.)
  -l DIR      Path to Spring build directory. Optional, but must be specified to build tests.
  -h          Print this help menu.
  \\n" "$0" 1>&2
  exit 1
}

BUILD_TESTS=OFF

if [ $# -ne 0 ]; then
  while getopts "c:l:h" opt; do
    case "${opt}" in
      c )
        CDT_INSTALL_DIR=$(realpath $OPTARG)
      ;;
      l )
        SPRING_BUILD_DIR=$(realpath $OPTARG)
        BUILD_TESTS=ON
      ;;
      h )
        usage
      ;;
      ? )
        echo "Invalid Option!" 1>&2
        usage
      ;;
      : )
        echo "Invalid Option: -${OPTARG} requires an argument." 1>&2
        usage
      ;;
      * )
        usage
      ;;
    esac
  done
fi

SPRING_DIR_CMAKE_OPTION=''

if [[ "${BUILD_TESTS}" == "ON" ]]; then
  if [[ ! -f "$SPRING_BUILD_DIR/lib/cmake/spring/spring-config.cmake" ]]; then
    echo "Invalid path to Spring build directory: $SPRING_BUILD_DIR"
    echo "Spring build directory is required to build tests. If you do not wish to build tests, leave off the -l option."
    echo "Cannot proceed. Exiting..."
    exit 1;
  fi

  echo "Using Spring build directory at: $SPRING_BUILD_DIR"
  echo ""
  SPRING_DIR_CMAKE_OPTION="-Dspring_DIR=${SPRING_BUILD_DIR}/lib/cmake/spring"
fi

CDT_DIR_CMAKE_OPTION=''

if [[ -z $CDT_INSTALL_DIR ]]; then
  echo "No CDT location was specified. Assuming installed in standard location."
  echo ""
else
  if [[ ! -f "$CDT_INSTALL_DIR/lib/cmake/cdt/cdt-config.cmake" ]]; then
    echo "Invalid path to CDT installation/build directory: $CDT_INSTALL_DIR"
    echo "If CDT is installed at the standard system location, then you do not need to use the -c option."
    echo "Cannot proceed. Exiting..."
    exit 1;
  fi

  echo "Using CDT installation/build at: $CDT_INSTALL_DIR"
  echo ""
  CDT_DIR_CMAKE_OPTION="-Dcdt_DIR=${CDT_INSTALL_DIR}/lib/cmake/cdt"
fi

printf "\t=========== Building system-contracts ===========\n\n"
RED='\033[0;31m'
NC='\033[0m'
CPU_CORES=$(getconf _NPROCESSORS_ONLN)
mkdir -p build
pushd build &> /dev/null
cmake -DBUILD_TESTS=${BUILD_TESTS} ${SPRING_DIR_CMAKE_OPTION} ${CDT_DIR_CMAKE_OPTION} ../
make -j $CPU_CORES
popd &> /dev/null
