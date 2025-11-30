#!/usr/bin/env python3
"""
Generate large-scale dependency graph test cases for PubGrub solver.

This script generates C++ test code with complex dependency graphs that have:
- Diamond dependencies (multiple packages depending on the same package)
- Tight version constraints that cause conflicts
- Deep dependency chains
- Random but realistic dependency patterns
"""

import random
import argparse
from typing import List, Tuple, Set

class PackageGenerator:
    def __init__(self, num_packages: int, min_version: int = 10, max_version: int = 30,
                 max_deps: int = 5, seed: int = 42):
        """
        Initialize the package generator.

        Args:
            num_packages: Total number of packages to generate
            min_version: Minimum version number
            max_version: Maximum version number
            max_deps: Maximum number of dependencies per package version
            seed: Random seed for reproducibility
        """
        self.num_packages = num_packages
        self.min_version = min_version
        self.max_version = max_version
        self.max_deps = max_deps
        random.seed(seed)

        # Generate package names
        self.packages = [f"pkg-{i:04d}" for i in range(num_packages)]

        # Create layers to ensure acyclic dependencies
        self.layers = self._create_layers()

    def _create_layers(self) -> List[List[str]]:
        """Create dependency layers to ensure DAG (Directed Acyclic Graph)."""
        num_layers = max(5, int(self.num_packages ** 0.5))  # sqrt for reasonable depth
        layer_size = self.num_packages // num_layers

        layers = []
        idx = 0
        for layer_idx in range(num_layers):
            if layer_idx == num_layers - 1:
                # Last layer gets remaining packages
                layers.append(self.packages[idx:])
            else:
                layers.append(self.packages[idx:idx + layer_size])
                idx += layer_size

        return layers

    def _get_potential_deps(self, pkg: str) -> List[str]:
        """Get potential dependencies for a package (only from lower layers)."""
        # Find which layer this package is in
        pkg_layer = -1
        for i, layer in enumerate(self.layers):
            if pkg in layer:
                pkg_layer = i
                break

        # Can only depend on packages in lower layers
        potential_deps = []
        for i in range(pkg_layer + 1, len(self.layers)):
            potential_deps.extend(self.layers[i])

        return potential_deps

    def _generate_version_constraint(self, v: int) -> str:
        """Generate a version constraint."""
        constraint_type = random.choices(
            ['singleton', 'narrow_range', 'wide_range'],
            weights=[0.3, 0.4, 0.3]  # 30% singleton (tight), 40% narrow, 30% wide
        )[0]

        if constraint_type == 'singleton':
            # Exact version requirement (causes most conflicts)
            return f"VS::singleton({v})"
        elif constraint_type == 'narrow_range':
            # Narrow range (2-4 versions)
            range_size = random.randint(2, 4)
            upper = min(v + range_size, self.max_version)
            return f"VS::between({v}, {upper})"
        else:
            # Wide range (5-10 versions)
            range_size = random.randint(5, 10)
            upper = min(v + range_size, self.max_version)
            return f"VS::between({v}, {upper})"

    def generate_dependencies(self) -> dict:
        """Generate dependency graph."""
        deps = {}

        # Root package depends on top layer packages
        root_deps = random.sample(self.layers[0], min(10, len(self.layers[0])))
        deps[("root", self.min_version)] = [
            (dep, f"VS::between({self.min_version}, {self.max_version})")
            for dep in root_deps
        ]

        # Generate dependencies for each package
        for pkg in self.packages:
            potential_deps = self._get_potential_deps(pkg)

            if not potential_deps:
                # Leaf package - no dependencies
                for v in range(self.min_version, self.max_version):
                    deps[(pkg, v)] = []
                continue

            # Each version has different dependencies
            for v in range(self.min_version, self.max_version):
                num_deps = random.randint(0, min(self.max_deps, len(potential_deps)))

                if num_deps == 0:
                    deps[(pkg, v)] = []
                else:
                    # Select random dependencies
                    selected_deps = random.sample(potential_deps, num_deps)

                    # Create version constraints
                    version_deps = []
                    for dep in selected_deps:
                        constraint = self._generate_version_constraint(v)
                        version_deps.append((dep, constraint))

                    deps[(pkg, v)] = version_deps

        return deps

    def generate_dependency_file(self, deps: dict) -> str:
        """
        Generate dependency file in text format.

        Format:
        package version dep1:constraint1 dep2:constraint2 ...

        Constraints:
        - singleton:X -> exact version X
        - range:X:Y -> between X and Y
        """
        lines = []
        lines.append(f"# Auto-generated dependency graph")
        lines.append(f"# Packages: {self.num_packages + 1}")
        lines.append(f"# Format: package version dep1:constraint dep2:constraint ...")
        lines.append(f"# Constraints: singleton:X or range:X:Y")
        lines.append("")

        for (pkg, ver), dep_list in sorted(deps.items()):
            parts = [pkg, str(ver)]

            for dep, constraint in dep_list:
                # Convert C++ constraint to simple format
                if "singleton" in constraint:
                    # Extract version from VS::singleton(X)
                    v = constraint.split("(")[1].split(")")[0]
                    parts.append(f"{dep}:singleton:{v}")
                elif "between" in constraint:
                    # Extract range from VS::between(X, Y)
                    params = constraint.split("(")[1].split(")")[0].split(",")
                    v1 = params[0].strip()
                    v2 = params[1].strip()
                    parts.append(f"{dep}:range:{v1}:{v2}")

            lines.append(" ".join(parts))

        return "\n".join(lines)

    def print_statistics(self, deps: dict):
        """Print statistics about the generated graph."""
        total_deps = sum(len(dep_list) for dep_list in deps.values())
        avg_deps = total_deps / len(deps) if deps else 0

        # Count packages by number of dependencies
        dep_counts = {}
        for dep_list in deps.values():
            count = len(dep_list)
            dep_counts[count] = dep_counts.get(count, 0) + 1

        # Count singleton constraints
        singleton_count = 0
        for dep_list in deps.values():
            for _, constraint in dep_list:
                if "singleton" in constraint:
                    singleton_count += 1

        print("\n=== Dependency Graph Statistics ===")
        print(f"Total packages: {self.num_packages + 1} (including root)")
        print(f"Total package-versions: {len(deps)}")
        print(f"Total dependencies: {total_deps}")
        print(f"Average dependencies per version: {avg_deps:.2f}")
        print(f"Singleton constraints: {singleton_count} ({singleton_count/total_deps*100:.1f}%)")
        print(f"\nDependency distribution:")
        for count in sorted(dep_counts.keys()):
            print(f"  {count} deps: {dep_counts[count]} versions")


