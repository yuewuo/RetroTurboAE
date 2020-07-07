
#include <fstream>
#include <iostream>
using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 9) {
        cout << "usage: <filename> <string>" << endl;
        cout << argc << endl;
        for (int i = 0; i <= argc; i ++)
            cout << argv[i] << endl;
		return -1;
    }

    ofstream fout(argv[7], std::fstream::in | std::fstream::out | std::fstream::app);

    fout << argv[8] << endl;

    fout.close();

    return 0;
}