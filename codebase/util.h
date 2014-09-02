/*###############################################################
## MODULE: util.cpp
## VERSION: 1.0 
## SINCE 2014-08-24
## AUTHOR Jimmy Lin (xl5224) - JimmyLin@utexas.edu  
## DESCRIPTION: 
##      
#################################################################
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/

#include<vector>
#include<iostream>
#include<fstream>
#include<map>
#include<set>
#include<cmath>    
#include<cstring>
#include<algorithm>

#include<stdio.h>
#include<stdlib.h>

#ifndef MAT_H
#include "mat.h"
#endif
#ifndef EXSPARSE_MAT_H
#include "exSparseMat.h"
#endif

using namespace std;
const double UTIL_INF = 1e300;

double dot(vector<double>& v, vector<pair<int,double> >& f);
double dot(double* v, vector<pair<int,double> >& f); 
double dot(double* v, double* w, int d);
void vadd(double* v, vector<pair<int,double> >& f);
void vadd(vector<double>& vec, vector<pair<int,double> >& f);
void vsub(double* v, vector<pair<int,double> >& f);
int max_index (vector<double>& vec);
int max_index (double* arr, int size);
double flip (double a); int flip (int a);

double clustering_objective (double ** dist_mat, double ** z, int N);
void get_all_centroids(double ** W, vector<int>* centroids, int nRows, int nCols);
void output_objective (double obj);
void output_model (double** W, int N); 
void output_model (Esmat* W);
void output_assignment (Esmat* W, vector< pair<int,int> >* word_lookup);
 
double sign (int input) {
    if (input > 0) return 1.0;
    else if ( input < 0 ) return -1.0;
    else return 0.0;
}
bool pair_Second_Elem_Comparator (const std::pair<int, double>& firstElem, const std::pair<int, double>& secondElem) {
    // sort pairs by second element with *decreasing order*
    return firstElem.second > secondElem.second;
}

class Instance {
	public:
	int id;
	int label;
	vector<pair<int,double> > fea;
	Instance(int _id){
		id = _id;
		_x_sq = -1.0;
	}
	double x_sq(){
		// Pre-calculated
		if( _x_sq != -1.0 ){
			return _x_sq;
		}
		// Calculate and Cache
		_x_sq = 0.0;
		double tmp;
		for(int i=0;i<fea.size();i++){
			tmp = fea[i].second;
			_x_sq += tmp*tmp;
		}
		return _x_sq;
	}
	private:
	double _x_sq;
};
class Cluster{
	public:
	int d;
	vector<double>  mu;
	double mu_sq;
	Cluster(int _d){
		d = _d;
		for(int i=0;i<d+1;i++)
			mu.push_back( ((double)rand()/RAND_MAX)*2-1 );
		mu[0] = 0.0;
		mu_sq = -1;
	}
	Cluster(vector<double>& _mu){
		
		d = _mu.size()-1;
		for(int i=0;i<_mu.size();i++)
			mu.push_back( _mu[i] );
		mu_sq = -1;
	}
	double dist(Instance* ins){
		if( mu_sq == -1 ){
			mu_sq = 0.0;
			for(int i=0;i<d+1;i++){
				mu_sq += mu[i]*mu[i];
			}
		}
		return  ins->x_sq() + mu_sq - 2*dot(mu,ins->fea);
	}
};
typedef double (* dist_func) (Instance*, Instance*, int); 
double L2norm (Instance * ins1, Instance * ins2, int D) {
    // TODO: 
    //   1. refine by using hash table to restore each instance
    double * diff = new double [D];
    for (int i = 0; i < D; i ++) 
        diff[i] = 0.0;
    int n1 = ins1->fea.size();
    for (int i = 0; i < n1; i ++) {
        if (ins1->fea[i].first-1 < 0) continue;
        diff[ ins1->fea[i].first-1 ] = ins1->fea[i].second;
    }
    int n2 = ins2->fea.size();
    for (int i = 0; i < n2; i ++) {
        if (ins2->fea[i].first-1 < 0) continue;
        diff[ ins2->fea[i].first-1 ] -= ins2->fea[i].second;
    }

    double norm = 0.0;
    for (int i = 0; i < D; i ++) {
        norm += diff[i] * diff[i];
    }
    norm = sqrt(norm);
    
    delete[] diff;
    //cout << norm << endl;
    return norm; 
}

class ScoreComparator{
	private:
	vector<double>* score_vec;
	public:
	ScoreComparator(vector<double>* vec){
		score_vec = vec;
	}
	bool operator() (int i,int j) { 
		return score_vec->at(i) < score_vec->at(j); 
	}
};

void read2D(char* fname, vector<Instance*>& data){
	ifstream fin(fname);
	int tmp;
	double x,y;
	int id=0;
	while(!fin.eof()){
		
		fin >> tmp >> x >> y;
		if( fin.eof() )break;

		Instance* ins = new Instance(id++);
		ins->fea.push_back(make_pair(1,x));
		ins->fea.push_back(make_pair(2,y));
		
		data.push_back(ins);
	}
	fin.close();
}

void readFixDim(char* fname, vector<Instance*>& data, int D){
	ifstream fin(fname);
	int tmp;
	double val;
	int id=0;
	while(!fin.eof()){
		fin >> tmp;
		if( fin.eof() )break;
		
		Instance* ins = new Instance(id++);
	       	for(int j=0;j<D;j++){
			fin >> val;
			ins->fea.push_back(make_pair(j+1,val));
		}
		data.push_back(ins);
		id++;
	}
	fin.close();
}
/* libsvm reader */
class Parser {
	public:
	static  vector< Instance* >*  parseSVM(char* fileName, int& numFea){
		ifstream fin(fileName);
		if( fin.fail() ){
			cerr << "cannot find file." << endl;
			exit(0);
		}
		
		vector< Instance* >* data = new vector< Instance* >();
		
		char line[100000];
		vector<string> tokens;
		numFea=0;
		int id=0;
		while( !fin.eof() ){
			
			Instance* ins = new Instance(id++);
			
			fin.getline(line,100000);
			string str = string(line);
			tokens = split(str," ");
			//if( str.length() < 2 )
			//	continue;

			//label
			ins->label = atoi(tokens[0].c_str());

			//feature
			for(int i=1;i<tokens.size();i++){
				
				vector<string> pv = split(tokens[i],":");
				pair<int,double> pair;
				pair.first = atoi(pv[0].c_str());
				pair.second = atof(pv[1].c_str());
				ins->fea.push_back(pair);
			}
			//cerr << "fea="<< ins->fea.back().second << endl;
			//cerr << data->size() << ", " << ins->fea.size() <<  endl;
			if( ins->fea.size()>0 && ins->fea.back().first > numFea )
				numFea = ins->fea.back().first;
			
			data->push_back( ins );
		}
		
		data->pop_back();
		return data;
	}

