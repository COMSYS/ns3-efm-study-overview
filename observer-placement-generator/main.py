from argparse import ArgumentParser
from op_manager import ObserverPlacementManager

if __name__ == '__main__':
    parser = ArgumentParser(description="Observer Placement Generator")
    parser.add_argument("target_topology", help="Target topology name")
    parser.add_argument("--target_topology_file_path", "-f", help="Target topology file path if mapping should not be used")
    parser.add_argument("--op_file", "-o", help="Observer placement file path")
    args = parser.parse_args()

    if args.target_topology_file_path is not None:
        manager = ObserverPlacementManager(args.op_file, {args.target_topology: args.target_topology_file_path})
    else:
        manager = ObserverPlacementManager(args.op_file)
    
    manager.generate_all_placements({args.target_topology}, True)

