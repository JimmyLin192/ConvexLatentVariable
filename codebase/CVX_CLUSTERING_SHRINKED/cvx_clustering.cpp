/*############################################################### 
## MODULE: cvx_clustering.cpp
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

#include "cvx_clustering.h"
#include <cassert>
#include <queue>

#include "../util.h"

/* algorithmic options */ 
#define EXACT_LINE_SEARCH  // comment this to use inexact search

/* dumping options */
// #define FRANK_WOLFE_DUMP
#define EXACT_LINE_SEARCH_DUMP
// #define BLOCKWISE_DUMP
// #define NCENTROID_DUMP
// #define SPARSE_CLUSTERING_DUMP

const double FRANK_WOLFE_TOL = 1e-20;
typedef double (* dist_func) (Instance*, Instance*, int); 
const double r = 10000.0;
const double EPS = 0;

bool pairComparator (const std::pair<int, double>& firstElem, const std::pair<int, double>& secondElem) {
    // sort pairs by second element with decreasing order
    return firstElem.second > secondElem.second;
}

double first_subproblm_obj (double ** dist_mat, double ** yone, double ** zone, double ** wone, double rho, int N) {
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
#ifdef FRANK_WOLFE_DUMP
    cout << "[Frank_wolfe] (loss, linear, quadratic, dummy, total) = (" 
         << sum1 << ", " << sum2 << ", " << sum3 << ", " << dummy_penalty << ", " << total
         <<  ")" << endl;
#endif

    mat_free (temp, N, N);
    mat_free (diffone, N, N);
    delete [] temp_vec;

    return total;
}
class Compare
{
    public:
        bool operator() (pair<int, double> obj1, pair<int, double> obj2)
        {
            return pairComparator(obj1, obj2);
        }
};

void frank_wolf (double ** dist_mat, double ** yone, double ** zone, double ** wone, double rho, int N, int K) {
    // cout << "within frank_wolf" << endl;
    double sum_zone = mat_sum(zone, N, N);
    // STEP ONE: compute gradient mat initially
    vector< set< pair<int, double> > > actives (N, set<pair<int,double> >());
    vector< priority_queue< pair<int,double>, vector< pair<int,double> >, Compare> > pqueues (N, priority_queue< pair<int,double>, vector< pair<int,double> >, Compare> ());
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double grad =0.5*dist_mat[i][j]+yone[i][j]+rho*(wone[i][j]-zone[i][j]); 
            pqueues[i].push(make_pair(j, grad));
        }
    }
    // STEP TWO: iteration solve each row 
    int k = 0; // iteration number
    bool is_global_optimal_reached = false;
    set<pair<int,double> >::iterator it;
    // cout << "within frank_wolf: start iteration" << endl;
    while (k < K && !is_global_optimal_reached) {
        // compute new active atom: can be in active set or not
        vector< pair<int, double> > s (N, pair<int,double>());
        vector<bool> isInActives (N, false);
        for (int i = 0; i < N; i++) {
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
            if (k==0) {
                gamma = 1.0;
            } else {
                // gamma* = (sum1 + sum2 + sum3) / sum4, where
                // sum1 = 1/2 sum_n sum_k (w - s)_nk * || x_n - mu_k ||^2
                // sum2 = sum_n sum_k y_nk (w - s)_nk
                // sum3 = - rho * sum_n sum_k  (w - z) (w-s)
                // sum4 = sum_n sum_k rho * (s - w)^2
                for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                    double w_minus_s;
                    double w_minus_z = wone[i][it->first] - zone[i][it->first];
                    if (it->first == s[i].first) {
                        w_minus_s = wone[i][it->first]-1.0;
                    } else {
                        w_minus_s = wone[i][it->first];
                    }
                    sum1 += 0.5 * w_minus_s * dist_mat[i][it->first];
                    sum2 += yone[i][it->first] * w_minus_s;
                    sum3 += rho * w_minus_s * w_minus_z;
                    sum4 += rho * w_minus_s * w_minus_s; 
                }
                if (!isInActives[i]) {
                    sum1 += 0.5 * (-1.0) * dist_mat[i][s[i].first];
                    sum2 += yone[i][it->first] * (-1.0);
                    sum3 += rho * (-1.0) * (wone[i][s[i].first]-zone[i][s[i].first]);
                    sum4 += rho;
                }
                if (fabs(sum4) > FRANK_WOLFE_TOL) {
                    gamma = (sum1 + sum2 + sum3) / sum4;
                    if (gamma < FRANK_WOLFE_TOL) {
                        gamma = 0.0;
                        is_global_optimal_reached = true;
                    }
                } else {
                    gamma = 0.0;
                    is_global_optimal_reached = true;
                }
            }
#ifdef EXACT_LINE_SEARCH_DUMP
            if (gamma > 1 || gamma < 0) {
            cout << "[exact line search] (sum1, sum2, sum3, sum4, gamma) = ("
                << sum1 << ", " << sum2 << ", " << sum3 << ", " << sum4 << ", " << gamma
                << ")" << endl;
            }
#endif
#else
            gamma = 2.0 / (k+2.0);
#endif
            // update wone
            // for (int i = 0; i < N; i++) {
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                wone[i][it->first] *= (1-gamma);
            }
            wone[i][s[i].first] += gamma;
            // }    
            // update new actives 
            // for (int i = 0; i < N; i ++) {
            set<pair<int, double> > temp;
            if (!isInActives[i]) {
                actives[i].insert(pqueues[i].top());
                pqueues[i].pop();
            }
            double new_grad;
            for (it=actives[i].begin(); it!=actives[i].end(); ++it) {
                int j = it->first;
                new_grad=0.5*dist_mat[i][j]+yone[i][j]+rho*(wone[i][j]-zone[i][j]); 
                temp.insert (make_pair(it->first, new_grad));
            }
            actives[i].swap(temp);
            // cout << "actives[" << i << "]: " << actives[i].size() << endl;
        }
        // cout << "within frank_wolf: next iteration" << endl;
        k ++;
    }
    // compute value of objective function
    double penalty = first_subproblm_obj (dist_mat, yone, zone, wone, rho, N);
    // report the #iter and objective function
    // cout << "[Frank-Wolfe] iteration: " << k << ", first_subpro_obj: " << penalty << endl;
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
#ifdef BLOCKWISE_DUMP
        cout << "[Blockwise] (group_lasso, linear, quadratic) = ("
            << group_lasso << ", " << sum2 << ", " << sum3
            << ")" << endl;
