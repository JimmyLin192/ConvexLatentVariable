#include<string>
#include<iostream>
#include<fstream>
#include<vector>
#include<cassert>
#include<algorithm>
#include"math.h"

using namespace std;

/* Global variables */
const double DUMMY_PENALTY_RATE = 1e5;
const double TRIM_THRESHOLD = 10e-5;
const double EPISILON = 10e-3;
const double INF = 10e300;

/* Definition of Data structure for Extensible Sparse Matrix (Esmat*) */
typedef struct {
    int nRows; int nCols;
    vector< pair<int, double> >  val;
} Esmat ;

typedef double (* Operator) (double, double); 
typedef bool (* Comparator) (double, double); 

/* Powerful function pointers and definition of instantiated operators */
double min (double a, double b) { return a < b ? a : b; }
double max (double a, double b) { return a > b ? a : b; }
double sum (double a, double b) { return a + b; }
double diff (double a, double b) { return a - b; } // non-symmetric 
double times (double a, double b) { return a * b; }
double power2 (double val, double counter) 
    { return counter + val * val; } // non-symmetric
double count (double value, double counter) { 
    // note that for this "count" function, two input is not symmetric
    if (fabs(value) > TRIM_THRESHOLD) counter += 1.0;
    return counter; 
}

bool gt (double a, double b) { return a > b; }
bool lt (double a, double b) { return a < b; }

/* Prototype for fundamental functions, typically computational frameworks*/
double esmat_unary_operate (Esmat *A, Operator opt);
void esmat_bin_operate (Esmat* A, Esmat* B, Esmat* dest, Operator opt);
void esmat_operate_col (Esmat* A, Esmat* dest, Operator opt, double init_value);
void esmat_operate_row (Esmat* A, Esmat* dest, Operator opt, double init_value);
void esmat_compare_col (Esmat* A, Esmat* dest, Comparator cmp);
void esmat_compare_row (Esmat* A, Esmat* dest, Comparator cmp);

/* Allocation and De-allocation */
Esmat* esmat_init (int nRows, int nCols);
Esmat* esmat_init (Esmat * A);
Esmat* esmat_init ();
void esmat_init_all (vector<Esmat*>* src);
Esmat* esmat_read (string fname);
bool esmat_equal (Esmat* esmat, double **mat);
bool esmat_equal (Esmat* A, Esmat* B);
void esmat_free (Esmat* src);
void esmat_free_all (vector<Esmat*> src);
void esmat_zeros (Esmat* A);
void esmat_augment (Esmat* A);
void esmat_resize (Esmat* A, Esmat* B);
void esmat_resize (Esmat* A, int nRows, int nCols);

/* Rearrange one esmat */
void esmat_align (Esmat* mat);
void esmat_copy (Esmat* A, Esmat* D);
void esmat_trim (Esmat* A);

/* submat and merge */
void esmat_submat_row (Esmat* mat, vector<Esmat*> submats, vector< pair<int,int> >* look_up);
void esmat_submat_row (Esmat* mat, vector<Esmat*> submats, vector<int>* word_lookup, vector< vector<int> >* voc_lookup);
void esmat_merge_row (Esmat* submat, int start_index, int end_index, Esmat* mat);
void esmat_merge_row (Esmat* submat, vector<int>* sub_voc_lookup, Esmat* mat);

/* frobenius product and norm */
void esmat_abs (Esmat* A, Esmat* dest);
double esmat_frob_prod (Esmat* A, Esmat* B);
double esmat_sum (Esmat* A);
double esmat_frob_norm (Esmat* A);

/* scalar multiplication */
void esmat_scalar_mult (double scalar, Esmat* A);
void esmat_scalar_mult (double scalar, Esmat* A, Esmat* dest);

/* Auxiliary functions */
bool esmat_isValid (Esmat* A, Esmat* B, int mode);
bool esmat_isAligned (Esmat* A);
string esmat_toString (Esmat* A);
string esmat_toInfo (Esmat* A);
void esmat_print (Esmat* A, string str);

/* Compute dummy term */
double esmat_compute_dummy (Esmat* A, double r);

/* Add and Subtract two extensible sparse matrices */
void esmat_add (Esmat* A, Esmat* B, Esmat* dest);
void esmat_sub (Esmat* A, Esmat* B, Esmat* dest);

