#include "hdp_medoids.h"
#include <set>
using namespace std;

/* 
  TODO list: 
    1. [DONE] use mat for dist_mat 
       [DONE] remove compiler error
    2. double check the implementation for hdp_medoids
    3. implement multiple runs and pick up the best
 */

/* Compute the mutual distance of input instances contained within "data" */
void compute_dist_mat (double** dist_mat, Lookups* tables, int N, int D) {
    // STEP ZERO: parse input
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    vector< pair<int,int> > word_lookup = *(tables->word_lookup); 
    // STEP ONE: compute distribution for each document
    vector< map<int, double> > distributions (D, map<int, double>());
    for (int d = 0; d < D; d ++) {
        // a. compute sum of word frequency
        int sumFreq = 0;
        for (int w = doc_lookup[d].first; w < doc_lookup[d].second; w++) {
            sumFreq += word_lookup[w].second;
        }
        // b. compute distribution
        for (int w = doc_lookup[d].first; w < doc_lookup[d].second; w++) {
            int voc_index = word_lookup[w].first;
            double prob = 1.0 * word_lookup[w].second / sumFreq;
            distributions[d].insert(pair<int, double> (voc_index, prob));
        }
    }
    // STEP TWO: compute weight of word within one document
    for (int j = 0; j < D; j ++) {
        for (int d = 0; d < D; d ++) {
            for (int w = doc_lookup[d].first; w < doc_lookup[d].second; w++) {
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
#ifdef DIST_MAT_DUMP
                cout << "i= " << w << ", j=" << esmat_index << ", dist: " << dist << ", ";
                cout << "count_w_d1: " << count_w_d1 << ", prob_w_d2: " << prob_w_d2 << endl;
#endif
                dist_mat[w][j] = dist;
            }
        }
    }
}
void output_objective (vector<int> z, vector< vector<int> > v, vector<double> LAMBDAs, double** dist_mat, Lookups* tables) {
    int D = tables->nDocs;
    double lambda_global = LAMBDAs[0];
    double lambda_local = LAMBDAs[1];
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);

    double obj = 0.0; 
    double global = 0.0;
    double local = 0.0;
    set<int> globals;
    int nLocalMedoids = 0;
    for (int d = 0; d < D; d ++) {
        for (int i = doc_lookup[d].first; i < doc_lookup[d].second; i++) {
            int g = v[d][z[i]];
            cout << "z[" << i << "]= " << z[i];
            cout << " v[d].size = " << v[d].size() << endl;
            cout << "g=" << g << endl;
            obj += dist_mat[i][g];
            globals.insert(g);
        }
        nLocalMedoids += v[d].size();
    }
    global = globals.size() * lambda_global;
    local = nLocalMedoids * lambda_local;
    ofstream obj_out("opt_objective");
    obj_out << "Main: " << obj << endl;
    obj_out << "Global: " << global << endl;
    obj_out << "Local: " << local << endl;
    obj_out.close();
}
void output_model (vector< vector<int> > v, Lookups* tables) {
    int N = tables->nWords;
    int D = tables->nDocs;
    set<int> globals;
    ofstream medoids_out("opt_model");
    for (int d = 0; d < D; d ++) {
        int nLocalMedoids = v[d].size();
        for (int l = 0; l < nLocalMedoids; l++) 
            globals.insert(v[d][l]);
    }
    int m = 0;
    for (set<int>::iterator it=globals.begin();it!=globals.end();++it)  {
        // NOTE: all id and global_medoids id are 1-based
        medoids_out << "Medoids[" << m << "]: " << *it+1<< endl;
        m++;
    }
    medoids_out.close();
}
void output_assignment (vector<int> z, vector<vector<int> > v, Lookups* tables) {
    int D = tables->nDocs;
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    ofstream asgn_out("opt_assignments");
    for (int d = 0; d < D; d ++) {
        asgn_out << "d = " << d << endl;
        for (int i = doc_lookup[d].first; i < doc_lookup[d].second; i++) {
            // NOTE: all id and global_medoids id are 1-based
            asgn_out << "  id=" << i+1 << ", " << v[d][z[i]]+1 << "(1)" << endl;
        }
    }
    asgn_out.close();
}
void hdp_medoids (vector<int>& z, vector<vector<int> >& v, vector<int>& global_medoids, double** dist_mat, vector<double> LAMBDAs, Lookups* tables) {
    // SET MODEL-RELEVANT PARAMETERS 
    assert (LAMBDAs.size() == 2);
    double lambda_global = LAMBDAs[0];
    double lambda_local = LAMBDAs[1];
    double ALPHA = 1.0;
    double RHO = 1.0;
    int N = tables->nWords;
    int D = tables->nDocs;
    vector< pair<int,int> > doc_lookup = *(tables->doc_lookup);
    vector< pair<int,int> > word_lookup = *(tables->word_lookup); 

    /* A. DECLARE AND INITIALIZE INVOLVED VARIABLES AND MATRICES */
    // 1) randomize initial global cluster medoid 
    global_medoids[0] = random() % D;
    // 3) initialize local cluster indicator z_ij = 1 for all j = 1
    for (int d = 0; d < D; d ++) {
        for (int i = doc_lookup[d].first; i < doc_lookup[d].second; i++) {
            z[i] = 0;
        }
    }
    // 4) initialize global cluster association v_j1 = 1
    for (int d = 0; d < D; d ++) {
        v[d][0] = global_medoids[0];
    }
    /* B. Repetition until Convergence */
    int iter = 0;
    int MAX_ITER = 1000;
    while (iter < MAX_ITER) {
        cout << "========================iter=" << iter << "=============\n";
        for (int d = 0; d < D; d++) {
            for (int i = doc_lookup[d].first; i < doc_lookup[d].second; i ++) {
                // 1) preprocess distance
                // cout << "nLocalMedoids: " << v[d].size() << endl;
                double* processed_dist_mat = new double [D];
                for (int examplar = 0; examplar < D; examplar ++) {
                    int esmat_index = i + examplar * N;
                    int value = dist_mat[i][examplar];
                    processed_dist_mat[examplar] = value;
                }
                set<int> temp;
                int nGlobalMedoids= global_medoids.size();
                for (int g = 0; g < nGlobalMedoids; g++) 
                    temp.insert(global_medoids[g]);
                int nLocalMedoids = v[d].size();
                for (int l = 0; l < nLocalMedoids; l++) 
                    temp.erase(v[d][l]);
                for (set<int>::iterator it=temp.begin();it!=temp.end();++it) 
                    processed_dist_mat[*it] += lambda_local;
                // 2) if ..,  global and local augmentation
                //  if min_p d_ijp > \lambda_l + \lambda_g
                int min_index = -1;
                double min_value = INF; 
                for (int index = 0; index < D; index ++) {
                    if (processed_dist_mat[index] < min_value) {
                        min_index = index;
                        min_value = processed_dist_mat[index];
                    }
                }
                if (min_value > lambda_local+lambda_global) {
                    global_medoids.push_back(min_index);
                    v[d].push_back(min_index);
                    z[i] = nLocalMedoids;
                    nLocalMedoids ++;
                    cout << "global and local aug " << endl;
                }
                // 3) otherwise, local assignment or local augmentation
                else {
                    bool is_c_exist = false;
                    nLocalMedoids = v[d].size();
                    cout << "v[" << d << "]" << nLocalMedoids << endl;
                    for (int l = 0; l < nLocalMedoids; l ++) {
                       // cout << "min_index: " << min_index << ", v[d][l] =" << v[d][l] << endl;
                         cout << "  v[" << d <<"][" <<l <<"]:" << v[d][l] << endl;
                        if (v[d][l] == min_index) {
                            z[i] = l;
                            is_c_exist = true;
                            break;
                        }
                    }
                    if (!is_c_exist) { // local augmentation
                        cout << "nonexist! min_index: " << min_index << endl;
                        v[d].push_back(min_index);
                        cout << "min_index: " << min_index << ", v[d][last] =" << v[d][nLocalMedoids] << endl;
                        z[i] = nLocalMedoids;
                        nLocalMedoids ++;
                    }
                }
                delete[] processed_dist_mat;
            }
        }
        // 4) re-elect medoids for all local clusters
        for (int d = 0; d < D; d++) {
            int nLocalMedoids = v[d].size();
            for (int l = 0; l < nLocalMedoids; l++) {
                vector<int> words;
                for (int i = doc_lookup[d].first; i < doc_lookup[d].second; i++) 
                    if (z[i] == l) words.push_back(i);
                int nDocWords = words.size();
                double ** temp_dist_mat = mat_init(nDocWords, D);
                for (int w = 0; w < nDocWords; w++) 
                    for (int td = 0; td < D; td++) 
                        temp_dist_mat[w][td] = dist_mat[words[w]][td];
                double * sum_temp_dist_col = new double [D];
                mat_sum_col (temp_dist_mat, sum_temp_dist_col, nDocWords, D);
                int min_index = -1; 
                double min_value = INF; // min sum mu_jc
                for (int td = 0; td < D; td++) {
                    if (sum_temp_dist_col[td] < min_value) {
                        min_index = td;
                        min_value = sum_temp_dist_col[td];
                    }
                }
                int is_new_medoid_exist = false;
                for (int s = 0; s < l; s++) {
                    if (v[d][s] == min_index) {
                        v[d].erase(v[d].begin()+l);
                        is_new_medoid_exist = true;
                        for (int i=doc_lookup[d].first;i<doc_lookup[d].second; i++) 
                            if (z[i] == l) z[i] = s;
                    }
                    break;
                }
                if (!is_new_medoid_exist) 
                    v[d][l] = min_index;
                v[d][l] = min_index;
                delete[] sum_temp_dist_col;
                mat_free (temp_dist_mat, nDocWords, D);
            }
        }
        // 7) re-elect medoids for global cluster
        int nGlobalMedoids = global_medoids.size();
        cout << "re-election: " << nGlobalMedoids << endl;
        for (int g = 0; g < nGlobalMedoids; g++) {
            cout << "1" << endl;
            int p = global_medoids[g];
            vector<int> words;
            set<int> examplar_set;
            for (int d = 0; d < D; d++) {
                for (int i = doc_lookup[d].first; i < doc_lookup[d].second; i ++) {
                    if (v[d][z[i]] == p) {
                        words.push_back(i);
                        examplar_set.insert(d);
                    }
                }
            }
            vector<int> examplars;
            for (set<int>::iterator it=examplar_set.begin();it!=examplar_set.end();++it) 
            {
                examplars.push_back(*it);
                cout << "uniq_examplar: " << *it << endl;
            }
            // useless global_medoids
            if (examplars.size() == 0) { continue;}
            int R = words.size(); 
            int C = examplars.size();
            double** candidate_dist_mat = mat_init (R,C);
            for (int r = 0; r < R; r ++) 
                for (int c = 0; c < C; c ++) 
                    candidate_dist_mat[r][c] = dist_mat[words[r]][examplars[c]];
            double* min_d_rc = new double [C];
            mat_sum_col (candidate_dist_mat, min_d_rc, R,C);
            int min_index;
            double min_value = INF;
            for (int m = 0; m < C; m ++) {
                if (min_d_rc[m] < min_value) {
                    min_index = m;
                    min_value = min_d_rc[m];
                }
            }
            cout << "size: " << examplars.size() << ", min_index: " << min_index << endl;
            int new_p = examplars[min_index];
            global_medoids[g] = new_p; // update
            delete[] min_d_rc;
            mat_free (candidate_dist_mat, R,C);
        }
        iter ++;
    }
}

