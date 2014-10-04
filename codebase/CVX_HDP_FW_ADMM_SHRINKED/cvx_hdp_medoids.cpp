/*############################################################### 
## MODULE: cvx_hdp_medoids.cpp
## VERSION: 1.0 
## SINCE 2014-06-14
## AUTHOR:
##     Jimmy Lin (xl5224) - JimmyLin@utexas.edu  
## DESCRIPTION: 
##
################################################################# 
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/

#include "cvx_hdp_medoids.h"


/* algorithmic options */ 
#define EXACT_LINE_SEARCH  // comment this to use inexact search

/* dumping options */
// #define FRANK_WOLFE_DUMP
// #define EXACT_LINE_SEARCH_DUMP
// #define BLOCKWISE_DUMP

const double SPARSITY_TOL = 1e-4;
const double FRANK_WOLFE_TOL = 1e-20;
const double ADMM_EPS = 1e-2;
typedef double (* dist_func) (Instance*, Instance*, int); 
const double r = 1000000.0;

class Compare
{
    public:
        bool operator() (pair<int, double> obj1, pair<int, double> obj2)
        {
            return obj1.second > obj2.second;
        }
};
double first_subproblm_obj (double** dist_mat, double** yone, double** zone, double** wone, double rho, int N) {
    double ** temp = mat_init (N, N);
    double ** diffone = mat_init (N, N);
    mat_zeros (diffone, N, N);
    // sum1 = 0.5 * sum_n sum_k (w_nk * d^2_nk) -> loss
    mat_zeros (temp, N, N);
    mat_times (wone, dist_mat, temp, N, N);
    double sum1 = 0.5 * mat_sum (temp, N, N);
    // sum2 = y_1^T dot (w_1 - z) -> linear
    mat_zeros (temp, N, N);
    mat_sub (wone, zone, diffone, N, N); // temp = w_1 - z_1
    mat_tdot (yone, diffone, temp, N, N);
    double sum2 = mat_sum (temp, N, N);
    // sum3 = 0.5 * rho * || w_1 - z_1 ||^2 -> quadratic
    mat_zeros (temp, N, N);
    mat_sub (wone, zone, temp, N, N);
    double sum3 = 0.5 * rho * mat_norm2 (temp, N, N);
    // sum4 = r dot (1 - sum_k w_nk) -> dummy
    double * temp_vec = new double [N];
    mat_sum_row (wone, temp_vec, N, N);
    double dummy_penalty = 0.0;
    for (int i = 0; i < N; i ++) {
        dummy_penalty += r*(1 - temp_vec[i]);
    }
    double total = sum1+sum2+sum3+dummy_penalty;
    cout << "[Frank_wolfe] (loss, linear, quadratic, dummy, total) = (" 
        << sum1 << ", " << sum2 << ", " << sum3 << ", " << dummy_penalty << ", " << total
        <<  ")" << endl;
    mat_free (temp, N, N);
    mat_free (diffone, N, N);
    delete [] temp_vec;

    return total;
}
void frank_wolfe_solver (double ** dist_mat, double ** y, double ** z, double ** w, double rho, int R, int C, int FW_MAX_ITER, set<int>& col_active_set) {
    // cout << "within frank_wolfe_solver" << endl;
    // STEP ONE: compute gradient mat initially
    vector< set< pair<int, double> > > actives (R, set<pair<int,double> >());
    vector< priority_queue< pair<int,double>, vector< pair<int,double> >, Compare> > pqueues (R, priority_queue< pair<int,double>, vector< pair<int,double> >, Compare> ());
    for (int i = 0; i < R; i++) {
        for (set<int>::iterator it=col_active_set.begin();it != col_active_set.end(); ++it) {
            int j = *it;
            double grad=0.5*dist_mat[i][j]+y[i][j]+rho*(w[i][j]-z[i][j]); 
            if (w[i][j] > 1e-10) 
                actives[i].insert(make_pair(j,grad));
            else 
                pqueues[i].push(make_pair(j, grad));
        }
    }
    // STEP TWO: iteration solve each row 
    int k = 0;  // iteration number
    vector<bool> is_fw_opt_reached (R, false);
    set<pair<int,double> >::iterator it;
    // cout << "within frank_wolfe_solver: start iteration" << endl;
    while (k < FW_MAX_ITER) { // TODO: change to use portional criteria
        // compute new active atom: can be in active set or not
        vector< pair<int, double> > s (R, pair<int,double>());
        vector<bool> isInActives (R, false);
        for (int i = 0; i < R; i++) {
            if (is_fw_opt_reached[i]) continue;
            if (pqueues[i].size() <= 0) continue;
            pair<int,double> tmp_pair = pqueues[i].top();
            s[i].first = tmp_pair.first;
            s[i].second = tmp_pair.second;
            // cout << s[i].first << ":" << s[i].second << endl;
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                // take the minimal of each row
                if (it->second < s[i].second) {
                    isInActives[i] = true;
                    s[i].first = it->first;
                    s[i].second = it->second;
                }
            }
            // compute gamma: inexact or exact
            double gamma; // step size of line search
#ifdef EXACT_LINE_SEARCH
            double sum1=0.0, sum2=0.0, sum3=0.0, sum4=0.0;
            // gamma* = (sum1 + sum2 + sum3) / sum4, where
            // sum1 = 1/2 sum_n sum_k (w - s)_nk * || x_n - mu_k ||^2
            // sum2 = sum_n sum_k y_nk (w - s)_nk
            // sum3 = - rho * sum_n sum_k  (w - z) (w-s)
            // sum4 = sum_n sum_k rho * (s - w)^2
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                double w_minus_s;
                double w_minus_z = w[i][it->first] - z[i][it->first];
                if (it->first == s[i].first) {
                    w_minus_s = w[i][it->first]-1.0;
                } else {
                    w_minus_s = w[i][it->first];
                }
                sum1 += 0.5 * w_minus_s * (dist_mat[i][it->first] -r);
                sum2 += y[i][it->first] * w_minus_s;
                sum3 += rho * w_minus_s * w_minus_z;
                sum4 += rho * w_minus_s * w_minus_s; 
            }
            if (!isInActives[i]) {
                sum1 += 0.5 * (-1.0) * (dist_mat[i][s[i].first] - r);
                sum2 += y[i][it->first] * (-1.0);
                sum3 += rho * (-1.0) * (w[i][s[i].first]-z[i][s[i].first]);
                sum4 += rho;
            }

            if (fabs(sum4) > 0) {
                gamma = (sum1 + sum2 + sum3) / sum4;
#ifdef EXACT_LINE_SEARCH_DUMP
                cout << "[exact] i=" << i ;
                cout << ",k=" << k;
                cout << ",sum1="<< sum1;
                cout << ",sum2="<< sum2;
                cout << ",sum3="<< sum3;
                cout << ",sum4="<< sum4;
                cout << ",gamma="<< gamma;
                cout << endl;
#endif
                gamma = max(gamma, 0.0);
                gamma = min(gamma, 1.0);
            } else {
                gamma = 0.0;
                is_fw_opt_reached[i] = true;
            }
#else
            gamma = 2.0 / (k+2.0);
#endif
            // update w
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                w[i][it->first] *= (1-gamma);
            }
            w[i][s[i].first] += gamma;
            // update new actives 
            set<pair<int, double> > temp;
            if (!isInActives[i]) {
                actives[i].insert(pqueues[i].top());
                pqueues[i].pop();
            }
            double new_grad;
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                int j = it->first;
                new_grad=0.5*dist_mat[i][j]+y[i][j]+rho*(w[i][j]-z[i][j]); 
                temp.insert (make_pair(it->first, new_grad));
            }
            actives[i].swap(temp);
            // cout << "actives[" << i << "]: " << actives[i].size() << endl;
        }
        // cout << "within frank_wolfe_solver: next iteration" << endl;
        k ++;
    }