/* min, max and sum over column and row elements */
void esmat_max_over_col (Esmat* A, Esmat* dest); 
void esmat_count_over_col (Esmat* A, Esmat* dest); 
void esmat_sum_col (Esmat* A, Esmat* dest);
void esmat_sum_row (Esmat* A, Esmat* dest); 

/* min, max column and row elements */
void esmat_min_col (Esmat* A, Esmat* dest);
void esmat_max_col (Esmat* A, Esmat* dest);
void esmat_min_row (Esmat* A, Esmat* dest);
void esmat_max_row (Esmat* A, Esmat* dest);

bool pair_First_Elem_inc_Comparator (const std::pair<int, double>& firstElem, const std::pair<int, double>& secondElem) {
    // sort pairs by second element with *incremental order*
    return firstElem.first < secondElem.first;
}

/* Allocate one extensible sparse matrix with all zero entries */
Esmat* esmat_init (int nRows, int nCols) {
    Esmat* freshman = new Esmat ();
    freshman->nRows = nRows;
    freshman->nCols = nCols;
    // freshman->val = new vector< pair<int, double> > ();
    return freshman;
}
Esmat* esmat_init (Esmat* src) {
    return esmat_init (src->nRows, src->nCols);
}
Esmat* esmat_init () {
    return esmat_init (0, 0);
}
void esmat_init_all (vector<Esmat*>* src) {
    int N = src->size();
    for (int i = 0; i < N; i++) {
        (*src)[i] = esmat_init();
    }
}
void esmat_resize (Esmat* A, Esmat* B) {
    A->nRows = B->nRows;
    A->nCols = B->nCols;
}
void esmat_resize (Esmat* A, int nRows, int nCols) {
    A->nRows = nRows;
    A->nCols = nCols;
}
Esmat* esmat_read (string fname) {
   	ifstream fin (fname.c_str());
    int nRows, nCols;
    fin >> nRows >> nCols;
    Esmat* result = esmat_init (nRows, nCols);
    double val;
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            fin >> val;
            if (fabs(val) > TRIM_THRESHOLD) {
                int esmat_index = i + j * nRows;
                result->val.push_back(make_pair(esmat_index, val));
            }
        }
    }
    esmat_align (result);
    fin.close();
    return result;
}
bool esmat_equal (Esmat* A, Esmat* B) {
    if (A->nRows != B->nRows || A->nCols != B->nCols) {
        return false;
    }
    bool isequal = true;
    Esmat * temp = esmat_init (A);
    esmat_sub (A, B, temp);
    int size_temp = temp->val.size();
    for (int i = 0; i < size_temp; i++) {
        if (fabs(temp->val[i].second) >= TRIM_THRESHOLD) {
            isequal = false;
            break;
        }
    }
    esmat_free (temp);
    return isequal;
}
bool esmat_equal (Esmat* esmat, double **mat) {
    bool isequal = true;
    int esmat_size = esmat->val.size();
    int mat_capacity = esmat->nRows * esmat->nCols;
    int i = 0, esmat_index = 0, mat_index = 0;
    while (true) {
        esmat_index = esmat->val[i].first;
        double mat_value = mat[mat_index%esmat->nRows][mat_index/esmat->nRows];
        if (esmat_index > mat_index) {
            // there is non-zero value in mat but no in esmat, hence not equal
            if (fabs(mat_value) > TRIM_THRESHOLD) {
                // cout << esmat_index << "," << mat_index << endl;
                isequal = false; 
                break;
            }
            ++ mat_index;
        } else if (esmat_index == mat_index) {
            double esmat_value = esmat->val[i].second;
            // element in the same position is not equal to each other
            if (fabs(esmat_value - mat_value) > EPISILON) {
                isequal = false; 
                break;
            }
            ++ i; ++ mat_index;
            if (i >= esmat_size) break;
        }
    }
    while (mat_index < mat_capacity) {
        double mat_value = mat[mat_index%esmat->nRows][mat_index/esmat->nRows];
        if (fabs(mat_value) > TRIM_THRESHOLD) {
            isequal = false; 
            break;
        }
        ++ mat_index;
    }
    return isequal;
}
/* Deallocate given Esmat */
void esmat_free (Esmat* src) {
    src->val.clear();
    delete src;
}
void esmat_free_all (vector<Esmat*> src) {
    int N = src.size();
    for (int i = 0; i < N; i ++) {
        esmat_free(src[i]);
    }
}
void esmat_zeros (Esmat* A) {
    A->val.clear();
}
/* esmat alignment: place entry of sparse matrix in index-increasing order */
void esmat_align (Esmat* mat) {
    std::sort(mat->val.begin(), mat->val.end(), pair_First_Elem_inc_Comparator);
}

