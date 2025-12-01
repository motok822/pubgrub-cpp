// Generic test loader for dependency graphs
// Usage: ./test_loader <dependency_file>
//
// This loads dependencies from a text file and runs both naive and optimized solvers.

#include "../include/core.h"
#include "../include/provider.h"
#include "../src/dpll_solver.cpp"
#include "../src/cdcl_solver.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <chrono>
#include <map>
#include <string>
#include <vector>

using TestProvider = OfflineDependencyProvider<std::string, int>;
using VS = Ranges<int>;

static void print_timing(const std::string &label, long long naive_us, long long solver_us, size_t naive_size, size_t solver_size)
{
    std::cout << label << " timing (microseconds)\n";
    std::cout << "  DPLL:    " << naive_us << " us (packages=" << naive_size << ")\n";
    std::cout << "  CDCL:   " << solver_us << " us (packages=" << solver_size << ")\n";
    if (solver_us > 0)
    {
        double speedup = static_cast<double>(naive_us) / solver_us;
        std::cout << "  Speedup:  " << speedup << "x (DPLL/CDCL)\n";

        if (speedup > 1.0)
        {
            std::cout << "  *** PubGrub solver is " << speedup << "x FASTER! ***\n";
        }
        else
        {
            std::cout << "  Note: DPLL is faster for this case (ratio=" << (1.0 / speedup) << "x)\n";
        }
    }
}

void load_dependencies(TestProvider &provider, const std::string &filename, int &root_version)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open: " << filename << "\n";
        exit(1);
    }

    std::string line;
    int line_num = 0;
    int package_count = 0;
    int dep_count = 0;

    while (std::getline(file, line))
    {
        line_num++;
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string package;
        int version;

        iss >> package >> version;

        // Track root version
        if (package == "root")
        {
            root_version = version;
        }

        // Read dependencies
        std::map<std::string, VS> deps;
        std::string dep_spec;
        while (iss >> dep_spec)
        {
            dep_count++;
            // Parse dep_spec: "dep_name:constraint_type:params"
            size_t colon1 = dep_spec.find(':');
            if (colon1 == std::string::npos)
            {
                std::cerr << "Invalid dep spec at line " << line_num << ": " << dep_spec << "\n";
                continue;
            }

            std::string dep_name = dep_spec.substr(0, colon1);
            std::string rest = dep_spec.substr(colon1 + 1);

            size_t colon2 = rest.find(':');
            if (colon2 == std::string::npos)
            {
                std::cerr << "Invalid constraint at line " << line_num << ": " << dep_spec << "\n";
                continue;
            }

            std::string constraint_type = rest.substr(0, colon2);
            std::string params = rest.substr(colon2 + 1);

            if (constraint_type == "singleton")
            {
                int v = std::stoi(params);
                deps[dep_name] = VS::singleton(v);
            }
            else if (constraint_type == "range")
            {
                size_t colon3 = params.find(':');
                if (colon3 == std::string::npos)
                {
                    std::cerr << "Invalid range at line " << line_num << ": " << dep_spec << "\n";
                    continue;
                }
                int v1 = std::stoi(params.substr(0, colon3));
                int v2 = std::stoi(params.substr(colon3 + 1));
                deps[dep_name] = VS::between(v1, v2);
            }
        }

        provider.add_dependencies(package, version, deps.begin(), deps.end());
        package_count++;
    }

    file.close();
    std::cout << "Loaded " << package_count << " package-versions with " << dep_count << " dependencies from " << filename << "\n";
}

int main(int argc, char *argv[])
{
    std::string dep_file;
    if (argc < 2)
    {
        dep_file = "test_generated.txt";
        std::cout << "No dependency file specified. Using default: " << dep_file << "\n";
    }
    else
    {
        dep_file = argv[1]; // Assign dep_file from argv[1]
    }

    std::cout << "========================================\n";
    std::cout << "PubGrub Solver Performance Test\n";
    std::cout << "========================================\n\n";

    TestProvider provider;
    int root_version = 10;

    // Load dependencies from file
    std::cout << "Loading dependencies...\n";
    load_dependencies(provider, dep_file, root_version);
    std::cout << "\n";

    // === Run both solvers ===
    std::cout << "Running solvers (this may take a while)...\n\n";
    std::string root = "root";

    std::cout << "[1/2] Running naive DPLL solver...\n";
    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();
    std::cout << "      DPLL solver completed.\n\n";

    std::cout << "[2/2] Running optimized PubGrub solver...\n";
    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();
    std::cout << "      CDCL solver completed.\n\n";

    // Convert to sorted maps for comparison
    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    // Verify both found solutions
    std::cout << "========================================\n";
    std::cout << "Results\n";
    std::cout << "========================================\n";

    if (naive_sorted.size() == 0 || solver_sorted.size() == 0)
    {
        std::cerr << "ERROR: At least one solver found no solution!\n";
        std::cerr << "  DPLL packages: " << naive_sorted.size() << "\n";
        std::cerr << "  CDCL packages: " << solver_sorted.size() << "\n";
        return 1;
    }

    if (naive_sorted.count("root") == 0 || solver_sorted.count("root") == 0)
    {
        std::cerr << "ERROR: Root package missing from solution!\n";
        return 1;
    }

    std::cout << "Solution package count: " << solver_sorted.size() << "\n\n";

    print_timing("Performance",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());

    std::cout << "\n========================================\n";
    std::cout << "✓ Test passed successfully!\n";
    std::cout << "========================================\n";

    return 0;
}
