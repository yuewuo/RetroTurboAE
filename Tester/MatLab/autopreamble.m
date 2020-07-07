

preamble = struct('bps', 2,  'period', 32, 'duty', 32, ...
 'd', [0, 1, 3,   2, 2, 2, 2,   1, 1, 1, 1,   2, 2, 2, 2,  0, 0, 0]);

a1 = channel_preamble(preamble, 50, 1152);
unloadlibrary('librtm');



a=grpacc(a1(1+5760*3:end,:),5760,48);
ofst = [5343 1634 2864];

a2 = @(K, n) sum(a(K:K+n-1,:)) / n;
d = zeros(3,2);
for i = 1:3
  d(i,:) = a2(ofst(i), 160);
end
a = ((d(2:3,:) - d(1,:))' \ (a - d(1,:))')';
plot(a);
[~,i]=max(a(1:1000,2))
a = circshift(a, 395-i);
if 0
	filename = sprintf('preamble_ref%s.ref', datestr(now, 'yyyymmdd_HHMMSS'));
	f=fopen(filename, 'wb');
	fwrite(f, a(1:4096,:)','float');
	fclose(f);
	copyfile(filename, 'refn4.ref');
end

