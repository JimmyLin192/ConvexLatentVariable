import java.io.*;
import java.util.*;

class FreqList extends ArrayList<Pair<Integer,Integer>>{ }

public class GenLP{
	
	final static int EST_DOC_LEN = 10;
	final static int W = 0;
	final static int XI_K = 1;
	final static int DELTA_K = 2;
	final static int XI_VK = 3;
	
	static int w_offset = 0;
	static int xi_k_offset =0;
	static int delta_k_offset =0;
	static int xi_vk_offset =0;
	
	//final static double NOISE_PENALTY = 1e5; //penalty for a word not explained by any topic
	final static double EPS = 0.1; //topic asymmetric perturbation
	final static double INF = 1e200;

	public static void main(String[] args){
		
		if( args.length < 4 ){
			System.err.println("Usage: java GenLP [docs] [max_num_topic] [lambda_topic] [lambda_voc]");
			System.exit(0);
		}
		
		String docs_file = args[0];
		int K = Integer.valueOf(args[1]); //an upper bound on the number of topics
		double lambda_topic = Double.valueOf(args[2]);
		double lambda_voc = Double.valueOf(args[3]);
		
		List< FreqList >  docs = new ArrayList();
		List< FreqList >  docs_inv = new ArrayList();
		readDocs(docs_file, docs, docs_inv);
		int V = docs_inv.size();//vocabulary size
		
		System.out.println("|docs|="+docs.size() + ", |voc|=" + V +", lambda_topic=" +lambda_topic+", lambda_voc="+lambda_voc);
		int N = docs.size();
		int numVar = setOffset(N, K, V);
		int numIneq =  N*K + K;
		for(int v=0;v<docs_inv.size();v++){
			numIneq += K*docs_inv.get(v).size(); //how many docs involve this word;
		}
		int numEq = N;
		
		// Construct A (#constraints by #var), b (#constraints), c (#var) and lb (#var)
		double[] lb = new double[numVar];
		for(int i=0;i<numVar;i++)
			lb[i] = 0.0;
		
		double[] c = new double[numVar];
		for(int i=0;i<numVar;i++)
			c[i] = 0.0;
		//// asymmetric perturbation
		for(int i=0;i<N;i++){
			for(int k=0;k<K;k++){
				c[ varIndex(W, i*K+k) ] = EPS*k;
			}
		}
		//// topic_penalty
		for(int k=0;k<K;k++){
			c[ varIndex(XI_K, k) ] = lambda_topic;// + EPS*k;
		}
		//// vocabulary_penalty (delta_k)
		for(int k=0;k<K;k++){
			c[ varIndex(DELTA_K, k) ] = lambda_voc;// + EPS*k;
		}
		
		List<ArrayList<Pair<Integer,Double>>> A = new ArrayList();
		double[] b = new double[numIneq];
		int cur_b_ind=0;
		////w_dk <= \xi_k for all d,k
		for(int i=0;i<N;i++){
			for(int k=0;k<K;k++){
				ArrayList<Pair<Integer,Double>> spv = new ArrayList<Pair<Integer,Double>>();
				spv.add( new Pair( varIndex(W,i*K+k), 1.0) );
				spv.add( new Pair( varIndex(XI_K, k), -1.0) );
				A.add(spv);

				b[ cur_b_ind++ ] = 0.0;
			}
		}
		//// (\sum_{v\in Voc} \xi_vk) <= 1+\delta_k , for all k
		for(int k=0;k<K;k++){
			ArrayList<Pair<Integer,Double>> spv = new ArrayList<Pair<Integer,Double>>();
			spv.add(new Pair( varIndex(DELTA_K,k), -1.0));
			for(int v=0;v<docs_inv.size();v++){
				spv.add(new Pair(varIndex(XI_VK,v*K+k), 1.0));
			}
			A.add(spv);
			//b[ cur_b_ind++ ] = 1.0;
			b[ cur_b_ind++ ] = 0.0;
		}
		//// w_dk \leq \xi_vk, for (d,v) appears in docs
		Integer d;
		for(int v=0;v<docs_inv.size();v++){
			FreqList freqList = docs_inv.get(v);
			for(Pair<Integer,Integer> p: freqList){
				d = p.first;
				for(int k=0;k<K;k++){
					ArrayList<Pair<Integer,Double>> spv = new ArrayList<Pair<Integer,Double>>();
					spv.add(new Pair(varIndex(W,d*K+k),1.0));
					spv.add(new Pair(varIndex(XI_VK, v*K+k),-1.0));
					A.add(spv);
					
					b[ cur_b_ind++ ] = 0.0;
				}
			}
		}
		System.err.println(A.size()+" "+b.length+" "+cur_b_ind+" "+numIneq);

		List<ArrayList<Pair<Integer,Double>>> Aeq = new ArrayList();
		double[] beq = new double[numEq];
		int cur_beq_ind=0;
		////\sum_k w_dk = 1, for all d
		for(int i=0;i<N;i++){
			ArrayList<Pair<Integer,Double>> spv = new ArrayList<Pair<Integer,Double>>();
			for(int k=0;k<K;k++){
				spv.add(new Pair(varIndex(W,i*K+k),1.0));
			}
			Aeq.add(spv);
			
			beq[cur_beq_ind++] = 1.0;
		}
		assert cur_beq_ind == numEq;
		
		//x0
		double[] x0 = new double[numVar];
		for(int i=0;i<numVar;i++)
			x0[i] = 0.0;
		////w_d0=1
		for(int i=0;i<N;i++){
			x0[varIndex(W,i*K+0)] = 1.0;
			for(int k=1;k<K;k++)
				x0[varIndex(W,i*K+k)] = 0.0;
		}
		////\xi_0=1
		x0[varIndex(XI_K,0)] = 1.0;
		////\xi_v0=1
		for(int v=0;v<V;v++)
			x0[varIndex(XI_VK,v*K+0)] = 1.0;
		////\delta_0 = V-1
		x0[varIndex(DELTA_K,0)] = V-1;

		//Write A, b, c, Aeq, beq out
		writeToFile(A,b,c,Aeq,beq,lb,x0);
	}
	
