
%gain check%
t =resample(readraw('db.raw', 4), 80000, 56875);
IQ = [cph([t(:,1) t(:,2)]), cph([t(:,3) t(:,4)])];
%IQ=readraw('bin/rxdata.raw',2);
%n = 1000;
%IQ = randi([-2,2],n,2)+randn(n,2)*0.1;
%IQ = IQ * [1 0.5; -0.5 -1];
scatter(IQ(:,1), IQ(:,2), '.');
while 1
r=ginput(1); d = ginput(2);
IQ = ((d - r)' \ (IQ - r)')';
scatter(IQ(:,1), IQ(:,2), '.');

end