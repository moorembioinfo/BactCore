#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unistd.h>

/*
Matthew P. Moore 2024
https://github.com/moorembioinfo/BactCore
Compile with: g++ -std=c++11 -O3 BactCore-large.cpp -o BactCore-large
moorembioinfo/BactCore is licensed under the
GNU General Public License v3.0
*/

using namespace std;

const double threshold = 0.05;

void process_chunk(const vector<string>& seqs, vector<int>& global_Ns, vector<int>& global_gaps) {
    int seq_length = seqs[0].length();

    for (int i = 0; i < seq_length; i++) {
        int local_Ns = 0;
        int local_gaps = 0;

        for (const auto& seq : seqs) {
            if (seq[i] == 'N') {
                local_Ns++;
            } else if (seq[i] == '-') {
                local_gaps++;
            }
        }

        global_Ns[i] += local_Ns;
        global_gaps[i] += local_gaps;
    }
}

void write_output(const vector<string>& seqs, const vector<string>& names, const vector<int>& global_Ns, const vector<int>& global_gaps) {
    int seq_length = seqs[0].length();
    int total_seqs = seqs.size();

    for (size_t j = 0; j < seqs.size(); j++) {
        cout << ">" << names[j] << '\n';
        const string& seq = seqs[j];
        
        for (int i = 0; i < seq_length; i++) {
            if (global_Ns[i] + global_gaps[i] <= threshold * total_seqs) {
                cout << seq[i];
            }
        }
        cout << '\n';
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " input_file" << endl;
        return 1;
    }

    string filename = argv[1];
    ifstream file(filename);
    if (!file) {
        cerr << "Couldn't open alignment file " << filename << endl;
        return 1;
    }

    // Set the chunk size
    const size_t chunk_size = 10000;

    // Vectors to accumulate global Ns and gaps counts
    vector<int> global_Ns;
    vector<int> global_gaps;
    bool initialized = false;

    while (!file.eof()) {
        vector<string> seqs;
        vector<string> names;
        string line, seq, name;
        size_t num_seqs = 0;

        // Chunk sequences
        while (num_seqs < chunk_size && getline(file, line)) {
            if (line[0] == '>') {
                if (!seq.empty()) {
                    seqs.push_back(seq);
                    names.push_back(name);
                    seq.clear();
                }
                name = line.substr(1);
            } else {
                seq += line;
            }
            num_seqs++;
        }
        seqs.push_back(seq);
        names.push_back(name);

        if (!initialized) {
            int seq_length = seqs[0].length();
            global_Ns.resize(seq_length, 0);
            global_gaps.resize(seq_length, 0);
            initialized = true;
            }

            process_chunk(seqs, global_Ns, global_gaps);

            write_output(seqs, names, global_Ns, global_gaps);
        }

        return 0;
} 