	static vector<string> split(string str, string pattern){
		vector<string> str_split;
		size_t i=0;
		size_t index=0;
		while( index != string::npos ){

			index = str.find(pattern,i);
			str_split.push_back(str.substr(i,index-i));

			i = index+1;
		}
		if ( str_split.back()=="" )
			str_split.pop_back();
		
		return str_split;
	}
};

template <class T>
void shuffle( vector<T>& vec ) {
	int l = vec.size();
	int j;
	for(int i=0; i<l; i++)
	{
		j = i+rand()%(l-i);
		swap(vec[i], vec[j]);
	}
}

double dot(vector<double>& v, vector<pair<int,double> >& f) {
	
	double sum = 0.0;
	for(int i=0;i<f.size();i++)
		sum += v[ f[i].first ] * f[i].second;

	return sum;
}

double dot(double* v, vector<pair<int,double> >& f) {
	
	double sum = 0.0;
	for(int i=0;i<f.size();i++)
		sum += v[ f[i].first ] * f[i].second;

	return sum;
}

double dot(double* v, double* w, int d) {
	
	double sum = 0.0;
	for(int i=0;i<d;i++) {
		sum += v[i]*w[i];
	}
	return sum;
}

void vadd(double* v, vector<pair<int,double> >& f) {
	for(int i=0;i<f.size();i++)
		v[ f[i].first ] += f[i].second;
}

