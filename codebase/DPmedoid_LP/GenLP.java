import java.io.*;
import java.util.*;

public class GenLP{
	
	static Map<String,Integer> varIndexMap = new HashMap();
	static List<String> varNameList = new ArrayList();
	
	static double lambda;
	
	static double eps = 1e-2;
	static Random ran = new Random();
	
	public static void main(String[] args){
		
		if( args.length < 2){
			System.err.println("Usage: java GenLP [dist_mat] [lambda]");
			System.exit(0);
		}
		
		String fname = args[0];
		lambda = Double.valueOf(args[1]);
		
		List<double[]> distMat = readDistMat(fname);
		
		System.out.println("N="+distMat.size());
		System.out.println("lambda="+lambda);

		Map<Integer,Double> c_map = new HashMap(); 
		generate_c(distMat, c_map);
		System.out.println("num_var="+c_map.size());

		List<Double> c = new ArrayList();
		for(int i=0;i<c_map.size();i++){
			c.add(c_map.get(i));
		}
		//purturbate(c);
		
		List<List<Pair<Integer,Double>>> Aeq = new ArrayList();
		List<Double> beq = new ArrayList();
		generate_Aeq_beq(distMat, Aeq, beq);
		System.out.println("num_eq="+Aeq.size());

		List<List<Pair<Integer,Double>>> A = new ArrayList();
		List<Double> b = new ArrayList();
		generate_A_b(distMat, A, b);
		System.out.println("num_ineq="+A.size());
		
		List<Double> lb = new ArrayList();
		for(int i=0;i<varNameList.size();i++){
			lb.add(0.0);
		}
		
		writeLP(c,A,b,Aeq,beq,lb);
		writeVarName("varName",varNameList);
	}
	
	static void purturbate(List<Double> c){
		
		for(int i=0;i<c.size();i++){
			c.set( i, c.get(i)+noise() );
		}
	}

	static List<double[]> readDistMat(String fname){
		
		List<double[]> list = new ArrayList();
		try{
			BufferedReader bufr = new BufferedReader(new FileReader(fname));
			String line;
			String[] tokens;
			while( (line=bufr.readLine()) != null ){
				
				tokens = line.split(" ");
				double[] dist_arr = new double[tokens.length];
				for(int i=0;i<tokens.length;i++){
					dist_arr[i] = Double.valueOf(tokens[i]);
				}
				list.add(dist_arr);
			}
			
		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}

		return list;
	}

	static void writeVarName(String fname, List<String> names){
		try{
			BufferedWriter bufw = new BufferedWriter(new FileWriter(fname));
			for(String name: names){
				bufw.write(name+"\n");
			}
			bufw.close();

		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}
	}

	static void writeLP(List<Double> c, List<List<Pair<Integer,Double>>> A, List<Double> b, List<List<Pair<Integer,Double>>> Aeq, List<Double> beq, List<Double> lb){
		
		writeVect("c",c);
		writeMat("A",A);
		writeMat("Aeq",Aeq);
		writeVect("b",b);
		writeVect("beq",beq);
		writeVect("lb",lb);
	}
	
	static void writeMat(String fname, List<List<Pair<Integer,Double>>> A){
		
		try{
			BufferedWriter bufw = new BufferedWriter(new FileWriter(fname));
			bufw.write(A.size()+"\t"+varNameList.size()+"\t"+0+"\n");
			for(int i=0;i<A.size();i++){
				for(int j=0;j<A.get(i).size();j++){
					bufw.write((i+1)+"\t"+(A.get(i).get(j).key+1)+"\t"+A.get(i).get(j).value+"\n");
				}
			}
			bufw.close();

		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}
	}

	static void writeVect(String fname, List<Double> b){
		try{
			BufferedWriter bufw = new BufferedWriter(new FileWriter(fname));
			for(Double v: b){
				bufw.write(v+"\n");
			}
			bufw.close();
			
		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}
	}
	
	static void generate_A_b(List<double[]> distMat, List<List<Pair<Integer,Double>>> A, List<Double> b){
		
		int N = distMat.size();
		int K = distMat.get(0).length;
		
		// w_{i,j} \leq \xi_{j}, for all i,j
		for(int i=0;i<N;i++){
			for(int j=0;j<K;j++){
				List<Pair<Integer,Double>> a = new ArrayList();
				a.add( new Pair(varIndex("w["+i+","+j+"]"), 1.0) );
				a.add( new Pair(varIndex("xi["+j+"]"), -1.0) );
				A.add(a);
				b.add(0.0);
			}
		}
	}
	
	static void generate_Aeq_beq(List<double[]> distMat, List<List<Pair<Integer,Double>>> Aeq, List<Double> beq){
		
		int N = distMat.size();
		int K = distMat.get(0).length;

		// \sum_j w_{i,j} = 1, for all i
		for(int i=0;i<N;i++){
			List<Pair<Integer,Double>> a = new ArrayList();
			for(int j=0;j<K;j++){
				a.add( new Pair(varIndex("w["+i+","+j+"]"), 1.0) );
			}
			Aeq.add(a);
			beq.add(1.0);
		}
	}

	static void generate_c(List<double[]> distMat, Map<Integer,Double> c){
		
		int index;
		//c_{i,j}: \sum_{i} \sum_{j} w_{ij} d_{ij}
		double[] dist_arr;
		for(int i=0;i<distMat.size();i++){
			dist_arr = distMat.get(i);
			for(int j=0;j<dist_arr.length;j++){
				c.put( varIndex("w["+i+","+j+"]"), dist_arr[j] );
			}
		}
		
		//c_{xi_{j}} \lambda \sum_j \xi_j
		for(int j=0;j<distMat.get(0).length;j++){
			c.put( varIndex("xi["+j+"]"), lambda );
		}
	}

	static double noise(){
		return eps * ran.nextGaussian();
	}

	static Integer varIndex(String varName){
		
		Integer ind =  varIndexMap.get(varName);
		if( ind != null )
			return ind;
		else{
			ind = varNameList.size();
			varNameList.add(varName);
			varIndexMap.put(varName,ind);
			return ind;
		}
	}

}