/* submat and merge */
/* pick subset of rows and form a new esmat, used local document penalty */
void esmat_submat_row (Esmat* mat, vector<Esmat*> submats, vector< pair<int,int> >* look_up) {
    int nDocs = look_up->size();
    // renew the characteristics of submats
    for (int d = 0; d < nDocs; d ++) {
        int end_index = (*look_up)[d].second;
        int start_index = (*look_up)[d].first;
        submats[d]->nRows = end_index - start_index;
        submats[d]->nCols = mat->nCols;
        submats[d]->val.clear();
    }

    int mat_size = mat->val.size();
    int i = 0, d = 0;
    int start_index = (*look_up)[d].first;
    int end_index = (*look_up)[d].second;
    while (i < mat_size) {
        int mat_index = mat->val[i].first;
        int mat_row_index = mat_index % mat->nRows;
        int mat_col_index = mat_index / mat->nRows;
        // find a corresponding submats
        while (!(start_index <= mat_row_index && mat_row_index < end_index)) {
            ++ d;
            if (d >= nDocs) d = 0;
            start_index = (*look_up)[d].first;
            end_index = (*look_up)[d].second;
        }
        // get value of that entry
        double value = mat->val[i].second;
        // compute corresponding position in submats
        int submat_row_index = mat_row_index - start_index;
        int submat_col_index = mat_col_index;
        int submat_index = submat_row_index + submat_col_index * submats[d]->nRows;
        submats[d]->val.push_back(make_pair(submat_index, value));
        ++ i;
    }
}
// This submat_row version is for coverage_subproblem
void esmat_submat_row (Esmat* mat, vector<Esmat*> submats, vector<int>* word_lookup, vector< vector<int> >* voc_lookup) {
    // cout << esmat_toInfo (mat);
    // cout << esmat_toString (mat);
    int nVocs = voc_lookup->size();
    // STEP ONE: renew the characteristics of submats
    for (int v = 0; v < nVocs; v ++) {
        submats[v]->nRows = (*voc_lookup)[v].size();
        submats[v]->nCols = mat->nCols;
        submats[v]->val.clear();
    }
    // STEP TWO:
    int mat_size = mat->val.size();
    for (int i = 0; i < mat_size; i ++) {
        int mat_index = mat->val[i].first;
        int mat_row_index = mat_index % mat->nRows;
        int mat_col_index = mat_index / mat->nRows;

        int v = (*word_lookup)[mat_row_index] - 1;
        vector<int>::iterator it;
        it = find((*voc_lookup)[v].begin(), (*voc_lookup)[v].end(), mat_row_index);
        int submat_row_index = it - (*voc_lookup)[v].begin();
        int submat_col_index = mat_col_index;
        int submat_index = submat_row_index + submat_col_index * (submats[v]->nRows);
       // cout << mat_row_index << "," << mat_col_index << "," << mat_index << ",v=" << v<< endl;
       // cout << submat_row_index << "," << submat_col_index << "," << submat_index << endl;
        double value = mat->val[i].second;
        submats[v]->val.push_back(make_pair(submat_index, value));
    }
    // STEP THREE: re-align the submats
    for (int v = 0; v < nVocs; v ++) {
        esmat_align (submats[v]);
    }
}

