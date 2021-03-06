/*###############################################################
## MODULE: HDP.cpp
## VERSION: 2.0 
## SINCE 2014-07-21
## AUTHOR:
##     Jimmy Lin (xl5224) - JimmyLin@utexas.edu  
##
## DESCRIPTION: 
##     Convex relaxation for HDP resolution
################################################################# 
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/

#include "HDP.h"

/* algorithmic options */ 
#define EXACT_LINE_SEARCH  // comment this to use inexact search

/* dumping options */
// #define FRANK_WOLFE_DUMP
// #define EXACT_LINE_SEARCH_DUMP
// #define BLOCKWISE_DUMP
// #define NTOPIC_DUMP

double sign (int input) {
    if (input > 0) return 1.0;
    else if ( input < 0 ) return -1.0;
    else return 0.0;
}

bool pair_Second_Elem_Comparator (const std::pair<int, double>& firstElem, const std::pair<int, double>& secondElem) {
    // sort pairs by second element with *decreasing order*
    return firstElem.second > secondElem.second;
}

/*{{{*/
/*
int get_nCentroids (double ** W, int nRows, int nCols) {

    int nTopics = 0;

    double * sum_belonging = new double [nCols];

    mat_max_col (W, sum_belonging, nRows, nCols);

    for (int i = 0; i < nCols; i ++ ) {
        if (sum_belonging[i] > 3e-1) { 
            nCentroids ++;
        }
    }
    
    delete[] sum_belonging;

    return nTopics;
}

vector<int> get_all_centroids(double ** W, int nRows, int nCols) {

    std::vector<int> topics;

    double * sum_belonging = new double [nCols];
    for (int i = 0; i < nCols; i ++) {
        sum_belonging[i] = 0.0;
    }

    mat_max_col (W, sum_belonging, nRows, nCols);

    for (int i = 0; i < nCols; i ++ ) {
        if (sum_belonging[i] > 3e-1) {
            centroids.push_back(i);
        }
    }
    
    delete[] sum_belonging;

    return topics;
}
*/
/*}}}*/

/* dummy_penalty = r dot (1 - sum_k w_nk) */
double get_dummy_loss (Esmat* Z) {
    Esmat* temp_vec = esmat_init (Z->nRows, 1);
    esmat_sum_row (Z, temp_vec);
    double dummy_loss = esmat_compute_dummy (temp_vec);
    esmat_free (temp_vec);
    return dummy_loss;
}
/* \lambda_g \sumk \maxn |\wnk| */
double get_global_topic_reg (Esmat* absZ, double lambda) {
    Esmat* maxn = esmat_init (1, absZ->nCols);
    Esmat* sumk = esmat_init (1, 1);
    esmat_max_over_col (absZ, maxn);
    esmat_sum_row (maxn, sumk);
    double global_topic_reg = -INF;
    if (sumk->val.size() > 0)
        global_topic_reg = lambda * sumk->val[0].second; 
    else 
        global_topic_reg = 0.0;
    esmat_free (sumk);
    esmat_free (maxn);
    return global_topic_reg;
}
/* \lambdal \sum_d \sum_k \underset{n \in d}{\text{max}} |\wnk| */
double get_local_topic_reg (Esmat* absZ, double lambda, vector< pair<int,int> >* doc_lookup) {
    // STEP ONE: initialize sub matrix for each document
    int nDocs = doc_lookup->size();
    vector<Esmat*> sub_absZ (nDocs);
    for (int d = 0; d < nDocs; d ++) {
        sub_absZ[d] = esmat_init (0,0);
    }

    // STEP TWO: separate entire Z to submat Z
    esmat_submat_row (absZ, sub_absZ, doc_lookup);

    // STEP THREE: compute global topic regularizer for each localized doc
    double local_topic_reg = 0.0;
    for (int d = 0; d < nDocs; d ++) {
        local_topic_reg += get_global_topic_reg (sub_absZ[d], lambda); 
    }

    // Final: free resource
    esmat_free_all (sub_absZ);
    return local_topic_reg;
}
/* \lambdab \sum_k \sum_{w \in voc(k)} \underset{word(n) = w}{\text{max}} |\wnk|  */
double get_coverage_reg (Esmat* absZ, double lambda, vector<int>* word_lookup, vector< vector<int> >* voc_lookup) {
    // STEP ONE: initialize sub matrix for each vocabulary
    int nVocs = voc_lookup->size();
    vector<Esmat*> sub_absZ (nVocs);
    esmat_init_all (&sub_absZ);
    esmat_submat_row (absZ, sub_absZ, word_lookup, voc_lookup);
    double coverage_reg = 0.0;
    for (int v = 0; v < nVocs; v ++) {
        coverage_reg += get_global_topic_reg (sub_absZ[v], lambda); 
    }
    esmat_free_all (sub_absZ);
    return coverage_reg;
}

