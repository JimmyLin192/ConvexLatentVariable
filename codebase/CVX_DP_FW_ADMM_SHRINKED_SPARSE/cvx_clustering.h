/*###############################################################
## MODULE: cvx_clustering.cpp
## VERSION: 1.0 
## SINCE 2014-06-14
## AUTHOR Jimmy Lin (xl5224) - JimmyLin@utexas.edu  
## DESCRIPTION: 
##     This file includes problem-specific data structure and
## utility function.
#################################################################
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/

#include <cassert>
#include <queue>
#include <time.h>
#include "../util.h"

const double FRANK_WOLFE_TOL = 1e-20;
const double ADMM_EPS = 1e-5;
typedef double (* dist_func) (Instance*, Instance*, int); 
const double r = 10000.0;

class cell {
    public:
        int index;
        double w, z, y, grad; 
        cell () {}
        cell (int i, double w, double z, double y, double grad) {
            this->index = i;
            this->w = w;
            this->z = z;
            this->y = y;
            this->grad = grad;
        }
        void copy_from (cell other) {
            this->index = other.index;
            this->w = other.w;
            this->z = other.z;
            this->y = other.y;
            this->grad = other.grad;

        }
};
bool cell_grad_Comparator (const cell& firstElem, const cell& secondElem) {
    // sort cell by second element with *increasing order* of grad
    return firstElem.grad > secondElem.grad;
}
class CellCompare
{
    public:
        bool operator() (cell obj1, cell obj2)
        {
            return cell_grad_Comparator (obj1, obj2);
        }
};

/* \lambda_g \sumk \maxn |\wnk| */
double compute_group_lasso (Esmat* w, double lambda) {
    if (w->val.size() == 0) 
        return 0.0;
    Esmat* maxn = esmat_init (1, w->nCols);
    Esmat* sumk = esmat_init (1, 1);
    esmat_max_over_col (w, maxn);
    esmat_sum_row (maxn, sumk);
    double lasso = -INF;
    if (sumk->val.size() > 0)
        lasso = lambda * sumk->val[0].second; 
    else 
        lasso = 0.0;
    esmat_free (sumk);
    esmat_free (maxn);
    return lasso;
}

void subproblem_objective (double** dist_mat, Esmat* y, Esmat* z, Esmat* w, double rho, int N, double lambda) {
    Esmat* diff = esmat_init (N, N);
    esmat_zeros (diff);
    // reg = 0.5 * sum_k max_n | w_nk |  -> group-lasso
    double lasso = compute_group_lasso(w, lambda); 
    // loss = 0.5 * sum_n sum_k (w_nk * d^2_nk) -> loss
    double loss = 0.5 * esmat_frob_prod (dist_mat, w);
    // linear = y_1^T dot (w_1 - z) -> linear
    esmat_sub (w, z, diff); // temp = w_1 - z_1
    double linear = esmat_frob_prod (y, diff);
    // quadratic = 0.5 * rho * || w_1 - z_1 ||^2 -> quadratic
    double quadratic = 0.5 * rho * esmat_frob_norm (diff);
    // dummy = r dot (1 - sum_k w_nk) -> dummy
    double dummy_penalty = esmat_compute_dummy (w, r);

    // double total = loss+linear+quadratic+dummy_penalty;
    cout << "(loss, lasso, linear, quadratic, dummy, total) = (" 
        << loss << ", " << lasso << ", " << linear << ", " <<
        quadratic << ", " << dummy_penalty << ")" << endl;
    /*
    if (dummy_penalty < -100000) {
        cout << "----------------------" << endl;
        cout << "[dummy_w < 0]" << endl;
        cout << esmat_toString (w);
        cout << "----------------------" << endl;
        int R = w->nRows;
        vector<double> temp_vec (R, 0.0);
        int size = w->val.size();
        for (int i = 0; i < size; i ++) {
            int esmat_index = w->val[i].first;
            int row_index = esmat_index % w->nRows;
            int col_index = esmat_index / w->nRows;
            double value = w->val[i].second;
            temp_vec[row_index] += value;
        }
        double dummy= 0.0;
        for (int i = 0; i < R; i ++) {
            dummy += r * (1.0 - temp_vec[i]);
            cout << "i=" << i << ", sum=" << temp_vec[i] << endl;
        }
        exit(0);
    }
    */
    esmat_free (diff);
    // return total;
}
double overall_objective (double** dist_mat, double lambda, int N, Esmat* z) {
    // N is number of entities in "data", and z is N by N.
    // z is current valid solution (average of w_1 and w_2)
    // STEP ONE: compute 
    //     loss = sum_i sum_j z[i][j] * dist_mat[i][j]
    double loss = 0.5 * esmat_frob_prod(dist_mat, z);
    cout << "loss=" << loss;
    // STEP TWO: compute dummy loss
    // sum4 = r dot (1 - sum_k w_nk) -> dummy
    double dummy_penalty = esmat_compute_dummy (z, r);
    cout << ", dummy= " << dummy_penalty;
    // STEP THREE: compute group-lasso regularization
    double reg = compute_group_lasso (z, lambda); 
    cout << ", reg=" << reg ;
    double overall = loss + reg + dummy_penalty;
    cout << ", overall=" <<  overall << endl;
    return loss + reg;
}