void vadd(vector<double>& vec, vector<pair<int,double> >& f) {
	
	for(int i=0;i<f.size();i++)
		vec[ f[i].first ] += f[i].second;
}

void vsub(double* v, vector<pair<int,double> >& f) {
	for(int i=0;i<f.size();i++)
		v[ f[i].first ] -= f[i].second;
}

int max_index (vector<double>& vec) {
    double vmax = -UTIL_INF; 
    int imax;
    for (int i = 0; i < vec.size(); i ++) {
        if (vec[i] > vmax) {
            vmax = vec[i];
            imax = i;
        }
    }

    return imax;
}

int max_index (double* arr, int size) {
	
	double vmax= -UTIL_INF;
	int imax;
	for(int i=0;i<size;i++) {
		if( arr[i] > vmax ) {
			vmax = arr[i];
			imax = i;
		}
	}
	return imax;
}

double flip (double a) {
	return (a==0.0)?1.0:0.0;
}

int flip (int a) {
	return (a==0)?1:0;
}

double clustering_objective (double ** dist_mat, double ** z, int N) {
    // N is number of entities in "data", and z is N by N.
    // z is current valid solution (average of w_1 and w_2)
    // STEP ONE: compute 
    //     loss = sum_i sum_j z[i][j] * dist_mat[i][j]
    double clustering_loss = 0.0;
    for (int i = 0; i < N; i ++) {
        for (int j = 0; j < N; j ++) {
            clustering_loss += 0.5 * z[i][j] * dist_mat[i][j];
        }
    }
    return clustering_loss;
}
double clustering_objective (Esmat* dist_mat, Esmat* z) {
    // z is current valid solution (average of w_1 and w_2)
    return 0.5*esmat_frob_prod(dist_mat, z);
}
void get_all_centroids(double ** W, vector<int>* centroids, int nRows, int nCols) {
    double * max_belonging = new double [nCols];
    for (int i = 0; i < nCols; i ++) {
        max_belonging[i] = 0.0;
    }
    mat_max_col (W, max_belonging, nRows, nCols);
    for (int i = 0; i < nCols; i ++ ) {
        if (fabs(max_belonging[i]) > 0.3) {
            centroids->push_back(i+1);
        }
    }
    delete[] max_belonging;
}
void output_objective (double obj) {
    ofstream obj_out ("opt_objective");
    obj_out << obj << endl;
    obj_out.close();
}
void output_model (double** W, int N) {
    ofstream model_out ("opt_model");
    vector<int> centroids;
    get_all_centroids (W, &centroids, N, N); // contains index of all centroids
    int nCentroids = centroids.size();
    model_out << "nCentroids: " << nCentroids << endl;
    for (int i = 0; i < nCentroids; i ++) {
        model_out << "centroids[" << i <<"]: " << centroids[i] << endl;
    }
    model_out.close();
}

void output_assignment (double ** W, vector<Instance*>& data, int N) {
    ofstream asgn_out ("opt_assignment");
    // get all centroids
    for (int i = 0; i < N; i ++) {
        // output identification and its belonging
        asgn_out << "id=" << i+1 << ", fea[0]=" << data[i]->fea[0].second << ", ";  // sample id
        for (int j = 0; j < N; j ++) {
            if( fabs(W[i][j]) > 3e-1 ) {
                asgn_out << j+1 << "(" << W[i][j] << "),\t";
            }
        }
        asgn_out << endl;
        // output distance of one sample to each centroid 
        /*fout << "dist_centroids: (";
        for (int j = 0; j < nCentroids - 1; j ++) {
            fout << dist_mat[i][ centroids[j] ] << ", ";
        }
        fout << dist_mat[i][ centroids[nCentroids-1] ] << ")";

        fout << endl;*/
    }
    asgn_out.close();
}