#ifdef FRANK_WOLFE_DUMP
     double penalty = first_subproblm_obj (dist_mat, y, z, w, rho, R);
     cout << "[Frank-Wolfe] iteration: " << k << ", first_subpro_obj: " << penalty << endl;
#endif
}

double second_subproblem_obj (double ** ytwo, double ** z, double ** wtwo, double rho, int N, double* lambda) {

    double ** temp = mat_init (N, N);
    double ** difftwo = mat_init (N, N);
    mat_zeros (difftwo, N, N);

    // reg = 0.5 * sum_k max_n | w_nk |  -> group-lasso
    mat_zeros (temp, N, N);
    double * maxn = new double [N]; 
    for (int i = 0; i < N; i ++) { // Ian: need initial 
        maxn[i] = -INF;
    }

    for (int i = 0; i < N; i ++) {
        for (int j = 0; j < N; j ++) {
            if (wtwo[i][j] > maxn[j])
                maxn[j] = wtwo[i][j];
        }
    }
    double sumk = 0.0;
    for (int i = 0; i < N; i ++) {
        sumk += lambda[i]*maxn[i];
    }
    double group_lasso = sumk; 

    // sum2 = y_2^T dot (w_2 - z) -> linear
    mat_zeros (temp, N, N);
    mat_sub (wtwo, z, difftwo, N, N);
    mat_tdot (ytwo, difftwo, temp, N, N);
    double sum2 = mat_sum (temp, N, N);

    // sum3 = 0.5 * rho * || w_2 - z_2 ||^2 -> quadratic mat_zeros (temp, N, N);
    mat_sub (wtwo, z, temp, N, N);
    double sum3 = 0.5 * rho * mat_norm2 (temp, N, N);

    mat_free (temp, N, N);
    // ouput values of each components
    cout << "[Blockwise] (group_lasso, linear, quadratic) = ("
        << group_lasso << ", " << sum2 << ", " << sum3
        << ")" << endl;
    return group_lasso + sum2 + sum3;
}