/* put submat to specified position of mat */
// this merge_row version is for local_topic_subproblem
void esmat_merge_row (Esmat* submat, int start_index, int end_index, Esmat* mat) {
    assert (start_index < end_index);

    int submat_size = submat->val.size();
    for (int i = 0; i < submat_size; i ++) {
        int submat_index = submat->val[i].first;
        int submat_row_index = submat_index % submat->nRows;
        int submat_col_index = submat_index / submat->nRows;
        // get value of that entry
        double value = submat->val[i].second;
        // compute corresponding position in submat
        int mat_row_index = submat_row_index + start_index;
        int mat_col_index = submat_col_index;
        int mat_index = mat_row_index + mat_col_index * mat->nRows;
        mat->val.push_back(make_pair(mat_index, value));
    }
}
// this merge_row version is for coverage_subproblem
void esmat_merge_row (Esmat* submat, vector<int>* sub_voc_lookup, Esmat* mat) {
    int nWords = sub_voc_lookup->size(); // related to one vocabulary
    assert (submat->nRows == nWords);

    int submat_size = submat->val.size();
    for (int i = 0; i < submat_size; i ++) {
        int submat_index = submat->val[i].first;
        int submat_row_index = submat_index % submat->nRows;
        int submat_col_index = submat_index / submat->nRows;
        
        int mat_row_index = (*sub_voc_lookup)[submat_row_index];
        int mat_col_index = submat_col_index;
        int mat_index = mat_row_index + mat_col_index * mat->nRows;

        double value = submat->val[i].second;
        mat->val.push_back(make_pair(mat_index, value));
    }
}
/* frobenius product */
double esmat_frob_prod (Esmat* A, Esmat* B) {
    Esmat* dest = esmat_init();
    esmat_bin_operate (A, B, dest, times);
    double result = esmat_sum (dest);
    esmat_free (dest);
    return result;
}
/* abs */
void esmat_abs (Esmat* A, Esmat* dest) {
    dest->nRows = A->nRows;
    dest->nCols = A->nCols;
    dest->val.clear();

    for (int i = 0; i < A->val.size(); i ++) {
        // insert multiplied value to the same position
        dest->val.push_back(make_pair(A->val[i].first, fabs(A->val[i].second))); 
    }
}
/* scalar times a esmat and store on input matrix*/
void esmat_scalar_mult (double scalar, Esmat* A) {
    int sizeA = A->val.size();
    int capacity = A->nRows * A->nCols;
    /* 
    cout << "capacity: " << capacity << " sizeA: " << sizeA << endl;
    cout << A->nRows << "," << A->nCols << endl;
    */
    for (int i = 0; i < sizeA; i ++) {
        // cout << A->val[i].first << endl;
        assert (A->val[i].first < capacity);
        A->val[i].second *= scalar; 
    }
}

/* scalar times a esmat */
void esmat_scalar_mult (double scalar, Esmat* A, Esmat* dest) {
    dest->nRows = A->nRows; 
    dest->nCols = A->nCols;
    dest->val.clear();
    for (int i = 0; i < A->val.size(); i ++) {
        // insert multiplied value to the same position
        dest->val.push_back(make_pair(A->val[i].first, scalar*A->val[i].second)); 
    }
}

/* Check validity (dim alignment) of input esmat 
 *  mode:
 *    1 - same dim alignment
 *    2 - product alignment
 * */
bool esmat_isValid (Esmat* A, Esmat* B, int mode) {
    bool success = false;
    if (mode == 1) {
        if (A->nRows == B->nRows && A->nCols == B->nCols) 
            success = true;
    } else if (mode == 2) {
        if (A->nCols == B->nRows) 
            success = true;
    }
    return success;
}
bool esmat_isAligned (Esmat* A) {
    int sizeA = A->val.size();    
    int last_index = -1;
    for (int i = 0; i < sizeA; i ++) {
        int index = A->val[i].first;
        if (index <= last_index) { 
            /*
            cout << "i: " << i << ", size: " << sizeA << endl;
            cout << "index: " << index << ", last_index: " << last_index << endl;
            cout << "[A]" << endl;
            cout << esmat_toString(A);
            */
            return false;
        } 
        last_index = index;
    }
    return true;
}

