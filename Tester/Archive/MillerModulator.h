
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iostream>
using namespace std;

struct MillerModulator {

    vector<string> packet;
    string answer;
    double frequency;
    int previous_logic_bit, signal, length, reverse;
    vector<string> bit2str;

    MillerModulator() {
        previous_logic_bit = 1;
        signal = 0;
        bit2str.push_back("0000");
        bit2str.push_back("FFFF");
        // Do nothing.
    }

    void MillerAppend(int bit) { // Append one bit to packet, according to miller coding method
        answer += char(bit + '0');
        if (bit == 0) {
            if (previous_logic_bit == 0) {
                signal = 1 - signal;
                packet.push_back(bit2str[signal]);
                packet.push_back(bit2str[signal]);
            }
            else {
                packet.push_back(bit2str[signal]);
                packet.push_back(bit2str[signal]);
            }
        }
        else {
            packet.push_back(bit2str[signal]);
            signal = 1 - signal;
            packet.push_back(bit2str[signal]);
        }
        previous_logic_bit = bit;
    }

    vector<string> Compress(vector<string> s) {
        vector<string> ret;
        for (size_t i = 0; i < s.size();) {
            size_t j = i;
            // Find next symbol that cannot be compressed
            while(j < s.size() && s[i] == s[j])
                j ++;
            // Compress s[i..(j-1)]
            ostringstream sout;
            sout << s[i] << ":" << j - i;
            ret.push_back(sout.str());
            i = j;
        }
        return ret;
    }

    void Generate() {
        // Initialize random seed
        srand(time(NULL));
        // Judge if we need to reverse the signal.
        if (reverse) {
            signal = 1 - signal;
        }
        // Initialize preamble
        int preamble_unit = 4 * frequency / 2000;
        for(int j = 0; j < 3 * preamble_unit; j ++)
            packet.push_back(bit2str[signal]);
        for(int i = 0; i < 5; i ++) {
            for(int j = 0; j < preamble_unit; j ++) {
                packet.push_back(bit2str[signal]);
            }
            signal = 1 - signal;
        }
        // Generate 
        MillerAppend(0);
        for (int i = 0; i < length; i ++) {
            int bit = rand() % 2;
            MillerAppend(bit);
        }
        MillerAppend(0);
        packet = Compress(packet);
    }

};