void skyline (double** wout, double**wbar, int R, int C, double lambda, set<int> col_active_sets) {
    vector< vector< double > > alpha_vec (C, vector<double>());
    vector< int > num_alpha_elem (C, 0);
    for (int i = 0; i < R; i ++) {
        set<int>::iterator it;
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            if (wbar[i][j] > SPARSITY_TOL) {
                alpha_vec[j].push_back (abs(wbar[i][j]));
                ++ num_alpha_elem[j];
            }
        }
    }
    vector<double> max_term (C, -INF);
    vector<double> separator (C);
    for (int j = 0; j < C; j ++) {
        // 2. sorting
        std::sort (alpha_vec[j].begin(), alpha_vec[j].end(), double_dec_comp);
        // 3. find mstar
        int mstar = 0; // number of elements support the sky
        double new_term, sum_alpha = 0.0;
        for (int i = 0; i < num_alpha_elem[j]; i ++) {
            sum_alpha += alpha_vec[j][i];
            new_term = (sum_alpha - lambda) / (i + 1.0);
            if ( new_term > max_term[j] ) {
                separator[j] = alpha_vec[j][i];
                max_term[j] = new_term;
                mstar = i;
            }
        }
        if (max_term[j] < 0) 
            max_term[j] = (sum_alpha - lambda) / R;
    }
    for (int i = 0; i < R; i ++) {
        // 4. assign closed-form solution to wout
        for (int j = 0; j < C; j ++) {
            if( max_term[j] < 0 ) {
                wout[i][j] = 0.0;
                continue;
            }
            double wbar_val = wbar[i][j];
            if ( abs(wbar_val) >= separator[j] ) 
                wout[i][j] = max_term[j];
            else 
                // its ranking is above m*, directly inherit the wbar
                wout[i][j] = max(wbar_val,0.0);
        }
    }
}
void group_lasso_solver (double ** y, double ** z, double ** w, double rho, vector<double>& lambda, int R, int C, set<int> col_active_sets) {
    int global_lambda = lambda[0];
    int local_lambda = lambda[1];

    double** wbar = mat_init (R, C);
    for (int i = 0; i < R; i ++) {
        for (int j = 0; j < C; j ++) {
            wbar[i][j] = (rho * z[i][j] - y[i][j]) / rho;
        }
    }
    // TODO: extend the group lasso solver to both local and global
    skyline (w, wbar, R, C, lambda[0], col_active_sets);

#ifdef BLOCKWISE_DUMP
    double penalty = second_subproblem_obj (y, z, w, rho, N, lambda);
    cout << "[Blockwise] second_subproblem_obj: " << penalty << endl;
#endif

    mat_free (wbar, R, C);
}