/* Compute the mutual distance of input instances contained within "data" */
void compute_dist_mat (vector<Instance*>& data, Esmat* dist_mat, int N, int D, dist_func df, bool isSym) {
    for (int j = 0; j < N; j ++) {
        Instance * muj = data[j];
        for (int i = 0; i < N; i ++) {
            Instance * xi = data[i];
            double dist_value = df (xi, muj, D);
            dist_mat->val.push_back(make_pair(j*N+i,dist_value));
        }
    }
}
void compute_dist_mat (vector<Instance*>& data, double** dist_mat, int N, int D, dist_func df, bool isSym) {
    for (int i = 0; i < N; i ++) {
        Instance * xi = data[i];
        for (int j = 0; j < N; j ++) {
            Instance * muj = data[j];
            double dist_value = df (xi, muj, D);
            dist_mat[i][j] = dist_value;
        }
    }
}
void group_lasso_solver (Esmat* Y, Esmat* Z, Esmat* w, double RHO, double lambda) {
    // STEP ONE: compute the optimal solution for truncated problem
    Esmat* wbar = esmat_init (w);
    Esmat* temp = esmat_init (w);
    esmat_scalar_mult (RHO, Z, temp); // temp = RHO * z
    esmat_sub (temp, Y, wbar); // wbar = RHO * z - y
    esmat_scalar_mult (1.0/RHO, wbar); // wbar = (RHO * z - y) / RHO
    esmat_free (temp);
    esmat_zeros (w);
    // STEP TWO: find the closed-form solution for second subproblem
    int SIZE = wbar->val.size();
    if (SIZE == 0)  return ;
    int R = wbar->nRows; int C = wbar->nCols;
    vector< vector< pair<int,double> > > alpha_vec (C, vector< pair<int,double> > ());
    vector<int> nValidAlpha (C, 0);
    for (int i = 0; i < SIZE; i ++) {
        int wbar_esmat_index = wbar->val[i].first;
        int wbar_row_index = wbar_esmat_index % R;
        int wbar_col_index = wbar_esmat_index / R;
        double value = wbar->val[i].second;
        alpha_vec[wbar_col_index].push_back (make_pair(wbar_esmat_index,fabs(value)));
        ++ nValidAlpha[wbar_col_index];
    }

    for (int j = 0; j < C; j ++) {
        if (nValidAlpha[j] <= 0) continue;
        // a) sort existing temp_vec
        std::sort (alpha_vec[j].begin(), alpha_vec[j].end(), pair_Second_Elem_Comparator);
        // b) find mstar
        int mstar = 0; // number of elements supporting the sky
        double separator; 
        double max_avg_term = -INF, tmp_avg_term;
        double sum_alpha = 0.0;
        for (int v = 0; v < nValidAlpha[j]; v ++) {
            sum_alpha += alpha_vec[j][v].second;
            tmp_avg_term = (sum_alpha - lambda) / (v + 1.0);
            if ( tmp_avg_term > max_avg_term ) {
                separator = alpha_vec[j][v].second;
                max_avg_term = tmp_avg_term;
                ++ mstar;
            }
        }
        if (max_avg_term < 0) { // include all elements 
            mstar = R;
            max_avg_term = (sum_alpha - lambda) / R;
        }
        if (mstar <= 0 ) {
            cout << "mstar <= 0" << endl;
        } else if ( max_avg_term < 0.0) {
            // here we set the whole column of output w to zeros
            // that is to do nothing for esmat for this column
            ;
        } else {
            for (int i = 0; i < nValidAlpha[j]; i ++) {
                double value = alpha_vec[j][i].second;
                if (fabs(value) >= separator) 
                    alpha_vec[j][i].second = max_avg_term;
                else {
                    alpha_vec[j][i].second = max(value, 0.0);
                }
            }
            std::sort (alpha_vec[j].begin(), alpha_vec[j].end(), pair_First_Elem_Comparator);
            for (int i = 0; i < nValidAlpha[j]; i++) 
                w->val.push_back(alpha_vec[j][i]); 
        }
    }
    // STEP THREE: recollect temporary variable - wbar
    esmat_free (wbar);
}
/*
void group_lasso_solver (Esmat* Y, Esmat* Z, Esmat* w, double RHO, double lambda) {
    // STEP ONE: compute the optimal solution for truncated problem
    Esmat* wbar = esmat_init (w);
    Esmat* temp = esmat_init (w);
    esmat_scalar_mult (RHO, Z, temp); // temp = RHO * z
    esmat_sub (temp, Y, wbar); // wbar = RHO * z - y
    esmat_scalar_mult (1.0/RHO, wbar); // wbar = (RHO * z - y) / RHO
    esmat_free (temp);
    esmat_zeros (w);
    // STEP TWO: find the closed-form solution for second subproblem
    int SIZE = wbar->val.size();
    int R = wbar->nRows; int C = wbar->nCols;
    if (SIZE == 0)  return ;
    // i is index of element in esmat->val, j is column index
    int i = 0; int j = 0;
    int begin_idx, end_idx;
    int col_es_begin = 0;
    vector< pair<int,double> >  alpha_vec;
    bool last_go = false;
    while ((i < SIZE && j < C) || last_go) {
        begin_idx = j * R;  // begin index of current column
        end_idx = (j+1) * R; // end index of current column 
        int esWBAR_index = -1;
        if (!last_go) esWBAR_index = wbar->val[i].first;
        // cout << "i: " << i << " , j: " << j << endl;
        if (last_go || esWBAR_index >= end_idx ) {
            int nValidAlpha = alpha_vec.size();
            // cout << "nValidAlpha: " << nValidAlpha << endl;
            if (nValidAlpha == 0) {
                ++ j; 
                continue;
            }
            // a) sort existing temp_vec
            std::sort (alpha_vec.begin(), alpha_vec.end(), pair_Second_Elem_Comparator);
            // b) find mstar
            int mstar = 0; // number of elements supporting the sky
            double separator; 
            double max_avg_term = -INF, tmp_avg_term;
            double sum_alpha = 0.0;
            for (int v = 0; v < nValidAlpha; v ++) {
                sum_alpha += alpha_vec[v].second;
                tmp_avg_term = (sum_alpha - lambda) / (v + 1.0);
                // cout << "tmp_avg_term: " << tmp_avg_term << endl;
                if ( tmp_avg_term > max_avg_term ) {
                    separator = alpha_vec[v].second;
                    max_avg_term = tmp_avg_term;
                    ++ mstar;
                }
            }
            if (max_avg_term < 0) { // include all elements 
                mstar = R;
                max_avg_term = (sum_alpha - lambda) / R;
            }
            // cout << "mstar: " << mstar << ", max_term: " << max_avg_term << endl;
            // c) assign closed-form solution of current column to w
            if (mstar <= 0 || max_avg_term < 0.0) {
                ; // this column of w is all-zero, hence we do nothing for that 
            } else {
                for (int esi = col_es_begin; wbar->val[esi].first < end_idx; esi ++) {
                    if (last_go && esi == SIZE) break;
                    double pos = wbar->val[esi].first;
                    double value = wbar->val[esi].second;
                    if (fabs(value) >= separator) 
                        w->val.push_back(make_pair(pos, max_avg_term));
                    else {
                        w->val.push_back(make_pair(pos, max(value, 0.0)));
                    }
                }
            }
            // d) clear all elements in alpha_vec 
            alpha_vec.clear();
           //  cout << "old_j: " << j << endl;
            if (last_go) break;
            // e) push current element to the cleared alpha_vec
            double value = wbar->val[i].second;
            alpha_vec.push_back (make_pair(esWBAR_index % R, fabs(value)));
            // f) go to operate next element and next column
            col_es_begin = i;
            ++ i; ++ j;
        } else if (esWBAR_index >= begin_idx) {
            // a) push current element to the cleared temp_vec
            double value = wbar->val[i].second;
            alpha_vec.push_back (make_pair(esWBAR_index % R, fabs(value)));
            // b) go to operate next element with fixed column index (j)
            ++ i; 
        } else { // impossible to occur
            assert (false);
        }
        if (i == SIZE) {last_go = true; }
    }
    // STEP THREE: recollect temporary variable - wbar
    esmat_free (wbar);
}
*/