	static void writeToFile(List<ArrayList<Pair<Integer,Double>>> A, double[] b, double[] c, List<ArrayList<Pair<Integer,Double>>> Aeq, double[] beq, double[] lb, double[] x0){
		
		writeSpMat(A,"A.txt", b.length, c.length);
		writeSpMat(Aeq,"Aeq.txt", beq.length, c.length);

		writeVect(b,"b.txt");
		writeVect(c,"c.txt");
		writeVect(beq,"beq.txt");
		writeVect(lb,"lb.txt");
		writeVect(x0,"x0.txt");
	}

	static void writeSpMat(List<ArrayList<Pair<Integer,Double>>> A, String fpath, int m, int n){
		try{	
			BufferedWriter bufw = new BufferedWriter(new FileWriter(fpath));
			bufw.write(""+m+"\t"+n+"\t"+0.0);
			bufw.newLine();
			for(int i=0;i<A.size();i++){
				for( Pair<Integer,Double> p : A.get(i) ){
					bufw.write(""+(i+1)+"\t"+(p.first+1)+"\t"+p.second);//plus 1 for matlab
					bufw.newLine();
				}
			}
			bufw.close();
		
		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}
	}

	static void writeVect(double[] v, String fpath){
		try{
			BufferedWriter bufw = new BufferedWriter(new FileWriter(fpath));
			for(int i=0;i<v.length;i++){
				bufw.write(""+v[i]);
				bufw.newLine();
			}
			bufw.close();
		
		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}
	}

	static int varIndex(int type, int index){
		
		switch(type){
			case W: return w_offset + index;
			case XI_K: return xi_k_offset + index;
			case DELTA_K: return delta_k_offset + index;
			case XI_VK: return xi_vk_offset + index;
			default: 
				    System.err.println("Unknown variable type: "+type);
				    return -1;
		}
	}
	
	static int setOffset(int N, int K, int V){

		w_offset = 0;
		xi_k_offset = w_offset + N*K;
		delta_k_offset = xi_k_offset + K;
		xi_vk_offset = delta_k_offset + K;
		
		return xi_vk_offset + V*K;
	}
	
	static void readDocs(String fpath, List<FreqList> docs, List<FreqList> inv_docs){
		
		try{
			BufferedReader bufr = new BufferedReader(new FileReader(fpath));
			String line;
			String[] tokens, wid_freq;
			int wid, freq;
			int V = -1;
			docs.clear();
			while( (line=bufr.readLine()) != null ){
				
				tokens = line.split(" ");
				
				FreqList freqList = new FreqList();
				for(int i=0;i<tokens.length;i++){
					wid_freq = tokens[i].split(":");
					wid = Integer.valueOf(wid_freq[0]);
					freq = Integer.valueOf(wid_freq[1]);
					
					freqList.add(new Pair(wid,freq));
					if( wid > V )
						V = wid;
				}
				
				docs.add(freqList);
			}
			bufr.close();
			V++;
			
			// construct inverted index
			inv_docs.clear();
			for(int i=0;i<V;i++){
				inv_docs.add(new FreqList());
			}
			for(int i=0;i<docs.size();i++){
				FreqList freqList = docs.get(i);
				for( Pair<Integer,Integer> p: freqList ){
					inv_docs.get(p.first).add(new Pair(i,p.second));
				}
			}
			
		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}
	}
}



class Pair<K,V>{
	public K first;
	public V second;
	public Pair(K k, V v){
		first = k;
		second = v;
	}
}
