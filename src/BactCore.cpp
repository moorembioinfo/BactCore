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

const double threshold = 0.05;

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

    vector<string> seqs;
    vector<string> names;
    string line, seq, name;
    while (getline(file, line)) {
        if (line[0] == '>') {
            if (!seq.empty()) {
                if (!(exclude_reference && name == "Reference")) {
                    seqs.push_back(seq);
                    names.push_back(name);
                }
                seq.clear();
            }
            name = line.substr(1);
        } else {
            seq += line;
        }
    }
    if (!(exclude_reference && name == "Reference")) {
        seqs.push_back(seq);
        names.push_back(name);
    }

    int seq_length = seqs[0].length();
    vector<int> Ns(seq_length, 0);
    vector<int> gaps(seq_length, 0);

    #pragma omp parallel for shared(seqs, Ns, gaps)
    for (int i = 0; i < seq_length; i++) {
        for (int j = 0; j < seqs.size(); j++) {
            const string& seq = seqs[j];
            if (seq[i] == 'N') {
    #pragma omp atomic
                Ns[i]++;
            } else if (seq[i] == '-') {
    #pragma omp atomic
                gaps[i]++;
            }
        }
    }

    string filtered_seqs;
    for (int j = 0; j < seqs.size(); j++) {
        filtered_seqs += ">" + names[j] + '\n';
        const string& seq = seqs[j];
        for (int i = 0; i < seq_length; i++) {
            if (Ns[i] + gaps[i] <= threshold * seqs.size()) {
                filtered_seqs += seq[i];
            }
        }
        filtered_seqs += '\n';
    }
    cout << filtered_seqs;
    return 0;
}
