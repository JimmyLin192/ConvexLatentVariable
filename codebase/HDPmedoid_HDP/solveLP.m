b = load('b');
beq = load('beq');
c = load('c');
lb = load('lb');

A = spconvert(load('A'));
Aeq = spconvert(load('Aeq'));

options = optimset('Display','iter','TolFun',1e-10);
[x,fval] = linprog(c,A,b,Aeq,beq,lb,[],[],options);
%x = bintprog(c,A,b,Aeq,beq);

fval

fp = fopen('sol','w');
d = length(x);
for i=1:d
	if( abs(x(i)) > 1e-3 )
		fprintf(fp,'%d\t%g\n',i,x(i));
	end
end
fclose(fp);

fp = fopen('fval','w');
fprintf(fp,'%g',fval);
fclose(fp);
