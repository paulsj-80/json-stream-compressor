#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include "json_compressor.h"

using namespace std;


string getfile(string fname) {
    std::ifstream r1(fname);
    std::stringstream buff;
    buff << r1.rdbuf();
    return buff.str();
}

void test_sample_1() {
    jsc::compress("../test/samples/sample1.txt", "/tmp/output.bin");

    assert(getfile("../test/samples/sample1.bin").
           compare(getfile("/tmp/output.bin")) == 0);    
}


int main(int argc, char* argv[]) {
    cerr << "********************************" << endl;
    cerr << "Starting tests" << endl;

    test_sample_1();

    cerr << "Tests finished succesfuly" << endl;

    return 0;
}
