import java.io.*;
import java.util.*;

public class CalcDist{
	
	public static void main(String[] args){
		
		if( args.length < 2){
			System.err.println("Usage: java CalcDist [dist_mat] [sol.name]");
			System.exit(0);
		}
		
		String fname = args[0];
		String fname_sol = args[1];

		List<double[]> distMat = readDistMat(fname);
		System.out.println("N="+distMat.size());
		
		Map<Integer,List<Integer>> clusters = readSol(fname_sol);
		
		//Calculate minimum distance between points not in the same cluster
		for(Integer j: clusters.keySet()){
			List<Integer> list1 = clusters.get(j);
			double min_dist = 1e300;
			for(Integer k: clusters.keySet()){
				if( j.equals(k) )
					continue;
				
				List<Integer> list2 = clusters.get(k);

				for(Integer i: list1)
					for(Integer i2: list2){
						if( distMat.get(i)[i2] < min_dist )
							min_dist = distMat.get(i)[i2];
					}
			}

			double max_dist = -1e300;
			for(Integer i: clusters.get(j)){
				if( distMat.get(j)[i] > max_dist )
					max_dist = distMat.get(j)[i];
			}
			System.out.println("cluster"+j+", min_inter_cluster_dist="+min_dist);
			System.out.println("cluster"+j+", max_intra_cluster_dist="+max_dist);
		}
	}
	
	static Map<Integer,List<Integer>> readSol(String fname){
		
		Map<Integer, List<Integer>> clusters = new HashMap();
		try{
			BufferedReader bufr = new BufferedReader(new FileReader(fname));
			String line, substr;
			String[] i_j;
			Integer i,j;
			while( (line=bufr.readLine()) != null ){
				
				if( line.charAt(0) != 'w' )
					continue;
				substr = line.substring(2,line.length()-3);
				
				i_j = substr.split(",");
				i = Integer.valueOf(i_j[0]);
				j = Integer.valueOf(i_j[1]);
				
				List<Integer> list;
				if( (list=clusters.get(j)) == null ){
					list = new ArrayList();
					clusters.put(j,list);
				}

				list.add(i);
			}
			
			bufr.close();
			
		}catch(Exception e){
			e.printStackTrace();
			System.exit(0);
		}

		return clusters;
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

}