double subproblem_objective (int prob_index, Esmat* Y, Esmat* Z, Esmat* W, double RHO, double lambda, Lookups* tables) {

    vector< pair<int,int> >* doc_lookup = tables->doc_lookup;
    vector<int>* word_lookup = tables->word_lookup; 
    vector< vector<int> >* voc_lookup = tables->voc_lookup;

    // STEP ONE: compute main term
    double main = -1.0;
    if (prob_index == 1) {
        // dummy_penalty = r dot (1 - sum_k w_nk)
        main = get_dummy_loss (W);
    } else {
        Esmat* absW = esmat_init (W);
        esmat_abs (W, absW);
        if (prob_index == 2) {
            main = get_global_topic_reg (absW, lambda);
        } else if (prob_index == 3) {
            main = get_local_topic_reg (absW, lambda, doc_lookup);
        } else if (prob_index == 4) {
            main = get_coverage_reg (absW, lambda, word_lookup, voc_lookup);
        }
        esmat_free (absW);
    }
    Esmat* w_minus_z = esmat_init (W);
    // STEP TWO: compute linear term: linear = y^T dot (w - z) 
    esmat_sub (W, Z, w_minus_z); // temp = w - z
    double linear = esmat_fdot (Y, w_minus_z);
    // STEP THREE: compute quadratic term: quadratic = 0.5 * RHO * || w - z ||^2 
    double quadratic = 0.5 * RHO * esmat_fnorm (w_minus_z);
    esmat_free (w_minus_z);
    /*
#ifdef FRANK_WOLFE_DUMP
cout << "[Frank_wolfe] (loss, linear, quadratic, dummy, total) = (" 
<< sum1 << ", " << sum2 << ", " << sum3 << ", " << dummy_penalty << ", " << total
<<  ")" << endl;
#endif
*/
    return main + linear + quadratic;
}
double original_objective (Esmat* Z, vector<double> LAMBDAs, Lookups* tables) {
    vector< pair<int,int> >* doc_lookup = tables->doc_lookup;
    vector<int>* word_lookup = tables->word_lookup; 
    vector< vector<int> >* voc_lookup = tables->voc_lookup;

    Esmat* absZ = esmat_init (Z);
    esmat_abs (Z, absZ);
    // STEP ONE: compute dummy loss
    double dummy = get_dummy_loss (Z);
    // cout << "dummy =" << dummy << endl;

    // STEP TWO: compute "GLOBAL TOPIC" group-lasso regularization
    double global_topic_reg = get_global_topic_reg (absZ, LAMBDAs[0]);
    // STEP THREE: compute "LOCAL TOPIC" group-lasso regularization
    double local_topic_reg = get_local_topic_reg (absZ, LAMBDAs[1], doc_lookup);
    // STEP FOUR: TODO compute "TOPIC COVERAGE" group-lasso regularization
    double coverage_reg = get_coverage_reg (absZ, LAMBDAs[2], word_lookup, voc_lookup);
    esmat_free (absZ); 
    return dummy + global_topic_reg + local_topic_reg + coverage_reg;
}
void frank_wolfe_solver (Esmat * Y_1, Esmat * Z_1, Esmat * w_1, double RHO, int N) {

    bool is_global_optimal_reached = false;

    // cout << "within frank_wolfe_solver" << endl;
    // This can be computed by using corner point. 
    Esmat * gradient = esmat_init (w_1);
    Esmat * s = esmat_init (w_1);

#ifndef EXACT_LINE_SEARCH
    int K = 300;
#else
    int K = 2;
    Esmat * w_minus_s = esmat_init (w_1);
    Esmat * w_minus_z = esmat_init (w_1);
#endif

    int k = 0; // iteration number
    double gamma; // step size
    double penalty;
    Esmat * tempS = esmat_init (w_1);

    //esmat_zeros (w_1);
    double sum1, sum2, sum3, sum4;
    // cout << "within frank_wolfe_solver: start iteration" << endl;
    while (k < K && !is_global_optimal_reached) {
        // STEP ONE: find s that minimizes <s, grad f>
        // gradient[i][j] = Y_1[i][j] + RHO * (w_1[i][j] - Z_1[i][j]) ;  //- r[i];
        esmat_sub (w_1, Z_1, tempS);
        esmat_scalar_mult (RHO, tempS);
        esmat_add (Y_1, tempS, gradient);
        esmat_min_row (gradient, s);

        /*
#ifdef FRANK_WOLFE_DUMP
        cout << "esmat_norm2 (w_1): " <<  esmat_norm2 (w_1) << endl;
        cout << "esmat_norm2 (Y_1): " <<  esmat_norm2 (Y_1) << endl;
        cout << "esmat_norm2 (gradient): " <<  esmat_norm2 (gradient) << endl;
        cout << "esmat_sum (s): " <<  esmat_sum (s) << endl;
#endif
*/
        // cout << "within frank_wolfe_solver: step one finished" << endl;

        // STEP TWO: apply exact or inexact line search to find solution
#ifndef EXACT_LINE_SEARCH
        // Here we use inexact line search
        gamma = 2.0 / (k + 2.0);
#else
        // Here we use exact line search 
        if (k == 0) {
            gamma = 1.0;
        } else {
        // gamma* = (sum1 + sum2 + sum3) / sum4, where
        // sum1 = 1/2 sum_n sum_k (w - s)_nk * || x_n - mu_k ||^2
        // sum2 = sum_n sum_k (w - s)_nk
        // sum3 = - RHO * sum_n sum_k  (w - z) 
        // sum4 = sum_n sum_k RHO * (s - w)
        esmat_sub (w_1, s, w_minus_s);
        esmat_sub (w_1, Z_1, w_minus_z);

        // NOTE: in case of ||w_1 - s||^2 = 0, not need to optimize anymore
        // since incremental term = w + gamma (s - w), and whatever gamma is,
        // w^(k+1) = w^(k), this would be equivalent to gamma = 0
        if (esmat_fnorm(w_minus_s) == 0) {
            gamma = 0;
            is_global_optimal_reached = true;
            // reach the exit condition, do not make more iteration
        } else {
            /*
            esmat_fdot (w_minus_s, dist_mat, tempS);
            sum1 = 0.5 * esmat_sum (tempS);
            */
            sum2 = esmat_fdot (Y_1, w_minus_s);
            sum3 = RHO * esmat_fdot (w_minus_z, w_minus_s);
            sum4 = RHO * esmat_fnorm (w_minus_s);
            // gamma should be within interval [0,1]
            gamma = (sum1 + sum2 + sum3) / sum4;

#ifdef FRANK_WOLFE_DUMP
            cout << "esmat_norm2 (w_minus_s)" << esmat_fnorm (w_minus_s) << endl;
            cout << "esmat_norm2 (w_minus_z)" << esmat_fnorm (w_minus_z) << endl;
#endif
            // cout << "within frank_wolfe_solver: step two finished" << endl;

#ifdef EXACT_LINE_SEARCH_DUMP
            cout << "[exact line search] (sum1, sum2, sum3, sum4, gamma) = ("
                << sum1 << ", " << sum2 << ", " << sum3 << ", " << sum4 << ", " << gamma
                << ")"
                << endl;
#endif
        }

        }
#endif
        // update the w^(k+1)
        esmat_scalar_mult (gamma, s); 
        esmat_scalar_mult (1.0-gamma, w_1);
        esmat_copy (w_1, tempS);
        esmat_add (tempS, s, w_1);

        // cout << "within frank_wolfe_solver: step three finished" << endl;
        // report the #iter and objective function
        /*
           cout << "[Frank-Wolfe] iteration: " << k << ", first_subpro_obj: " << penalty << endl;
        */

        k ++;
    // cout << "within frank_wolfe_solver: next iteration" << endl;
    }
    // cout << "within frank_wolfe_solver: to free gradient" << endl;
    esmat_free (gradient);
    // cout << "within frank_wolfe_solver: to free temps" << endl;
    esmat_free (tempS);
    // cout << "within frank_wolfe_solver: to free s " << endl;
    esmat_free (s);
    // cout << "end frank_wolfe_solver: finished! " << endl;
}