// entry main function
int main (int argc, char ** argv) {

    // EXCEPTION control: illustrate the usage if get input of wrong format
    if (argc < 5) {
        cerr << "Usage: " << endl;
        cerr << "\thdp_medoids [voc_dataFile] [doc_dataFile] [lambda_global] [lambda_local]" << endl;
        exit(-1);
    }

    // PARSE arguments
    string voc_file (argv[1]);
    string doc_file (argv[2]);
    vector<double> LAMBDAs (2, 0.0);
    LAMBDAs[0] = atof(argv[3]); // lambda_document
    LAMBDAs[1] = atof(argv[4]); // lambda_topic

    // preprocess the input dataset
    vector<string> voc_list;
    voc_list_read (voc_file, &voc_list);
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
    double** dist_mat = mat_init (N, D);
    mat_zeros (dist_mat, N,D);
    compute_dist_mat (dist_mat, &lookup_tables, N, D);

    ofstream dmat_out ("dist_mat");
    dmat_out << mat_toString(dist_mat, N,D);

    vector<int> global_medoids (1,0);
    vector<int> z (N, 0);
    vector< vector<int> > v (D,vector<int>(1,0));

    hdp_medoids (z, v, global_medoids, dist_mat, LAMBDAs, &lookup_tables);
    cout << "out" << endl;

    /* Output objective */
    output_objective (z, v, LAMBDAs, dist_mat, &lookup_tables);
    /* Output cluster centroids */
    output_model (v, &lookup_tables);
    /* Output assignment */
    output_assignment (z, v, &lookup_tables);

    /* reallocation */
    mat_free (dist_mat, N,D);
}
