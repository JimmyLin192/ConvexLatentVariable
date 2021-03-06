/*###############################################################E
## MODULE: testESMat.cpp
## VERSION: 1.0 
## SINCE 2014-07-27
## AUTHOR Jimmy Lin (xl5224) - JimmyLin@utexas.edu  
## DESCRIPTION: 
##      
#################################################################
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/

#include<iostream>
#include<string>
#include "../mat.h"
#include "exSparseMat.h"

#define PASS "PASS"
#define FAIL "FAIL"
#define RESULT(x) x?PASS:FAIL

using namespace std;
double **mat_A, **mat_B;
int nRows_A, nCols_A, nCols_B, nRows_B;
Esmat *esmat_A, *esmat_B;

bool test_esmat_read () {
    return esmat_equal(esmat_A, mat_A) && esmat_equal(esmat_B, mat_B);
}

bool test_esmat_min_row () {
    double ** mat_minR_A = mat_init (nRows_A, nCols_A);
    double ** mat_minR_B = mat_init (nRows_B, nCols_B);
    mat_min_row (mat_A, mat_minR_A, nRows_A, nCols_A);
    mat_min_row (mat_B, mat_minR_B, nRows_B, nCols_B);
    Esmat * esmat_minR_A = esmat_init (); 
    Esmat * esmat_minR_B = esmat_init (); 
    esmat_min_row (esmat_A, esmat_minR_A);
    esmat_min_row (esmat_B, esmat_minR_B);

    bool success = true;
    success = success && esmat_equal(esmat_minR_A, mat_minR_A);
    if (!success) {
        cout << "Dump for esmat_minR_A: " << endl;
        cout << esmat_toString(esmat_minR_A);
        mat_print (mat_minR_A, nRows_A, nCols_A);
    }
    success = success && esmat_equal(esmat_minR_B, mat_minR_B);
    if (!success) {
        cout << "Dump for esmat_minR_B: " << endl;
        cout << esmat_toString(esmat_minR_B);
        mat_print (mat_minR_B, nRows_B, nCols_B);
    }
    
    esmat_free(esmat_minR_A);
    esmat_free(esmat_minR_B);
    mat_free (mat_minR_A, nRows_A, nCols_A);
    mat_free (mat_minR_B, nRows_B, nCols_B);
    return success;
}

bool test_esmat_max_col () {
    double ** mat_maxC_A = mat_init (nRows_A, nCols_A);
    double ** mat_maxC_B = mat_init (nRows_B, nCols_B);
    mat_max_col (mat_A, mat_maxC_A, nRows_A, nCols_A);
    mat_max_col (mat_B, mat_maxC_B, nRows_B, nCols_B);
    Esmat * esmat_maxC_A = esmat_init (); 
    Esmat * esmat_maxC_B = esmat_init (); 
    esmat_max_col (esmat_A, esmat_maxC_A);
    esmat_max_col (esmat_B, esmat_maxC_B);

    bool success = true;
    success = success && esmat_equal(esmat_maxC_A, mat_maxC_A);
    if (!success) {
        cout << "Dump for esmat_maxC_A: " << endl;
        cout << esmat_toString(esmat_maxC_A);
        mat_print (mat_maxC_A, nRows_A, nCols_A);
    } 
    success = success && esmat_equal(esmat_maxC_B, mat_maxC_B);
    if (!success) {
        cout << "Dump for esmat_maxC_B: " << endl;
        cout << esmat_toString(esmat_maxC_B);
        mat_print (mat_maxC_B, nRows_B, nCols_B);
    }
    esmat_free(esmat_maxC_A);
    esmat_free(esmat_maxC_B);
    mat_free (mat_maxC_A, nRows_A, nCols_A);
    mat_free (mat_maxC_B, nRows_B, nCols_B);
    return success;
}

int main (int args, char ** argv) {
    string mat_A_fname = "mat_A.txt";
    string mat_B_fname = "mat_B.txt";

    mat_A = mat_read (mat_A_fname, &nRows_A, &nCols_A);
    mat_B = mat_read (mat_B_fname, &nRows_B, &nCols_B);
    esmat_A = esmat_read (mat_A_fname);
    esmat_B = esmat_read (mat_B_fname);

    // cout << esmat_toString(esmat_A) << endl;
    // mat_print(mat_A, nRows_A, nCols_A);

    string result_esmat_read = RESULT(test_esmat_read());
    string result_esmat_min_row = RESULT(test_esmat_min_row());
    string result_esmat_max_col = RESULT(test_esmat_max_col());
    // string result_esmat_sum_row = RESULT(test_esmat_sum_row());
    // string result_esmat_sum_col = RESULT(test_esmat_sum_col());

    std::cout << "Test Cases for exSparseMat:" << endl;
    std::cout << "  test_esmat_read: " << result_esmat_read << endl;
    cout << "  test_esmat_min_row: " << result_esmat_min_row << endl;
    cout << "  test_esmat_max_col: " << result_esmat_max_col << endl;
    // cout << "  test_esmat_sum_row: " << result_esmat_sum_row << endl;
    // cout << "  test_esmat_sum_col: " << result_esmat_sum_col << endl;

    esmat_free (esmat_A);
    esmat_free (esmat_B);
    mat_free (mat_A, nRows_A, nCols_A);
    mat_free (mat_B, nRows_B, nCols_B);
    return 0;
}
