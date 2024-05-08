#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <omp.h>
#include <unistd.h>

/*
Matthew P. Moore 2024
https://github.com/moorembioinfo/BactCore
Compile with: g++ -std=c++11 -O3 -fopenmp BactCore-snps.cpp -o BactCore-snps
moorembioinfo/BactCore is licensed under the
GNU General Public License v3.0
*/

using namespace std;

int main(int argc, char* argv[]) {
    int num_threads = 1;
    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                num_threads = atoi(optarg);
                break;
            default:
                cerr << "Usage: " << argv[0] << " [-t num_threads] input_file" << endl;
                return 1;
        }
    }

    if (optind >= argc) {
        cerr << "Usage: " << argv[0] << " [-t num_threads] input_file" << endl;
        return 1;
    }

    string filename = argv[optind];
    ifstream file(filename);
    if (!file) {
        cerr << "Couldn't open alignment file " << filename << endl;
        return 1;
    }

    omp_set_num_threads(num_threads);

    vector<string> seqs;
    vector<string> names;
    string line, seq, name;
    while (getline(file, line)) {
        if (line[0] == '>') {
            if (!seq.empty()) {
                seqs.push_back(seq);
                seq.clear();
            }
            name = line.substr(1);
            names.push_back(name);
        } else {
            seq += line;
        }
    }
    if (!seq.empty()) {
        seqs.push_back(seq);
    }

    int seq_length = seqs[0].length();
    vector<bool> is_variable(seq_length, false);

    #pragma omp parallel for shared(seqs, is_variable)
    for (int i = 0; i < seq_length; i++) {
        char first_char = seqs[0][i];
        for (int j = 1; j < seqs.size(); j++) {
            if (seqs[j][i] != first_char) {
                is_variable[i] = true;
                break;
            }
        }
    }

    for (int j = 0; j < seqs.size(); j++) {
        cout << ">" << names[j] << endl;
        for (int i = 0; i < seq_length; i++) {
            if (is_variable[i]) {
                cout << seqs[j][i];
            }
        }
        cout << endl;
    }

    return 0;
}