double overall_objective (double ** dist_mat, vector<double>& lambda, int R, int C, double ** z) {
    // STEP ONE: compute 
    //     loss = sum_i sum_j z[i][j] * dist_mat[i][j]
    double normSum = 0.0;
    for (int i = 0; i < R; i ++) 
        for (int j = 0; j < C; j ++) 
            normSum += z[i][j] * dist_mat[i][j];
    double loss = 0.5 * normSum;
    cout << "loss=" << loss;
    // STEP TWO: compute dummy loss
    // sum4 = r dot (1 - sum_k w_nk) -> dummy
    double * temp_vec = new double [R];
    mat_sum_row (z, temp_vec, R, C);
    double dummy_penalty=0.0;
    for (int i = 0; i < R; i ++) 
        dummy_penalty += r * max(1 - temp_vec[i], 0.0) ;
    delete[] temp_vec;
    cout << ", dummy= " << dummy_penalty;
    // STEP THREE: compute group-lasso regularization
    double * maxn = new double [C]; 
    for (int j = 0; j < C; j ++) 
        maxn[j] = -INF;
    for (int i = 0; i < R; i ++) 
        for (int j = 0; j < C; j ++) 
            if ( fabs(z[i][j]) > maxn[j])
                maxn[j] = fabs(z[i][j]);
    delete[] maxn;
    double lasso = 0.0;
    for (int j = 0; j < C; j ++) 
        lasso += lambda[0]*maxn[j];
    cout << ", g_lasso=" << lasso;
    double overall = loss + lasso + dummy_penalty;
    cout << ", overall=" <<  overall << endl;
    return loss + lasso;
}

/* Compute the mutual distance of input instances contained within "data" */
void compute_dist_mat (vector<Instance*>& data, double ** dist_mat, int R, int C, dist_func df, bool isSym) {
    for (int i = 0; i < R; i ++) {
        for (int j = 0; j < C; j ++) {
            ;
            // dist_mat[i][j] = df (xi, muj, D);
        }
    }
}

