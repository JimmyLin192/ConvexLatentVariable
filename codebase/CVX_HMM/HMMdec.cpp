#include "HMMdec.h"

int main(int argc, char** argv){

	if( argc < 3 ){
		cerr << "Usage: ./HMMdec [seq_data] [ref_seq] (output)" << endl;
		exit(0);
	}

	char* file_seq = argv[1];
	char* file_ref = argv[2];
	char* file_out;
	if( argc > 3 )
		file_out = argv[3];
	else
		file_out = "output";
	
	vector<vector<char> > seqs;
	readData( file_seq, seqs );
	
	//Suppose we have ground truth root sequence
	vector<char> ref_seq;
	readRefSeq( file_ref, ref_seq );
	
	//set up Model
	PairHMMState::setupPairHMM(0.2,0.1818,0.2);

	//Use Pair
	vector<vector<State*> > output_alignment;
	double logLike, dataLogLike=0.0;
	for(int i=0;i<seqs.size();i++){
		output_alignment.push_back(vector<State*>());
		//logLike = align_Viterbi( seqs[i], ref_seq, new PairHMMState(State::BEGIN,0,0), output_alignment[i] );
		logLike = forward_backward( seqs[i], ref_seq, new PairHMMState(State::BEGIN,0,0) );
		//print_states(output_alignment[i],cerr); cerr << endl;
		dataLogLike += logLike;
	}
	cout << "LogLike=" << dataLogLike << endl;
	
	//writeOutput( file_out, seqs, ref_seq, output_alignment );

	return 0;
}