void group_lasso_solver (Esmat* Y, Esmat* Z, Esmat* w, double RHO, double lambda) {
    // STEP ONE: compute the optimal solution for truncated problem
    Esmat* wbar = esmat_init (w);
    esmat_zeros (wbar);
    esmat_scalar_mult (RHO, Z, wbar); // wbar = RHO * z
    esmat_sub (wbar, Y, wbar); // wbar = RHO * z - y
    esmat_scalar_mult (1.0/RHO, wbar); // wbar = (RHO * z - y) / RHO

    // STEP TWO: find the closed-form solution for second subproblem
    int SIZE = wbar->val.size();
    int R = wbar->nRows; int C = wbar->nCols;
    
    // i is index of element in esmat->val, j is column index
    int i = 0; int j = 0;
    int col_es_begin = wbar->val[0].first;
    vector< pair<int,double> > alpha_vec;
    while (i < SIZE && j < C) {
        int begin_idx = j * R;
        int end_idx = (j+1) * R;
        int esWBAR_index = wbar->val[i].first;
        if (esWBAR_index >= end_idx || i == SIZE - 1) { 
            // a) sort existing temp_vec
            std::sort (alpha_vec.begin(), alpha_vec.end(), pair_Second_Elem_Comparator);
            // b) find mstar
            int mstar = 0; // number of elements support the sky
            double separator;
            double max_term = -INF, new_term;
            double sum_alpha = 0.0;
            int nValidAlpha = alpha_vec.size();
            for (int v = 0; v < nValidAlpha; v ++) {
                sum_alpha += alpha_vec[v].second;
                new_term = (sum_alpha - lambda) / (v + 1.0);
                if ( new_term > max_term ) {
                    separator = alpha_vec[v].second;
                    max_term = new_term;
                    ++ mstar;
                }
            }
            // c) assign closed-form solution of current column to w
            if (nValidAlpha == 0 || max_term < 0) {
                ; // this column of w is all-zero, hence we do nothing for that 
            } else {
                for (int esi = col_es_begin; wbar->val[esi].first < end_idx; esi ++) {
                    double pos = wbar->val[esi].first;
                    double value = wbar->val[esi].second;
                    if (fabs(value) >= separator) 
                        w->val.push_back(make_pair(pos, max_term));
                    else 
                        w->val.push_back(make_pair(pos, max(value, 0.0)));
                }
            }
            // d) clear all elements in alpha_vec 
            alpha_vec.clear();
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
    }
    
    // report the #iter and objective function
    /*cout << "[Blockwise] sub2_objective: " << penalty << endl;
      cout << endl;*/

    // STEP THREE: recollect temporary variable - wbar
    esmat_free (wbar);
}

/* three subproblems that employed group_lasso_solver in different ways */
void global_topic_subproblem (Esmat* Y, Esmat* Z, Esmat* w, double RHO, double lambda) {
    group_lasso_solver (Y, Z, w, RHO, lambda);
}
void local_topic_subproblem (Esmat* Y, Esmat* Z, Esmat* w, double RHO, double lambda, Lookups* tables) {

    vector< pair<int,int> >* doc_lookup = tables->doc_lookup;
    vector<int>* word_lookup = tables->word_lookup; 
    vector< vector<int> >* voc_lookup = tables->voc_lookup;

    int nDocs = tables->nDocs;
    Esmat* tempW = esmat_init (w);
    vector<Esmat*> subW (nDocs); 
    vector<Esmat*> subY (nDocs);
    vector<Esmat*> subZ (nDocs);
    // STEP ZERO: initialize all submats
    for (int d = 0; d < nDocs; d ++) {
        subW[d] = esmat_init (0,0);
        subY[d] = esmat_init (0,0);
        subZ[d] = esmat_init (0,0);
    }
    // STEP ONE: separate esmat Y, Z, w to multiple small-sized esmat
    // NOTE: all esmat position has to be recomputed
    esmat_submat_row (w, subW, doc_lookup);
    esmat_submat_row (Y, subY, doc_lookup);
    esmat_submat_row (Z, subZ, doc_lookup);

    for (int d = 0; d < nDocs; d ++) {
        // STEP TWO: invoke group_lasso_solver to each individual group
        group_lasso_solver (subY[d], subZ[d], subW[d], RHO, lambda);

        // STEP TREE: merge solution of each individual group (place back)
        // NOTE: all esmat position has to be recomputed
        int start_row = (*doc_lookup)[d].first;
        int end_row = (*doc_lookup)[d].second;
        esmat_merge_row (subW[d], start_row, end_row, tempW);
    }
    // realign the mat->val with index-increasing order
    esmat_align (tempW);
    // STEP FIVE: free auxiliary resource
    for (int d = 0; d < nDocs; d ++) {
        esmat_free (subW[d]);
        esmat_free (subY[d]);
        esmat_free (subZ[d]);
    }

    // FINAL: update merged solution to w
    esmat_copy (tempW, w);
}
void coverage_subproblem (Esmat* Y, Esmat* Z, Esmat* w, double RHO, double lambda, Lookups* tables) {

    vector< pair<int,int> >* doc_lookup = tables->doc_lookup;
    vector<int>* word_lookup = tables->word_lookup; 
    vector< vector<int> >* voc_lookup = tables->voc_lookup;

    int nVocs = tables->nVocs;
    Esmat* tempW = esmat_init (w);
    vector<Esmat*> subW (nVocs); 
    vector<Esmat*> subY (nVocs);
    vector<Esmat*> subZ (nVocs);
    // STEP ZERO: initialize all submats
    for (int v = 0; v < nVocs; v++) {
        subW[v] = esmat_init (0,0);
        subY[v] = esmat_init (0,0);
        subZ[v] = esmat_init (0,0);
    }
    // STEP ONE: separate esmat Y, Z, w to multiple small-sized esmat
    // NOTE: all esmat position has to be recomputed
    esmat_submat_row (w, subW, word_lookup, voc_lookup);
    esmat_submat_row (Y, subY, word_lookup, voc_lookup);
    esmat_submat_row (Z, subZ, word_lookup, voc_lookup);

    for (int v = 0; v < nVocs; v++) {
        // STEP TWO: invoke group_lasso_solver to each individual group
        group_lasso_solver (subY[v], subZ[v], subW[v], RHO, lambda);

        // STEP TREE: merge solution of each individual group (place back)
        // NOTE: all esmat position has to be recomputed
        esmat_merge_row (subW[v], &((*voc_lookup)[v]), tempW);
    }
    // realign the mat->val with index-increasing order
    esmat_align (tempW);
    // STEP FIVE: free auxiliary resource
    for (int v = 0; v < nVocs; v ++) {
        esmat_free (subW[v]);
        esmat_free (subY[v]);
        esmat_free (subZ[v]);
    }

    // FINAL: update merged solution to w
    esmat_copy (tempW, w);
}

void HDP (vector<double> LAMBDAs, Esmat* W, Lookups* tables) {
    // SET MODEL-RELEVANT PARAMETERS 
    assert (LAMBDAs.size() == 3);
    double ALPHA = 1.0;
    double RHO = 1.0;
    int N = tables->nWords;

    /* DECLARE AND INITIALIZE INVOLVED VARIABLES AND MATRICES */
    Esmat* w_1 = esmat_init (N, 0);
    Esmat* w_2 = esmat_init (N, 0);
    Esmat* w_3 = esmat_init (N, 0);
    Esmat* w_4 = esmat_init (N, 0);

    Esmat* y_1 = esmat_init (N, 0);
    Esmat* y_2 = esmat_init (N, 0);
    Esmat* y_3 = esmat_init (N, 0);
    Esmat* y_4 = esmat_init (N, 0);

    Esmat* z = esmat_init (N, 0);

    Esmat* diff_1 = esmat_init (N, 0);
    Esmat* diff_2 = esmat_init (N, 0);
    Esmat* diff_3 = esmat_init (N, 0);
    Esmat* diff_4 = esmat_init (N, 0);

    /* SET ITERATION-RELEVANT VARIABLES */
    double error = INF;
    int iter = 0; 
    int max_iter = 2000;

    /* ITERATIVE OPTIMIZATION */
    while ( iter < max_iter ) { // STOPPING CRITERIA
        // STEP ZERO: RESET ALL SUBPROBLEM SOLUTIONS (OPTIONAL) 
        esmat_zeros (w_1);
        esmat_zeros (w_2);
        esmat_zeros (w_3);
        esmat_zeros (w_4);
        /*
#ifdef ITERATION_TRACE_DUMP
cout << "it is place 0 iteration #" << iter << ", going to get into frank_wolfe_solvere"  << endl;
#endif
*/
        // STEP ONE: RESOLVE W_1, W_2, W_3, W_4
        // resolve w_1
        frank_wolfe_solver (y_1, z, w_1, RHO, N); 
        double sub1_obj = subproblem_objective (1, y_1, z, w_1, RHO, 0.0, tables);
        /*
#ifdef ITERATION_TRACE_DUMP
cout << "frank_wolfe_solver done. norm2(w_1) = " << mat_norm2 (wone, N, N) << endl;
cout << "it is place 1 iteration #" << iter << ", going to get into group_lasso_solver"<< endl;
#endif
*/
        // resolve w_2
        global_topic_subproblem (y_2, z, w_2, RHO, LAMBDAs[0]);
        // compute value of objective function
        double sub2_obj = subproblem_objective (2, y_2, z, w_2, RHO, LAMBDAs[0],tables);
        /*
#ifdef ITERATION_TRACE_DUMP
cout << "it is place 3 iteration #" << iter << endl;
cout << "norm2(w_2) = " << mat_norm2 (wtwo, N, N) << endl;
#endif
*/
        // resolve w_3
        local_topic_subproblem (y_3, z, w_3, RHO, LAMBDAs[1], tables);
        double sub3_obj = subproblem_objective (3, y_3, z, w_3, RHO, LAMBDAs[1], tables);
        // resolve w_4
        coverage_subproblem (y_4, z, w_4, RHO, LAMBDAs[2], tables);
        double sub4_obj = subproblem_objective(4, y_4, z, w_4, RHO, LAMBDAs[2], tables);

        // STEP TWO: update z by averaging w_1, w_2, w_3 and w_4
        Esmat* temp = esmat_init ();
        esmat_add (w_1, w_2, z);
        esmat_copy (z, temp);
        esmat_add (temp, w_3, z);
        esmat_copy (z, temp);
        esmat_add (temp, w_4, z);
        esmat_scalar_mult (0.25, z);

        /*
#ifdef ITERATION_TRACE_DUMP
cout << "it is place 4 iteration #" << iter << endl;
cout << "norm2(z) = " << mat_norm2 (z, N, N) << endl;
#endif
*/
        // STEP THREE: update the y_1 and y_2 by w_1, w_2 and z
        esmat_sub (w_1, z, diff_1);
        // double trace_wone_minus_z = esmat_norm2 (diff_1); 
        esmat_scalar_mult (ALPHA, diff_1);
        esmat_copy (y_1, temp);
        esmat_add (temp, diff_1, y_1);

        esmat_sub (w_2, z, diff_2);
        //double trace_wtwo_minus_z = esmat_norm2 (diff_2); 
        esmat_scalar_mult (ALPHA, diff_2);
        esmat_copy (y_2, temp);
        esmat_add (temp, diff_2, y_2);

        esmat_sub (w_3, z, diff_3);
        //double trace_wthree_minus_z = esmat_norm2 (diff_3); 
        esmat_scalar_mult (ALPHA, diff_3);
        esmat_copy (y_3, temp);
        esmat_add (temp, diff_3, y_3);

        esmat_sub (w_4, z, diff_4);
        //double trace_wfour_minus_z = esmat_norm2 (diff_4); 
        esmat_scalar_mult (ALPHA, diff_4);
        esmat_copy (y_4, temp);
        esmat_add (temp, diff_4, y_4);

        // STEP FOUR: trace the objective function
        /*
        if (iter % 1 == 0) {
            // 1. trace the error
            error = original_objective (dist_mat, lambda, N, z);
            cout << "[Overall] iter = " << iter 
                 << ", Overall Error: " << error;
            /
#ifdef NTOPIC_DUMP
            // 2. get number of topic
            int nTopics = get_nTopics(z, N, N);
            cout << ", nTopics: " << nTopics;
#endif
            cout << endl;
        }
        */

        iter ++;
    }
    
    // STEP FIVE: memory recollection
    esmat_free (w_1);
    esmat_free (w_2);
    esmat_free (w_3);
    esmat_free (w_4);

    esmat_free (y_1);
    esmat_free (y_2);
    esmat_free (y_3);
    esmat_free (y_4);

    esmat_free (diff_1);
    esmat_free (diff_2);
    esmat_free (diff_3);
    esmat_free (diff_4);
    // STEP SIX: put converged solution to destinated W
    esmat_copy (z, W);
    esmat_free (z);
}

// entry main function
int main (int argc, char ** argv) {

    // EXCEPTION control: illustrate the usage if get input of wrong format
    if (argc < 5) {
        cerr << "Usage: HDP [voc_dataFile] [doc_dataFile] [lambda_global] [lambda_local] [lambda_coverage]" << endl;
        exit(-1);
    }

    // PARSE arguments
    string voc_file (argv[1]);
    string doc_file (argv[2]);
    vector<double> LAMBDAs (3, 0.0);
    LAMBDAs[0] = atof(argv[3]); // lambda_document
    LAMBDAs[1] = atof(argv[4]); // lambda_topic
    LAMBDAs[2] = atof(argv[5]); // lambda_block

    // preprocess the input dataset
    vector<string> voc_list;
    voc_list_read (voc_file, &voc_list);
    int nVocs = voc_list.size();

    vector< pair<int,int> > doc_lookup;
    vector<int> word_lookup;
    vector< vector<int> > voc_lookup (nVocs, vector<int>());
    Lookups lookup_tables;
    lookup_tables.doc_lookup = &doc_lookup;
    lookup_tables.word_lookup = &word_lookup;
    lookup_tables.voc_lookup = &voc_lookup;
    document_list_read (doc_file, &lookup_tables);
    
    lookup_tables.nDocs = lookup_tables.doc_lookup->size();
    lookup_tables.nWords = lookup_tables.word_lookup->size();
    lookup_tables.nVocs = nVocs;
    cerr << "nVocs = " << lookup_tables.nVocs << endl; // # vocabularies
    cerr << "nDocs = " << lookup_tables.nDocs << endl; // # documents
    cerr << "nWords = " << lookup_tables.nWords << endl; // # words
    cerr << "lambda_global = " << LAMBDAs[0] << endl;
    cerr << "lambda_local = " << LAMBDAs[1] << endl;
    cerr << "lambda_coverage = " << LAMBDAs[2] << endl;
    cerr << "TRIM_THRESHOLD = " << TRIM_THRESHOLD << endl;

    int seed = time(NULL);
    srand (seed);
    cerr << "seed = " << seed << endl;

    // restore matchness matrix in sparse representation
    /* here we consider non-noise version of topic model
    double ** match_mat = mat_init (N, N);
    mat_zeros (match_mat, N, N);
    */

    // Run sparse convex clustering
    Esmat* W = esmat_init (lookup_tables.nWords, 1);
    HDP (LAMBDAs, W, &lookup_tables);

    // Output results
    ofstream fout("result");

    // interpret result by means of voc_list
    /*{{{*/
    /*
    vector<int> centroids = get_all_centroids(W, N, N); // contains index of all centroids
    
    int nCentroids = centroids.size();
    for (int i = 0; i < N; i ++) {
        // output identification and its belonging
        fout << "id=" << i+1 << ", fea[0]=" << data[i]->fea[0].second << ", ";  // sample id
        for (int j = 0; j < N; j ++) {
            if( fabs(W[i][j]) > 3e-1 ) {
                fout << j+1 << "(" << W[i][j] << "),\t";
            }
        }
	fout << endl;

        // output distance of one sample to each centroid 
        fout << "dist_centroids: (";
        for (int j = 0; j < nCentroids - 1; j ++) {
            fout << dist_mat[i][ centroids[j] ] << ", ";
        }
        fout << dist_mat[i][ centroids[nCentroids-1] ] << ")";
        fout << endl;
    }
    */
/*}}}*/
}