void cvx_hdp_medoids (double ** dist_mat, int fw_max_iter, vector<double>& lambda, double ** W, int ADMM_max_iter, int SS_PERIOD, Lookups * tables) {
    int D = tables->nDocs;
    int N = tables->nWords;
    vector< pair<int,int> >* word_lookup = tables->word_lookup;
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    // parameters 
    double alpha = 0.2;
    double rho = 1;
    ofstream ss_out ("plot_cputime_objective");
    ss_out << "Time Objective" << endl;
    clock_t cputime = 0;
    clock_t prev = clock();
    // iterative optimization 
    double error = INF;
    double ** wone = mat_init (N, D);
    double ** wtwo = mat_init (N, D);
    double ** yone = mat_init (N, D);
    double ** ytwo = mat_init (N, D);
    double ** z = mat_init (N, D);
    double ** z_old = mat_init (N, D);
    double ** diffzero = mat_init (N, D);
    double ** diffone = mat_init (N, D);
    double ** difftwo = mat_init (N, D);
    mat_zeros (wone, N, D);
    mat_zeros (wtwo, N, D);
    mat_zeros (yone, N, D);
    mat_zeros (ytwo, N, D);
    mat_zeros (z, N, D);
    mat_zeros (z_old, N, D);
    mat_zeros (diffzero, N, D);
    mat_zeros (diffone, N, D);
    mat_zeros (difftwo, N, D);

    // variables for shriking method
    set<int> col_active_sets;
    // set initial active_set as all elements
    for (int i = 0; i < N; i++) {
        col_active_sets.insert(i);
    }

    cputime += clock() - prev;
    ss_out << cputime << " " << 0 << endl;
    prev = clock();
    int iter = 0; // Ian: usually we count up (instead of count down)
    bool no_active_element = false, admm_opt_reached = false;
    while ( iter < ADMM_max_iter ) { // TODO: stopping criteria

        // STEP ONE: resolve w_1 and w_2
        frank_wolfe_solver (dist_mat, yone, z, wone, rho, N, D, fw_max_iter, col_active_sets);
        group_lasso_solver (ytwo, z, wtwo, rho, lambda, N, D, col_active_sets);

        // STEP TWO: update z by averaging w_1 and w_2
        // STEP THREE: update the y_1 and y_2 by w_1, w_2 and z
        set<int>::iterator it;
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            for (int i = 0; i < N; i ++) {
                z[i][j] = (wone[i][j] + wtwo[i][j]) / 2.0;
                yone[i][j] += alpha * (wone[i][j] - z[i][j]) ;
                ytwo[i][j] += alpha * (wtwo[i][j] - z[i][j]) ;
            }
        }

        // STEP FOUR: trace the objective function
        if (iter < 3 * SS_PERIOD || (iter+1) % SS_PERIOD == 0) {
            cputime += clock() - prev;
            error = overall_objective (dist_mat, lambda, N, D, z);
            cout << "[Overall] iter = " << iter 
                << ", Loss Error: " << error << endl;
            ss_out << cputime << " " << error << endl;
            prev = clock();
        }
        // Shrinking Method:
        // STEP ONE: reduce number of elements considered in next iteration
        vector<int> col_to_shrink;
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            bool is_primal_shrink=false, is_dual_shrink=false;
            int i;
            for (i = 0; i < N; i++) {
                // (A) primal shrinking:
                if ( z[i][j] > ADMM_EPS || z_old[i][j] > ADMM_EPS ) break;
                // (B) dual shrinking:
                if ( wone[i][j] > ADMM_EPS || wtwo[i][j] > ADMM_EPS ) break;
            }
            // cache index of element to be removed
            if ( i == N ) 
                col_to_shrink.push_back(j);
        }
        
        // remove shrinked row/column from row/column active sets
        int num_col_to_shrink = col_to_shrink.size();
        for (int s = 0; s < num_col_to_shrink; s ++) {
            int j_shr = col_to_shrink[s];
            col_active_sets.erase(j_shr);
            for(int i=0;i<N;i++){
                wone[i][j_shr] = 0.0;
                z[i][j_shr] = 0.0;
                z_old[i][j_shr] = 0.0;
            }
        }
        // update z_old
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            for (int i = 0; i < N; i++) {
                z_old[i][j] = z[i][j];
            }
        }
        // count number of active elements
        int num_active_cols = col_active_sets.size();
        if ((iter+1) % SS_PERIOD == 0) {
            ;
            /*
            cout << "iter: " << iter;
            cout << ", num_active_cols: " << num_active_cols <<endl;
            */
        }
        int num_active_elements=N*num_active_cols;
        // STEP TWO: consider to open all elements to check optimality
        /*
        if (num_active_elements == 0 && !no_active_element) {
            no_active_element = true;
            // open all elements to verify the result
            cout << "open all elements for optimality checking!" << endl;
            col_active_sets.clear();
            for (int i = 0; i < N; i++) {
                col_active_sets.insert(i);
            }
        } else if (num_active_elements == 0 && no_active_element) 
            admm_opt_reached = true;
        else if (num_active_elements > 0 && no_active_element) {
            no_active_element = false;
            cout << "fail to reach global ADMM optima!" << endl;
        }
        */
        iter ++;
    }

    // STEP FIVE: memory recollection
    mat_free (wone, N, D);
    mat_free (wtwo, N, D);
    mat_free (yone, N, D);
    mat_free (ytwo, N, D);
    mat_free (diffone, N, D);
    mat_free (difftwo, N, D);
    mat_free (z_old, N, D);
    // STEP SIX: put converged solution to destination W
    mat_copy (z, W, N, D);
    mat_free (z, N, D);
    ss_out.close();
}

