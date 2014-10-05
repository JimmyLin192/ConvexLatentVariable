m = 4;
n = 5;

K = 3;

A = round(rand(m+1,n));
A(end,:) = ones(1,n);

b = ones(m+1,1);
b(end) = K;

fp = fopen('result','w');
T = 100000;
for i = 1:T
	A(1:end-1,:) = round(rand(m,n));
	x = A\b;
	if all(x<=1) && all(x>=0) 
		fprintf(fp,'%g\t',x);
		fprintf(fp,'\n');
	end
end
fclose(fp);
