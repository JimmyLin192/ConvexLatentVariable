Y = load('Y');
Z = load('Z');

[N,K] = size(Y);
n = N*K+K;

rho = 1;
lambda= 10;

H = sparse(n,n);
for i = 1:N*K
	H(i,i) = rho/2;
end

C = Y - rho*Z;
c = C(:);
c = [c ; lambda*ones(K,1)];

A = sparse(N*K, n);
b = zeros(N*K,1);
count = 1;
for j = 1:K
	for i = 1:N
		A(count, (j-1)*N+i) = 1;
		A(count, N*K + j ) = -1;
		b(count) = 0.0;
		count = count + 1;
	end
end

lb = zeros(n,1);

x = quadprog(H,c,A,b,[],[],lb,[]);

W = reshape(x(1:N*K),N,K);
Wt = W';

fp = fopen('W_out','w');
for i = 1:N
	fprintf(fp,'%g ',Wt(:,i));
	fprintf(fp,'\n');
end
fclose(fp);

fp = fopen('xi_out','w');
fprintf(fp,'%g\n',x(N*K+1:end));
fclose(fp);
