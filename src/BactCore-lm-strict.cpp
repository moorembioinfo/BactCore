
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <omp.h>
#include <unistd.h>

/*
Matthew P. Moore 2023
https://github.com/moorembioinfo/BactCore
Compile with: g++ -std=c++11 -O3 -fopenmp BactCore.cpp -o BactCore
moorembioinfo/BactCore is licensed under the
GNU General Public License v3.0
*/


using namespace std;

const double threshold = 0;

int main(int argc, char* argv[]) {
    int num_threads = 1;
    bool exclude_reference = false;
    int opt;
    while ((opt = getopt(argc, argv, "t:r")) != -1) {
        switch (opt) {
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'r':
                exclude_reference = true;
                break;
            default:
                cerr << "Usage: " << argv[0] << " [-t num_threads] [-r] input_file" << endl;
                return 1;
        }
    }

    if (optind >= argc) {
        cerr << "Usage: " << argv[0] << " [-t num_threads] [-r] input_file" << endl;
        return 1;
    }

    string filename = argv[optind];
    ifstream file(filename);
    if (!file) {
        cerr << "Couldn't open alignment file" << filename << endl;
        return 1;
    }

    omp_set_num_threads(num_threads);

    string line;
    vector<int> Ns, gaps;
    size_t total_sequences = 0;
    bool in_sequence = false;

    while (getline(file, line)) {
        if (line[0] == '>') {
            if (!exclude_reference || line.substr(1) != "Reference") {
                if (in_sequence) total_sequences++;
                in_sequence = false;
            }
        } else if (!exclude_reference || in_sequence) {
            in_sequence = true;
            if (Ns.empty()) {
                Ns.resize(line.length(), 0);
                gaps.resize(line.length(), 0);
            }
            for (size_t i = 0; i < line.length(); i++) {
                if (line[i] == 'N') Ns[i]++;
                if (line[i] == '-') gaps[i]++;
            }
        }
    }
    if (in_sequence) total_sequences++;

    vector<bool> sites_to_keep(Ns.size(), true);
    for (size_t i = 0; i < Ns.size(); i++) {
        if (Ns[i] + gaps[i] > threshold * total_sequences) {
            sites_to_keep[i] = false;
        }
    }

    file.clear();
    file.seekg(0, ios::beg);
    while (getline(file, line)) {
        if (line[0] == '>') {
            if (!exclude_reference || line.substr(1) != "Reference") {
                cout << line << '\n';
            }
        } else if (!exclude_reference || in_sequence) {
            for (size_t i = 0; i < line.length(); i++) {
                if (sites_to_keep[i]) {
                    cout << line[i];
                }
            }
            cout << '\n';
        }
    }

    return 0;
}