string esmat_toString (Esmat* A) {
    assert (A->nRows >= 0);
    assert (A->nCols >= 0);
    string idx_val_separator = ":";
    string field_seperator = ",";
    string line_separator = "\n";
    vector<string> allStrings (A->nRows, "") ;
    int sizeA = A->val.size();
    char buf[1000];
    for (int i = 0; i < sizeA; i ++) {
        int overall_idx = A->val[i].first;
        // column major data structure
        int col_idx = overall_idx / A->nRows;
        int row_idx = overall_idx % A->nRows;
        assert (col_idx < A->nCols);
        assert (row_idx < A->nRows);
        // generate newly added string
        sprintf(buf, "%d", col_idx);
        string temp = string(buf);
        if (A->val[i].second < 1e299) {
            sprintf(buf, "%f", A->val[i].second);
            temp += idx_val_separator + string(buf);
        } else {
            temp += idx_val_separator + "inf";
        }
        // row major string representation
        if (allStrings[row_idx].size() == 0) {
            allStrings[row_idx] = "" + temp;
        } else {
            allStrings[row_idx] += field_seperator + temp;
        }
    }
    string str = "";
    for (int i = 0; i < A->nRows; i ++) {
        if (allStrings[i].size() > 0)
            str += allStrings[i] + line_separator;
    }
    return str;
}
string esmat_toInfo (Esmat* A) {
    char buf[100];
    sprintf(buf, "%d, %d, %d\n", A->nRows, A->nCols, (int) A->val.size());
    return string(buf);
}
void esmat_print (Esmat* A, string str) {
    cout << str << " " << esmat_toInfo(A);
    cout << esmat_toString (A);
}

void esmat_augment (Esmat* A) {
    A->nCols += 1;    
}

/* copy content of Esmat* A to Esmat* D */
void esmat_copy (Esmat* A, Esmat* D) {

    D->nRows = A->nRows;
    D->nCols = A->nCols;
    D->val.clear();

    int sizeA = A->val.size();
    for (int i = 0; i < sizeA; i ++) {
        D->val.push_back(make_pair (A->val[i].first, A->val[i].second));
    }
}


void esmat_trim (Esmat* A, double threshold) {
    int sizeA = A->val.size();
	for (int i = 0; i < sizeA; i ++) {
        double value = A->val[i].second;
        if ( fabs(value) < threshold ) {
            // remove this index:value pair
            A->val.erase(A->val.begin()+i);
            -- i; -- sizeA;
        }
	}
}
void esmat_trim (Esmat* A) { esmat_trim(A, TRIM_THRESHOLD); }

double esmat_frob_prod (double** dist_mat, Esmat * A) {
    double result = 0.0;
    int sizeA = A->val.size();
    for (int i = 0; i < sizeA; i++) {
        int row_index = A->val[i].first % A->nCols;
        int col_index = A->val[i].first / A->nCols;
        result += dist_mat[row_index][col_index] * A->val[i].second;
    }
    return result;
}