def main():
    parser = argparse.ArgumentParser(description="Generate large-scale dependency test cases")
    parser.add_argument("-n", "--num-packages", type=int, default=1000,
                        help="Number of packages to generate (default: 1000)")
    parser.add_argument("-o", "--output", type=str, default="test_generated",
                        help="Output file base name (default: test_generated)")
    parser.add_argument("--test-name", type=str, default="test_generated_large",
                        help="Test function name (default: test_generated_large)")
    parser.add_argument("--max-deps", type=int, default=20,
                        help="Maximum dependencies per package version (default: 5)")
    parser.add_argument("--seed", type=int, default=42,
                        help="Random seed (default: 42)")
    parser.add_argument("--min-version", type=int, default=10,
                        help="Minimum version number (default: 10)")
    parser.add_argument("--max-version", type=int, default=60,
                        help="Maximum version number (default: 30)")
    parser.add_argument("--stats-only", action="store_true",
                        help="Only print statistics, don't generate files")

    args = parser.parse_args()

    print(f"Generating dependency graph with {args.num_packages} packages...")

    generator = PackageGenerator(
        num_packages=args.num_packages,
        min_version=args.min_version,
        max_version=args.max_version,
        max_deps=args.max_deps,
        seed=args.seed
    )

    deps = generator.generate_dependencies()
    generator.print_statistics(deps)

    if not args.stats_only:
        # Generate dependency file
        dep_filename = f"{args.output}.txt"
        print(f"\nGenerating dependency file...")
        dep_content = generator.generate_dependency_file(deps)

        with open(dep_filename, 'w') as f:
            f.write(dep_content)

        print(f"Dependency file written to: {dep_filename}")
        print(f"\nTo use this test:")
        print(f"1. Compile test_loader (once): g++ -std=c++17 -O2 -o test_loader test_loader.cpp -I..")
        print(f"2. Run: ./test_loader {dep_filename}")
        print(f"\nYou can generate many different dependency graphs (.txt files)")
        print(f"and test them all with the same compiled binary!")


if __name__ == "__main__":
    main()

