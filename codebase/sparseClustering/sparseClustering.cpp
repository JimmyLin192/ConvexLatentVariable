/*###############################################################
## MODULE: sparseKmeans.cpp
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

#include "sparseClustering.h"

double ** mat_init (int nRows, int nCols) {

    double ** res = new double * [nRows];
    for (int i = 0; i < nRows; i ++) {
        res[i] = new double [nCols];
    }

    return res;
}

void mat_free (double ** src) {

    // TODO: recollect the memory
    
    return ;
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

void mat_dot (double scalar, double ** src, double ** dest, int nRows, int nCols) {

    for (int i = 0; i < nRows; i ++) {
        for (int j = 0; j < nCols; j ++) {
            dest[i][j] = scalar * src[i][j];
        }
    }

}

double sign (int input) {

    if (input >= 0) return 1.0;
    else return -1.0;

}

bool pairComparator (const std::pair<int, double>& firstElem, const std::pair<int, double>& secondElem) {
    // sort pairs by second element with decreasing order
    return firstElem.second > secondElem.second;
}

void blockwise_closed_form (double ** ytwo, double ** ztwo, double ** wtwo, double rho, double lambda, int N) {

    // STEP ONE: compute the optimal solution for truncated problem
    double ** wbar = mat_init (N, N);
    mat_dot (rho, ztwo, wbar, N, N); // wbar = rho * z_2
    mat_sub (wbar, ytwo, wbar, N, N); // wbar = rho * z_2 - y_2
    mat_dot (1.0/rho, wbar, wbar, N, N); // wbar = (rho * z_2 - y_2) / rho

    // STEP TWO: find the closed-form solution for second subproblem
    for (int j = 0; j < N; j ++) {
        // 1. bifurcate the set of values
        vector< pair<int,double> > alpha_vec;
        for (int i = 0; i < N; i ++) {
            double value = wbar[i][j];
            alpha_vec.push_back (make_pair(i, abs(value)));
        }

        // 2. sorting
        std::sort (alpha_vec.begin(), alpha_vec.end(), pairComparator);

        // 3. find mstar
        int mstar = 0; // number of elements support the sky
        double separator;
        double old_term = -INF, new_term;
        double sum_alpha = 0.0;
        for (int i = 0; i < N; i ++) {
            sum_alpha += alpha_vec[i].second;
            new_term = (sum_alpha - lambda) / (i + 1.0);
            if ( new_term < old_term ) {
                separator = alpha_vec[i].second;
                break;
            }
            mstar ++;
            old_term = new_term;
        }
        double max_term = old_term; 

        // 3. assign closed-form solution to wtwo
        for (int i = 0; i < N; i ++) {
            // harness vector of pair
            double value = wbar[i][j];
            if ( abs(value) > separator ) { 
                wtwo[i][j] = sign(wbar[i][j]) * max_term;
            } else {
                // its ranking is above m*, directly inherit the wbar
                wtwo[i][j] = wbar[i][j];
            }
        }
    }
    // STEP THREE: recollect wbar
    mat_free (wbar);

}

double L2norm (Instance * ins1, Instance * ins2, int N) {
    // TODO: need to accelerate this part of codes

    double * vec1 = new double [N];
    double * vec2 = new double [N];

    for (int i = 0; i < ins1->fea.size(); i ++) {
        vec1[ ins1->fea[i].first ] = ins1->fea[i].second;
    }
    for (int i = 0; i < ins2->fea.size(); i ++) {
        vec2[ ins2->fea[i].first ] = ins2->fea[i].second;
    }
    
    double norm = 0.0;
    for (int i = 0; i < N; i ++) {
        norm += (vec1[i] - vec2[i]) * (vec1[i] - vec2[i]);
    }

    delete vec1, vec2;

    return norm;
}

double dist_func (Instance * ins1, Instance * ins2, int N) {
    return L2norm(ins1, ins2, N);
}

double opt_objective (vector<Instance*>& data, double lambda, int N, double ** z) {
    // N is number of entities in "data", and z is N by N.
    // z is current valid solution (average of w_1 and w_2)
    
    // STEP ONE: compute loss function
    // TODO: optimize in terms of symmetry
    double normSum = 0.0;
    for (int i = 0; i < N; i ++) {
        for (int j = 0; j < N; j ++) {
            Instance * xi = data[i] ;
            Instance * muj = data[j];
            double dist =  dist_func (xi, muj, N);
            normSum += z[i][j] * dist;
        }
    }
    double loss = 0.5 * normSum;

    // STEP TWO: compute group-lasso regularization
    double * maxn = new double [N]; 
    for(int i=0;i<N;i++) //Ian: need initial 
    	maxn[i] = -INF;
    for (int i = 0; i < N; i ++) {
        for (int j = 0; j < N; j ++) {
            if (z[j][i] > maxn[i])
                maxn[i] = z[j][i];
        }
    }
    double sumk = 0.0;
    for (int i = 0; i < N; i ++) {
        sumk = maxn[i];
    }
    double reg = lambda * sumk; 

    return loss + reg;
}

void sparseClustering ( vector<Instance*>& data, int D, int N, double lambda, double ** W) {

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
    double ** diffone = mat_init (N, N);
    double ** difftwo = mat_init (N, N);

    int iter = 0; //Ian: usually we count up (instead of count down)
    int max_iter = 1000;
    while ( iter < max_iter ) { // stopping criteria
        // STEP ONE: resolve w_1 and w_2
        // frank_wolf ();
        blockwise_closed_form (ytwo, z, wtwo, rho, lambda, N); 

        // STEP TWO: update z by w_1 and w_2
        mat_add (wone, wtwo, z, N, N);
        mat_dot (0.5, z, z, N, N);

        // STEP THREE: update the y_1 and y_2 by w_1, w_2 and z
        mat_sub (wone, z, diffone, N, N);
        mat_dot (alpha, diffone, diffone, N, N);
        mat_sub (yone, diffone, yone, N, N);

        mat_sub (wtwo, z, difftwo, N, N);
        mat_dot (alpha, difftwo, difftwo, N, N);
        mat_sub (ytwo, difftwo, ytwo, N, N);

        // STEP FOUR: trace the objective function
        error = opt_objective (data, lambda, N, z);
        cout << "iter=" << iter << " Overall Error: " << error << endl;

        iter ++;
    }

}

// entry main function
int main (int argc, char ** argv) {
    
    // exception control
    if (argc < 3) {
        cerr << "Usage: sparseClustering [dataFile] [lambda]" << endl;
        cerr << "Note: dataFile must be scaled to [0,1] in advance." << endl;
        exit(-1);
    }

    // parse arguments
    char * dataFile = argv[1];
    double lambda = atof(argv[2]);

    // read in data
    vector<Instance*> data;
    read2D (dataFile, data);

    // explore the data 
    int dimensions = -1;
    int N = data.size(); // data size
    for (int i = 0; i < N; i++) {
        vector< pair<int,double> > * f = &(data[i]->fea);
        int last_index = f->size() - 1;
        if (f->at(last_index).first > dimensions) {
            dimensions = f->at(last_index).first;
        }
    }

    int D = dimensions;
    cout << "D = " << D << endl;
    cout << "N = " << N << endl;
    cout << "lambda = " << lambda << endl;
    int seed = time(NULL);
    srand(seed);
    cout << "seed = " << seed << endl;

    // Run sparse convex clustering
    map<int, Cluster*> clusters;
    double ** W = mat_init (N, N); 
    sparseClustering (data, D, N, lambda, W);
    // Output results

}