//=========================================================
// The followings are computational frameworks.
// Do not modify them unless you know exactly what you are
// doing. 
// ========================================================
/* Framework of unary operation for Esmat A */
double esmat_unary_operate (Esmat * A, Operator opt) {
    double result = 0.0;
    int sizeA = A->val.size();
    for (int i = 0; i < sizeA; i++) 
        result = opt(A->val[i].second, result);
    return result;
}
/* Framework of binary operation for Esmat A */
void esmat_bin_operate (Esmat* A, Esmat* B, Esmat* dest, Operator opt) {
    assert (esmat_isValid (A, B, 1));

    dest->nRows = A->nRows;
    dest->nCols = A->nCols;
    dest->val.clear();

    int sizeA = A->val.size();
    int sizeB = B->val.size();
    int indexA, indexB;
    double value;

    int i = 0; int j = 0;
    while (i < sizeA && j < sizeB) {
        indexA = A->val[i].first;
        indexB = B->val[j].first;

        if (indexA < indexB) {
            value = opt(A->val[i].second, 0);
            dest->val.push_back(make_pair(indexA, value));
            ++i; 
        } else if (indexA > indexB) {
            value = opt(0, B->val[j].second);
            dest->val.push_back(make_pair(indexB, value));
            ++j;
        } else { // equality
            value = opt(A->val[i].second, B->val[j].second);
            dest->val.push_back(make_pair(indexA, value));
            ++i; ++j;
        }
    }

    if (i == sizeA && j < sizeB) {
        for (; j < sizeB; j ++) {
            indexB = B->val[j].first;
            value = opt(0, B->val[j].second);
            dest->val.push_back(make_pair(indexB, value));
        }
    } else if (j == sizeB && i < sizeA) {
        for (; i < sizeA; i ++) {
            indexA = A->val[i].first;
            value = opt(A->val[i].second, 0);
            dest->val.push_back(make_pair(indexA, value));
        }
    }
}
/* Framework for operating over each row of esmat A */
void esmat_operate_row (Esmat* A, Esmat* dest, Operator opt, double init_value) {
    int sizeA = A->val.size();
    // set up dest esmat, here assume it has been initialized
    dest->nRows = A->nRows;
    dest->nCols = 1;
    dest->val.clear();

    vector<double> temp (A->nRows, init_value);
    
    for (int i = 0; i < sizeA; i ++) {
        int row_idx = A->val[i].first % A->nRows;
        double value = A->val[i].second;
        temp[row_idx] = opt(value, temp[row_idx]);
    }

    for (int j = 0; j < A->nRows; j ++) {
        // trim very small term
        dest->val.push_back(make_pair(j, temp[j]));
    }
}
/* Framework for operating over each column of esmat A */
void esmat_operate_col (Esmat* A, Esmat* dest, Operator opt, double init_value) {
    int sizeA = A->val.size();
    // set up dest esmat, here assume it has been initialized
    dest->nRows = 1;
    dest->nCols = A->nCols;
    dest->val.clear();

    vector<double> temp (A->nCols, init_value);
    vector< vector<int> > counter (A->nCols, vector<int> ());
    
    for (int i = 0; i < sizeA; i ++) {
        int col_idx = A->val[i].first / A->nRows;
        temp[col_idx] = opt(A->val[i].second, temp[col_idx]);
        counter[col_idx].push_back(1);
    }
    for (int j = 0; j < A->nCols; j ++) {
        if (counter[j].size() < A->nRows) {
            temp[j] = opt(0, temp[j]);
        }
        // trim very small term
        dest->val.push_back(make_pair(j, temp[j]));
    }
}
void esmat_compare_col (Esmat* A, Esmat* dest, Comparator cmp) {
    int sizeA = A->val.size();
    dest->nRows = A->nRows;
    dest->nCols = A->nCols;
    dest->val.clear();

    double opt_value = 0;
    int opt_col_index = -1;
    int opt_index = -1;
    int sparse_index = -1;
    int prev_esmat_index = -1;
    
    for (int i = 0; i < sizeA; i++) {
        int esmat_index = A->val[i].first;
        int col_index = esmat_index / A->nRows;
        int row_index = esmat_index % A->nRows;
        double value = A->val[i].second;
        if (col_index > opt_col_index) {
            if (opt_index >= 0 && opt_col_index >= 0) {
                if (cmp(0, opt_value) && sparse_index >= 0) {
                    // cout << "sparse_index: " << sparse_index << endl;
                    dest->val.push_back(make_pair(opt_col_index*dest->nRows+sparse_index, 1));
                } else {
                  //  cout << "push: " << opt_col_index*dest->nRows+opt_index << endl;
                    dest->val.push_back(make_pair(opt_col_index*dest->nRows+opt_index, 1));
                }
                for (int j = opt_col_index+1; j < col_index; j ++) {
                    dest->val.push_back(make_pair(j*dest->nRows, 1));
                }
                if (row_index > 0) 
                    sparse_index = 0;
                else  // row_index == 0, first element
                    sparse_index = -1;
            }
            opt_col_index = col_index;
            opt_value = value;
           //  cout << "new opt_value: " << opt_value << endl;
            opt_index = row_index;
        } else if (col_index == opt_col_index) {
            if (cmp(value, opt_value)) {
                opt_value = value;
                opt_index = row_index;   
            }
           // cout << value << "," << opt_value  << endl;
            if (esmat_index > prev_esmat_index + 1) {
                if (sparse_index < 0) 
                    sparse_index = prev_esmat_index % dest->nRows + 1;
            }
        } else {
            assert(false);
        }
        prev_esmat_index = esmat_index;
    }
    if (cmp(0, opt_value) && sparse_index >= 0) {
        // cout << "sparse_index: " << sparse_index << endl;
        dest->val.push_back(make_pair(sparse_index, 1));
    } else {
        // cout <<  opt_col_index*dest->nRows+opt_index << endl;
        dest->val.push_back(make_pair(opt_col_index*dest->nRows+opt_index, 1));
    }
    for (int j = opt_col_index+1; j < A->nCols; j ++) {
        dest->val.push_back(make_pair(j*dest->nRows, 1));
    }
}
int find_first_zero_index (vector<int> vec, int max_index) {
    int size = vec.size();
    if (size == 0) {
        return -1; // no zero entry
    } else if (vec[0] > 0) {
        return 0;
    } else if (size == max_index) {
        return -1;
    }
    for (int i = 1; i < size; i ++) {
        if (vec[i] == vec[i-1]+1) {
            continue;
        } else {
            return vec[i-1]+1;
        }
    }
    return vec[size-1]+1;
}
void esmat_compare_row (Esmat* A, Esmat* dest, Comparator cmp) {
    int sizeA = A->val.size();
    dest->nRows = A->nRows;
    dest->nCols = A->nCols;
    dest->val.clear();

    if (sizeA == 0 && dest->nCols > 0) {
        for (int i = 0; i < A->nRows; i ++) {
            dest->val.push_back(make_pair(i, 1));
        }
        return ;
    }

    vector<double> opt_value (A->nRows, 0.0);
    vector<int> opt_index (A->nRows, -1);
    vector< vector<int> > counter (A->nRows, vector<int> ()); 

    for (int i = 0; i < sizeA; i ++) {
        int esmat_index = A->val[i].first;
        double value = A->val[i].second;
        int col_index = esmat_index / A->nRows;
        int row_index = esmat_index % A->nRows;
        counter[row_index].push_back(col_index);
        if (opt_index[row_index] < 0) { // first element on this row
            opt_value[row_index] = value;
            opt_index[row_index] = col_index;
        } else if (cmp(value, opt_value[row_index])) { // not first element
            opt_value[row_index] = value;
            opt_index[row_index] = col_index;
        }
    }
    for (int i = 0; i < A->nRows; i ++) {
        if (counter[i].size() < A->nCols && 
                (cmp(0, opt_value[i]) || opt_index[i] < 0)) {
            int sparse_index = find_first_zero_index (counter[i], A->nCols);
            dest->val.push_back(make_pair(sparse_index*(dest->nRows)+i, 1));
            continue;
        } else if (opt_index[i] >= 0)
            dest->val.push_back(make_pair((opt_index[i]*(dest->nRows))+i, 1));
    }
    esmat_align (dest);
}
/* Sum of all element on one matrix */
double esmat_sum (Esmat* A) 
{ return esmat_unary_operate (A, sum); }
/* Frobenius norm of one extensible sparse matrices */
double esmat_frob_norm (Esmat* A) 
{ return esmat_unary_operate (A, power2); }
/* Compute dummy term */
double esmat_compute_dummy (Esmat* A, double r) {
    int R = A->nRows;
    vector<double> temp_vec (R, 0.0);
    int size = A->val.size();
    for (int i = 0; i < size; i ++) {
        int esmat_index = A->val[i].first;
        int row_index = esmat_index % A->nRows;
        int col_index = esmat_index / A->nRows;
        double value = A->val[i].second;
        temp_vec[row_index] += value;
    }
    double dummy= 0.0;
    for (int i = 0; i < R; i ++) {
        dummy += r * (1.0 - temp_vec[i]);
    }
    return dummy; 
}