#endif

        //cerr << group_lasso << ", " << sum2 << ", " << sum3 << endl;
        return group_lasso + sum2 + sum3;
    }

    void blockwise_closed_form (double ** ytwo, double ** ztwo, double ** wtwo, double rho, double* lambda, int N) {

        // STEP ONE: compute the optimal solution for truncated problem
        double ** wbar = mat_init (N, N);
        mat_zeros (wbar, N, N);
        mat_dot (rho, ztwo, wbar, N, N); // wbar = rho * z_2
        mat_sub (wbar, ytwo, wbar, N, N); // wbar = rho * z_2 - y_2
        mat_dot (1.0/rho, wbar, wbar, N, N); // wbar = (rho * z_2 - y_2) / rho

        // STEP TWO: find the closed-form solution for second subproblem
        for (int j = 0; j < N; j ++) {
            // 1. bifurcate the set of values
            vector< pair<int,double> > alpha_vec;
            for (int i = 0; i < N; i ++) {
                double value = wbar[i][j];
                /*if( wbar[i][j] < 0 ){
                  cerr << "wbar[" << i << "][" << j << "]" << endl;
                  exit(0);
                  }*/
                alpha_vec.push_back (make_pair(i, abs(value)));
            }
            // 2. sorting
            std::sort (alpha_vec.begin(), alpha_vec.end(), pairComparator);
            /*
               for (int i = 0; i < N; i ++) {
               if (alpha_vec[i].second != 0)
               cout << alpha_vec[i].second << endl;
               }
               */
            // 3. find mstar
            int mstar = 0; // number of elements support the sky
            double separator;
            double max_term = -INF, new_term;
            double sum_alpha = 0.0;
            for (int i = 0; i < N; i ++) {
                sum_alpha += alpha_vec[i].second;
                new_term = (sum_alpha - lambda[j]) / (i + 1.0);
                if ( new_term > max_term ) {
                    separator = alpha_vec[i].second;
                    max_term = new_term;
                    mstar = i;
                }
            }
            // 4. assign closed-form solution to wtwo
            if( max_term < 0 ){
                for(int i=0;i<N;i++)
                    wtwo[i][j] = 0.0;
                continue;
            }
            for (int i = 0; i < N; i ++) {
                // harness vector of pair
                double value = wbar[i][j];
                if ( abs(value) >= separator ) {
                    wtwo[i][j] = max_term;
                } else {
                    // its ranking is above m*, directly inherit the wbar
                    wtwo[i][j] = max(wbar[i][j],0.0);
                }
            }
        }
        // compute value of objective function
        double penalty = second_subproblem_obj (ytwo, ztwo, wtwo, rho, N, lambda);
        // report the #iter and objective function
        /*cout << "[Blockwise] second_subproblem_obj: " << penalty << endl;
          cout << endl;*/

        // STEP THREE: recollect temporary variable - wbar
        mat_free (wbar, N, N);
    }

    double overall_objective (double ** dist_mat, double* lambda, int N, double ** z) {
        // N is number of entities in "data", and z is N by N.
        // z is current valid solution (average of w_1 and w_2)
        double sum = 0.0;
        for(int i=0;i<N;i++)
            for(int j=0;j<N;j++)
                sum += z[i][j];
        // cerr << "sum=" << sum/N << endl;
        // STEP ONE: compute 
        //     loss = sum_i sum_j z[i][j] * dist_mat[i][j]
        double normSum = 0.0;
        for (int i = 0; i < N; i ++) {
            for (int j = 0; j < N; j ++) {
                normSum += z[i][j] * dist_mat[i][j];
            }
        }
        double loss = 0.5 * (normSum);
        cout << "loss=" << loss;

        // STEP TWO: compute dummy loss
        // sum4 = r dot (1 - sum_k w_nk) -> dummy
        double * temp_vec = new double [N];
        mat_sum_row (z, temp_vec, N, N);
        double dummy_penalty=0.0;
        double avg=0.0;
        for (int i = 0; i < N; i ++) {
            avg += temp_vec[i];
            dummy_penalty += r * max(1 - temp_vec[i], 0.0) ;
        }
        cout << ", dummy= " << dummy_penalty;

        // STEP THREE: compute group-lasso regularization
        double * maxn = new double [N]; 
        for (int i = 0;i < N; i ++) { // Ian: need initial 
            maxn[i] = -INF;
        }
        for (int i = 0; i < N; i ++) {
            for (int j = 0; j < N; j ++) {
                if ( fabs(z[i][j]) > maxn[j])
                    maxn[j] = fabs(z[i][j]);
            }
        }
        double sumk = 0.0;
        for (int i = 0; i < N; i ++) {
            sumk += lambda[i]*maxn[i];
        }
        double reg = sumk; 
        cout << ", reg=" << reg << endl;

        delete[] maxn;
        delete[] temp_vec;
        return loss + reg + dummy_penalty;
    }

    double noise(){
        return EPS * (((double)rand()/RAND_MAX)*2.0 -1.0) ;
    }
    /* Compute the mutual distance of input instances contained within "data" */
    void compute_dist_mat (vector<Instance*>& data, double ** dist_mat, int N, int D, dist_func df, bool isSym) {
        for (int i = 0; i < N; i ++) {
            for (int j = 0; j < N; j ++) {
                Instance * xi = data[i];
                Instance * muj = data[j];
                dist_mat[i][j] = df (xi, muj, D) + noise();
            }
        }
    }

    void cvx_clustering ( double ** dist_mat, int fw_max_iter, int max_iter, int D, int N, double* lambda, double ** W) {
        // parameters 
        double alpha = 0.1;
        double rho = 1;
        // iterative optimization 
        double error = INF;
        double ** wone = mat_init (N, N);
        double ** wtwo = mat_init (N, N);
        double ** yone = mat_init (N, N);
        double ** ytwo = mat_init (N, N);
        double ** z = mat_init (N, N);
        double ** diffzero = mat_init (N, N);
        double ** diffone = mat_init (N, N);
        double ** difftwo = mat_init (N, N);
        mat_zeros (yone, N, N);
        mat_zeros (ytwo, N, N);
        mat_zeros (z, N, N);
        mat_zeros (diffzero, N, N);
        mat_zeros (diffone, N, N);
        mat_zeros (difftwo, N, N);

        int iter = 0; // Ian: usually we count up (instead of count down)
        while ( iter < max_iter ) { // stopping criteria

#ifdef SPARSE_CLUSTERING_DUMP
            cout << "it is place 0 iteration #" << iter << ", going to get into frank_wolfe"  << endl;
#endif
            // mat_set (wone, z, N, N);
            // mat_set (wtwo, z, N, N);
            mat_zeros (wone, N, N);
            mat_zeros (wtwo, N, N);
            // STEP ONE: resolve w_1 and w_2
            frank_wolf (dist_mat, yone, z, wone, rho, N, fw_max_iter); // for w_1
#ifdef SPARSE_CLUSTERING_DUMP
            cout << "norm2(w_1) = " << mat_norm2 (wone, N, N) << endl;
#endif

            blockwise_closed_form (ytwo, z, wtwo, rho, lambda, N);  // for w_2
#ifdef SPARSE_CLUSTERING_DUMP
            cout << "norm2(w_2) = " << mat_norm2 (wtwo, N, N) << endl;
#endif

            // STEP TWO: update z by averaging w_1 and w_2
            mat_add (wone, wtwo, z, N, N);
            mat_dot (0.5, z, z, N, N);
#ifdef SPARSE_CLUSTERING_DUMP
            cout << "norm2(z) = " << mat_norm2 (z, N, N) << endl;
#endif

            // STEP THREE: update the y_1 and y_2 by w_1, w_2 and z

            mat_sub (wone, z, diffone, N, N);
            double trace_wone_minus_z = mat_norm2 (diffone, N, N); 
            mat_dot (alpha, diffone, diffone, N, N);
            mat_add (yone, diffone, yone, N, N);

            mat_sub (wtwo, z, difftwo, N, N);
            //double trace_wtwo_minus_z = mat_norm2 (difftwo, N, N); 
            mat_dot (alpha, difftwo, difftwo, N, N);
            mat_add (ytwo, difftwo, ytwo, N, N);

            // STEP FOUR: trace the objective function
            if (iter % 100 == 0) {
                error = overall_objective (dist_mat, lambda, N, z);
                // get current number of employed centroid
#ifdef NCENTROID_DUMP
                int nCentroids = get_nCentroids (z, N, N);
#endif
                cout << "[Overall] iter = " << iter 
                    << ", Overall Error: " << error 
#ifdef NCENTROID_DUMP
                    << ", nCentroids: " << nCentroids
#endif
                    << endl;
            }
            iter ++;
        }

        // STEP FIVE: memory recollection
        mat_free (wone, N, N);
        mat_free (wtwo, N, N);
        mat_free (yone, N, N);
        mat_free (ytwo, N, N);
        mat_free (diffone, N, N);
        mat_free (difftwo, N, N);
        // STEP SIX: put converged solution to destination W
        mat_copy (z, W, N, N);
    }

    // entry main function
    int main (int argc, char ** argv) {
        // exception control: illustrate the usage if get input of wrong format
        if (argc < 6) {
            cerr << "Usage: cvx_clustering [dataFile] [fw_max_iter] [max_iter] [lambda] [dmatFile]" << endl;
            cerr << "Note: dataFile must be scaled to [0,1] in advance." << endl;
            exit(-1);
        }

        // parse arguments
        char * dataFile = argv[1];
        int fw_max_iter = atoi(argv[2]);
        int max_iter = atoi(argv[3]);
        double lambda_base = atof(argv[4]);
        char * dmatFile = argv[5];

        // vector<Instance*> data;
        // readFixDim (dataFile, data, FIX_DIM);

        // read in data
        int FIX_DIM;
        Parser parser;
        vector<Instance*>* pdata;
        vector<Instance*> data;
        pdata = parser.parseSVM(dataFile, FIX_DIM);
        data = *pdata;
        // vector<Instance*> data;
        // readFixDim (dataFile, data, FIX_DIM);

        // explore the data 
        int dimensions = -1;
        int N = data.size(); // data size
        for (int i = 0; i < N; i++) {
            vector< pair<int,double> > * f = &(data[i]->fea);
            int last_index = f->size()-1;
            if (f->at(last_index).first > dimensions) {
                dimensions = f->at(last_index).first;
            }
        }
        assert (dimensions == FIX_DIM);

        int D = dimensions;
        cerr << "D = " << D << endl; // # features
        cerr << "N = " << N << endl; // # instances
        cerr << "lambda = " << lambda_base << endl;
        cerr << "r = " << r << endl;
        int seed = time(NULL);
        srand (seed);
        cerr << "seed = " << seed << endl;

        //create lambda with noise
        double* lambda = new double[N];
        for(int i=0;i<N;i++){
            lambda[i] = lambda_base + noise();
        }

        // pre-compute distance matrix
        dist_func df = L2norm;
        double ** dist_mat = mat_init (N, N);
        //  double ** dist_mat = mat_read (dmatFile, N, N);
        mat_zeros (dist_mat, N, N);
        compute_dist_mat (data, dist_mat, N, D, df, true); 
        ofstream dist_mat_out ("dist_mat");
        dist_mat_out << mat_toString(dist_mat, N, N);
        dist_mat_out.close();

        // Run sparse convex clustering
        double ** W = mat_init (N, N);
        mat_zeros (W, N, N);
        cvx_clustering (dist_mat, fw_max_iter, max_iter, D, N, lambda, W);
        ofstream W_OUT("w_out");
        W_OUT<< mat_toString(W, N, N);
        W_OUT.close();

        // Output cluster
        output_objective(clustering_objective (dist_mat, W, N));

        /* Output cluster centroids */
        output_model (W, N);

        /* Output assignment */
        output_assignment (W, data, N);

        /* reallocation */
        mat_free (dist_mat, N, N);
        mat_free (W, N, N);
    }
