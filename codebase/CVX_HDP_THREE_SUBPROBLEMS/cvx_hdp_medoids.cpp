#include "cvx_hdp_medoids.h"

/* algorithmic options */ 
#define EXACT_LINE_SEARCH  // comment this to use inexact search

/* dumping options */
// #define EXACT_LINE_SEARCH_DUMP

const double SPARSITY_TOL = 1e-4; //
const double FRANK_WOLFE_TOL = 1e-20; //
const double ADMM_EPS = 1e-2;  //
const double r = 1000000.0; // dummy penality rate
const double EPS = 1e-5;

/* Compute the mutual distance of input instances contained within "data" */
void compute_dist_mat (double** dist_mat, Lookups* tables, int R, int C) {
    int N = tables->nWords;
    int D = tables->nDocs;
    assert (R == N);
    assert (C == D);
    // STEP ZERO: parse input
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    vector< pair<int,int> > word_lookup = *(tables->word_lookup); 

    // STEP ONE: compute distribution for each document
    vector< map<int, double> > distributions (D, map<int, double>());
    for (int d = 0; d < D; d ++) {
        // a. compute sum of word frequency
        int sumFreq = 0;
        for (int w = doc_lookup[d].first; w < doc_lookup[d].second; w++) 
            sumFreq += word_lookup[w].second;
        // b. compute distribution
        for (int w = doc_lookup[d].first; w < doc_lookup[d].second; w++) {
            int voc_index = word_lookup[w].first;
            double prob = 1.0 * word_lookup[w].second / sumFreq;
            distributions[d].insert(pair<int, double> (voc_index, prob));
        }
    }
    // STEP TWO: compute weight of word within one document
    mat_zeros(dist_mat, R, C);
    for (int d = 0; d < D; d ++) {
        for (int w = doc_lookup[d].first; w < doc_lookup[d].second; w++) {
            for (int j = 0; j < C; j ++) {
                int voc_index = word_lookup[w].first;
                int count_w_d1 = word_lookup[w].second;    
                double prob_w_d2;
                double dist;
                map<int, double>::const_iterator iter;
                iter = distributions[j].find(voc_index);
                if (iter == distributions[j].end()) {
                    prob_w_d2 = 0.0;
                    dist = INF;
                } else {
                    prob_w_d2 = iter->second;
                    //   dist(w, d2) =  - count_w_d1 * log( prob_d2(w) )
                    dist = - count_w_d1 * log(prob_w_d2);
                }
                int esmat_index = w + N * j;
                dist_mat[w][j] = dist + noise(0.0, 0.01);
            }
        }
    }
}