/* Add and Subtract two extensible sparse matrices */
void esmat_add (Esmat* A, Esmat* B, Esmat* dest) 
{ 
    esmat_bin_operate (A, B, dest, sum);
}
void esmat_sub (Esmat* A, Esmat* B, Esmat* dest) 
{ 
    esmat_bin_operate (A, B, dest, diff);
}

/* sum over column or row elements */
void esmat_sum_col (Esmat* A, Esmat* dest) 
{ esmat_operate_col (A, dest, sum, 0.0); }
void esmat_sum_row (Esmat* A, Esmat* dest) 
{ esmat_operate_row (A, dest, sum, 0.0); }

void esmat_count_over_col (Esmat* A, Esmat* dest) 
{ esmat_operate_col (A, dest, count, 0.0); }
void esmat_max_over_col (Esmat* A, Esmat* dest) 
{ esmat_operate_col (A, dest, max, -INF); }
void esmat_min_over_col (Esmat* A, Esmat* dest) 
{ esmat_operate_col (A, dest, min, INF); }

void esmat_min_row (Esmat* A, Esmat* dest) 
{ esmat_compare_row (A, dest, lt); }
void esmat_max_row (Esmat* A, Esmat* dest) 
{ esmat_compare_row (A, dest, gt); }
void esmat_min_col (Esmat* A, Esmat* dest) 
{ esmat_compare_col (A, dest, lt); }
void esmat_max_col (Esmat* A, Esmat* dest) 
{ esmat_compare_col (A, dest, gt); }
