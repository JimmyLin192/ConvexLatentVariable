#include<iostream>
#include<cassert>
#include"math.h"
#include<fstream>
#include<string>

using namespace std;
double MAT_DOUBLE_INF = 1e300;

void mat_zeros (double ** src, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) 
        for (int j = 0; j < nCols; j ++) 
            src[i][j] = 0;
}
double ** mat_init (int nRows, int nCols) {
    double ** res = new double * [nRows];
    for (int i = 0; i < nRows; i ++) 
        res[i] = new double [nCols];
    mat_zeros(res, nRows, nCols);
    return res;
}
double ** mat_read (char* fname, int R, int C) {
   	ifstream fin(fname);
    double ** result = mat_init (R, C);
    string word; 
    double val;
    for (int i = 0; i < R; i ++) {
        for (int j = 0; j < C; j ++) {
            fin >> word;
            if (word.compare("inf") == 0) 
                val = MAT_DOUBLE_INF;
            else 
                val = atof(word.c_str());
            result[i][j] = val;
        }
    }
    fin.close();
    return result;
}

void mat_free (double ** src, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        delete [] src[i];
    }
}

void mat_add (double ** src1, double ** src2, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = src1[i][j] + src2[i][j];
        }
    }
}

void mat_sub (double ** src1, double ** src2, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = src1[i][j] - src2[i][j];
        }
    }
}

void mat_times (double ** src1, double ** src2, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = src1[i][j]* src2[i][j];
        }
    }
}
double mat_frob_dot (double ** src1, double ** src2, int nRows, int nCols) {
    double frob_prod = 0.0;
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            frob_prod += src1[i][j]* src2[i][j];
        }
    }
    return frob_prod;
}
void mat_dot (double scalar, double ** src, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = scalar * src[i][j];
        }
    }
}

// tranpose first matrix and then compute dot product 
void mat_tdot (double ** src1, double ** src2, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = src1[i][j]*src2[i][j];
        }
    }
}

double mat_sum (double ** src, int nRows, int nCols) {
    double sum = 0.0;
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            sum += src[i][j];
        }
    }
    return sum;
}
void mat_sum_row (double ** src, double * dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        double sum = 0.0;
        for (int j = 0; j < nCols; j ++) 
            sum += src[i][j];
        dest[i] = sum;
    }
}

void mat_sum_col(double ** src, double * dest, int nRows, int nCols) {
    for(int j = 0;j < nCols; j ++) dest[j] = 0.0;
    for (int i = 0; i < nRows; i ++) 
        for (int j = 0; j < nCols; j ++) 
            dest[j] += src[i][j];
}

double mat_dot (double * vec1, double * vec2, int N) {
    double sum = 0.0;
    for (int i = 0; i < N; i ++) 
        sum += vec1[i] * vec2[i];
    return sum;
}
string mat_toString (double ** src, int nRows, int nCols) {
    string field_seperator = " ";
    string line_separator = "\n";
    string str = "";
    char buf[1000];
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) { 
            if (src[i][j] > 1e299)
                str += "inf" + field_seperator;
            else {
                sprintf(buf, "%f", src[i][j]);
                string temp = string(buf);
                str += temp;
                if (j < nCols - 1)
                    str += field_seperator;
            }
        }
        str += line_separator;
    }
    return str;
}
void mat_print (double ** src, int nRows, int nCols) {
    cout << mat_toString (src, nRows, nCols);
}

void mat_copy (double ** src, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = src[i][j];
        }
    }
}

void mat_max_col (double ** src, double ** dest, int nRows, int nCols) {
    // we assume that the given dest is all-zero mat
    for (int j = 0; j < nCols; j ++) {
        int max_index = -1;
        double max_value = -1e300;
        for (int i = 0; i < nRows; i ++) {
            if (src[i][j] > max_value) {
                max_index = i;
                max_value = src[i][j];
            }
            dest[i][j] = 0;
        }
        dest[max_index][j] = 1;
    }
}
void mat_max_col (double ** src, double * dest, int nRows, int nCols) {
    for (int j = 0; j < nCols; j ++) {
        int max_index = -1;
        double max_value = -1e300;
        for (int i = 0; i < nRows; i ++) {
            if (src[i][j] > max_value) {
                max_index = i;
                max_value = src[i][j];
            }
        }
        dest[j] = max_value;
    }
}
void mat_min_row (double ** src, double ** dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        int min_index = -1;
        double min_value = 10e300;
        for (int j = 0; j < nCols; j ++) {
            if (src[i][j] < min_value) {
                min_index = j;
                min_value = src[i][j];
            }
            dest[i][j] = 0;
        }
        assert (min_index >= 0);
        dest[i][min_index] = 1;
    }
}
void mat_min_row (double ** src, double * dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        int min_index = -1;
        double min_value = 10e300;
        for (int j = 0; j < nCols; j ++) {
            if (src[i][j] < min_value) {
                min_index = j;
                min_value = src[i][j];
            }
        }
        dest[i] = min_value;
    }
}
void mat_min_index_row (double ** src, double * dest, int nRows, int nCols) {
    for (int i = 0; i < nRows; i ++) {
        int min_index = -1;
        double min_value = 10e300;
        for (int j = 0; j < nCols; j ++) {
            if (src[i][j] < min_value) {
                min_index = j;
                min_value = src[i][j];
            }
        }
        dest[i] = min_index;
    }
}
double mat_norm2 (double ** src, int nRows, int nCols) {
    double sum = 0.0;
    for (int i = 0; i < nRows; i ++) 
        for (int j = 0; j < nCols; j ++) 
            sum += src[i][j] * src[i][j];
    return sum;
}

void mat_set_all (double ** mat, double value, int R, int C) {
	for (int i = 0; i < R; i++) 
		for (int j = 0; j < C; j++) 
            mat[i][j] = value;
}
void mat_set(double ** mat, double ** z, int R, int C) {
	for (int i = 0; i < R; i++) 
		for (int j = 0; j < C; j++) 
            mat[i][j] = z[i][j];
}

// TODO: mat_write and mat_read
void trim (double** mat, int R, int C) {
	for (int i = 0; i < R; i++) 
		for (int j = 0; j < C; j++) 
			if( fabs(mat[i][j]) < 1e-5 )
				mat[i][j] = 0.0;
}