// entry main function
int main (int argc, char ** argv) {

    if (argc < 7) {
        cerr << "Usage: " << endl;
        cerr << "\tcvx_hdp_medoids [voc_dataFile] [doc_dataFile] [lambda_global] [lambda_local] [FW_MAX_ITER] [ADMM_MAX_ITER]" << endl;
        exit(-1);
    }

    // PARSE arguments
    string voc_file (argv[1]);
    string doc_file (argv[2]);
    vector<double> LAMBDAs (2, 0.0);
    LAMBDAs[0] = atof(argv[3]); // lambda_global
    LAMBDAs[1] = atof(argv[4]); // lambda_local
    int FW_MAX_ITER = atoi(argv[5]);
    int ADMM_MAX_ITER = atoi(argv[6]);
    int SS_PERIOD = 100;

    // preprocess the input dataset
    vector<string> voc_list;
    voc_list_read (voc_file, &voc_list);
    cerr << "vocs read done! " << endl;
    int nVocs = voc_list.size();

    // init lookup_tables
    vector< pair<int,int> > doc_lookup;
    vector< pair<int,int> > word_lookup;
    vector< vector<int> > voc_lookup (nVocs, vector<int>());
    Lookups lookup_tables;
    lookup_tables.doc_lookup = &doc_lookup;
    lookup_tables.word_lookup = &word_lookup;
    lookup_tables.voc_lookup = &voc_lookup;
    document_list_read (doc_file, &lookup_tables);
    cerr << "docs read done" << endl;

    lookup_tables.nDocs = lookup_tables.doc_lookup->size();
    lookup_tables.nWords = lookup_tables.word_lookup->size();
    lookup_tables.nVocs = nVocs;
    int seed = time(NULL);
    srand (seed);
    cerr << "###########################################" << endl;
    cerr << "nVocs = " << lookup_tables.nVocs << endl; // # vocabularies
    cerr << "nDocs = " << lookup_tables.nDocs << endl; // # documents
    cerr << "nWords = " << lookup_tables.nWords << endl; // # words
    cerr << "lambda_global = " << LAMBDAs[0] << endl;
    cerr << "lambda_local = " << LAMBDAs[1] << endl;
    cerr << "TRIM_THRESHOLD = " << TRIM_THRESHOLD << endl;
    cerr << "seed = " << seed << endl;
    cerr << "###########################################" << endl;

    // Run sparse convex clustering
    int N = lookup_tables.nWords;
    int D = lookup_tables.nDocs;
    double** W = mat_init (N, D);
    double** dist_mat = mat_init (N, D);
    compute_dist_mat (dist_mat, &lookup_tables, N, D);

    ofstream dmat_out ("dist_mat");
    dmat_out << mat_toString (dist_mat, N, D);
    cerr << "dist_mat output finished.." << endl;
    cvx_hdp_medoids (dist_mat, FW_MAX_ITER, LAMBDAs, W, ADMM_MAX_ITER, SS_PERIOD, &Lookups);

    /* Output objective */
    output_objective (clustering_objective (dist_mat, W, N, D));
    /* Output cluster centroids */
    output_model (W, N, D);
    /* Output assignment */
    output_assignment (W, &lookup_tables);

    /* reallocation */
    mat_free (W, N, D);
    mat_free (dist_mat, N, D);
}