void frank_wolfe_solver (double ** dist_mat, double ** y, double ** z, double ** w, double rho, int R, int C, int FW_MAX_ITER, set<int>& col_active_set) {
    // cout << "within frank_wolfe_solver" << endl;
    // STEP ONE: compute gradient mat initially
    vector< set< pair<int, double> > > actives (R, set<pair<int,double> >());
    vector< priority_queue< pair<int,double>, vector< pair<int,double> >, Int_Double_Pair_Dec> > pqueues (R, priority_queue< pair<int,double>, vector< pair<int,double> >, Int_Double_Pair_Dec> ());
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
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) 
                w[i][it->first] *= (1-gamma);
            w[i][s[i].first] += gamma;
            // update new actives 
            set< pair<int, double> > temp;
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
        }
        k ++;
    }
}
void skyline (double** wout, double**wbar, int R_start, int R_end, int C, double lambda, set<int> col_active_sets) {
    vector< vector< double > > alpha_vec (C, vector<double>());
    vector< int > num_alpha_elem (C, 0);
    for (int i = R_start; i < R_end; i ++) {
        set<int>::iterator it;
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            if (wbar[i][j] > SPARSITY_TOL) {
                alpha_vec[j].push_back (abs(wbar[i][j]));
                ++ num_alpha_elem[j];
            }
        }
    }
    vector<double> max_term (C, -1e50);
    vector<double> separator (C, 0.0);
    int R = R_end - R_start;
    for (int j = 0; j < C; j ++) {
        if (num_alpha_elem[j] == 0) continue;
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
    for (int i = R_start; i < R_end; i ++) {
        // 4. assign closed-form solution to wout
        set<int>::iterator it;
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            if ( max_term[j] < 0 ) {
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
void global_group_lasso_solver (double** y, double** z, double** w, double rho, vector<double>& lambda, Lookups *tables, set<int> col_active_sets) {
    int R = tables->nWords;
    int C = tables->nDocs;
    int global_lambda = lambda[0];
    int local_lambda = lambda[1];
    double** wbar = mat_init (R, C); mat_zeros (wbar, R, C);
    for (int i = 0; i < R; i ++) 
        for (int j = 0; j < C; j ++) 
            wbar[i][j] = (rho * z[i][j] - y[i][j]) / rho;
    skyline (w, wbar, 0, R, C, global_lambda, col_active_sets);
    mat_free (wbar, R, C);
}
void local_group_lasso_solver (double** y, double** z, double** w, double rho, vector<double>& lambda, Lookups *tables, set<int> col_active_sets) {
    int R = tables->nWords;
    int C = tables->nDocs;
    vector< pair<int,int> > word_lookup = *(tables->word_lookup);
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    int global_lambda = lambda[0];
    int local_lambda = lambda[1];

    double** wbar = mat_init (R, C); mat_zeros (wbar, R, C);
    for (int i = 0; i < R; i ++) 
        for (int j = 0; j < C; j ++) 
            wbar[i][j] = (rho * z[i][j] - y[i][j]) / rho;
    // extend the group lasso solver to both local and global
    for (int d = 0; d < tables->nDocs; d++) {
        int R_start = doc_lookup[d].first;
        int R_end = doc_lookup[d].second;
        skyline (w, wbar, R_start, R_end, C, local_lambda, col_active_sets);
    }
    mat_free (wbar, R, C);
}
double lasso_objective (double** z, double lambda, int R_start, int R_end, int C) {
    double lasso = 0.0;
    vector<double> maxn(C, -INF); 
    for (int i = R_start; i < R_end; i ++)
        for (int j = 0; j < C; j ++)
            if ( fabs(z[i][j]) > maxn[j])
                maxn[j] = fabs(z[i][j]);
    for (int j = 0; j < C; j ++) 
        lasso += lambda * maxn[j];
    return lasso;
}
double overall_objective (double ** dist_mat, vector<double>& lambda, int R, int C, double ** z, Lookups* tables) {
    int D = tables->nDocs;
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    // STEP ONE: compute 
    //     loss = sum_i sum_j z[i][j] * dist_mat[i][j]
    double normSum = 0.0;
    for (int i = 0; i < R; i ++) 
        for (int j = 0; j < C; j ++) 
            normSum += z[i][j] * dist_mat[i][j];
    double loss = 0.5 * normSum;
    // STEP TWO: compute dummy loss
    // sum4 = r dot (1 - sum_k w_nk) -> dummy
    double * temp_vec = new double [R];
    mat_sum_row (z, temp_vec, R, C);
    double dummy_penalty = 0.0;
    for (int i = 0; i < R; i ++) 
        dummy_penalty += r * max(1 - temp_vec[i], 0.0);
    delete[] temp_vec;
    // STEP THREE: compute group-lasso regularization
    double global_lasso = lasso_objective(z, lambda[0], 0, R, C);
    double sum_local_lasso = 0.0;
    vector<double> local_lasso (D, 0.0);
    for (int d = 0; d < D; d++) {
        local_lasso[d] = lasso_objective(z, lambda[1], doc_lookup[d].first, doc_lookup[d].second, C); 
        sum_local_lasso += local_lasso[d];
    }
    double overall = loss + global_lasso + sum_local_lasso + dummy_penalty;
    cout << "loss: " << loss << ", dummy=" << dummy_penalty
        << ", global_lasso=" << global_lasso 
        << ", sum_local_lasso=" << sum_local_lasso 
        << ", overall=" << overall
        << endl;
    return loss + global_lasso + sum_local_lasso;
}


void cvx_hdp_medoids (double ** dist_mat, int fw_max_iter, vector<double>& lambda, double ** W, int ADMM_max_iter, int SS_PERIOD, Lookups * tables) {
    int N = tables->nWords;
    int D = tables->nDocs;
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
    double ** wthree = mat_init (N, D);
    double ** yone = mat_init (N, D);
    double ** ytwo = mat_init (N, D);
    double ** ythree = mat_init (N, D);
    double ** z = mat_init (N, D);
    double ** z_old = mat_init (N, D);
    mat_zeros (wone, N, D);
    mat_zeros (wtwo, N, D);
    mat_zeros (wthree, N, D);
    mat_zeros (yone, N, D);
    mat_zeros (ytwo, N, D);
    mat_zeros (ythree, N, D);
    mat_zeros (z, N, D);
    mat_zeros (z_old, N, D);

    // variables for shriking method
    // set initial active_set as all elements
    set<int> col_active_sets;
    for (int i = 0; i < D; i++) 
        col_active_sets.insert(i);

    cputime += clock() - prev;
    ss_out << cputime << " " << 0 << endl;
    prev = clock();
    int iter = 0; // Ian: usually we count up (instead of count down)
    bool no_active_element = false, admm_opt_reached = false;
    while ( iter < ADMM_max_iter ) { // TODO: stopping criteria
        // STEP ONE: resolve w_1 and w_2
        frank_wolfe_solver (dist_mat, yone, z, wone, rho, N, D, fw_max_iter, col_active_sets);
        global_group_lasso_solver (ytwo, z, wtwo, rho, lambda, tables, col_active_sets);
        local_group_lasso_solver (ythree, z, wthree, rho, lambda, tables, col_active_sets);
        // STEP TWO: update z by averaging w_1 and w_2
        // STEP THREE: update the y_1 and y_2 by w_1, w_2 and z
        set<int>::iterator it;
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            for (int i = 0; i < N; i ++) {
                z[i][j] = (wone[i][j] + wtwo[i][j] + wthree[i][j]) / 3.0;
                yone[i][j] += alpha * (wone[i][j] - z[i][j]) ;
                ytwo[i][j] += alpha * (wtwo[i][j] - z[i][j]) ;
                ythree[i][j] += alpha * (wthree[i][j] - z[i][j]) ;
            }
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
                if ( wone[i][j] > ADMM_EPS || wtwo[i][j] > ADMM_EPS || wthree[i][j] > ADMM_EPS) break;
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
            for(int i=0;i<N;i++) {
                wone[i][j_shr] = 0.0;
                wtwo[i][j_shr] = 0.0;
                wthree[i][j_shr] = 0.0;
                z[i][j_shr] = 0.0;
                z_old[i][j_shr] = 0.0;
            }
        }
        // update z_old
        for (it=col_active_sets.begin();it!=col_active_sets.end();++it) {
            int j = *it;
            for (int i = 0; i < N; i++) 
                z_old[i][j] = z[i][j];
        }
        // count number of active elements
        int num_active_cols = col_active_sets.size();
        // STEP FOUR: trace the objective function
        if (iter < 3 * SS_PERIOD || (iter+1) % SS_PERIOD == 0) {
            cputime += clock() - prev;
            error = overall_objective (dist_mat, lambda, N, D, z, tables);
            cout << "[Overall] iter = " << iter 
                << ", num_active_cols: " << num_active_cols
                << ", Loss Error: " << error << endl;
            ss_out << cputime << " " << error << endl;
            prev = clock();
        }
        int num_active_elements = N * num_active_cols;
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
    vector< pair<int,int> > word_in_doc;
    Lookups lookup_tables;
    lookup_tables.doc_lookup = &doc_lookup;
    lookup_tables.word_lookup = &word_lookup;
    lookup_tables.voc_lookup = &voc_lookup;
    lookup_tables.word_in_doc = &word_in_doc;
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

    // dist_mat computation and output
    double** dist_mat = mat_init (N, D);
    compute_dist_mat (dist_mat, &lookup_tables, N, D);
    ofstream dmat_out ("dist_mat");
    dmat_out << mat_toString (dist_mat, N, D);
    dmat_out.close();
    cerr << "dist_mat output finished.." << endl;

    // word2doc output with frequency
    ofstream w2dvec_out ("word2doc");
    for (int i = 0; i < N; i ++)
        w2dvec_out << word_in_doc[i].first << " "
           << word_in_doc[i].second << endl;
    w2dvec_out.close();
    cerr << "w2dvec output finished.." << endl;

    cvx_hdp_medoids (dist_mat, FW_MAX_ITER, LAMBDAs, W, ADMM_MAX_ITER, SS_PERIOD, &lookup_tables);

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
