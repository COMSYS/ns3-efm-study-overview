#! /bin/bash
set -e

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
setup_scripts_path=$parent_path
ns3_path=$parent_path/../ns-3-dev-fork

cp $setup_scripts_path/ns3-3-36-1-changes.patch $ns3_path
cd $ns3_path
git apply ns3-3-36-1-changes.patch

if ! command -v ninja &> /dev/null
then
    echo "ninja could not be found, fallback to Unix Makefiles"
    ./ns3 configure --build-profile=optimized --enable-tests --enable-examples -G "Unix Makefiles"
else
    ./ns3 configure --build-profile=optimized --enable-tests --enable-examples
fi

echo "Start building ns3"
./ns3 build
