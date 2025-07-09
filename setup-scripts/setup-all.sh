#! /bin/bash
set -e

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
ns3_path=$parent_path/../ns-3-dev-fork

if [ ! -f "$ns3_path/ns3" ]; then
    echo "Could not find $ns3_path/ns3. Are you sure you have cloned ns3? For example, use this: git submodule update --init --recursive"
    exit;
fi

echo "Start setting up simulation."
bash "$parent_path/setup-sim.sh"

echo "Done setting up simulation."
echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
echo "Start setting up result processing."

bash "$parent_path/setup-result-processing.sh"