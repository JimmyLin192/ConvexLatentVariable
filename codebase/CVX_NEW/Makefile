cpps=$(shell ls *.cpp)

all: 
	g++ -fopenmp -O3 $(cpps) -o cvx_clustering 

push:
	git add cvx_clustering.cpp Makefile 
	git commit -m "upgrade"
	git push

clean:
	rm -rf cvx_clustering opt* dist_mat w_out plot*
