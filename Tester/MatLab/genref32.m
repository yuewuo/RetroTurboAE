
ofst = 11579 * ones(1,32);
%ofst = [12219 0 0 0 0 0 ]
dat = zeros(2, 320, 8, 32);

for i = [1]
  x = readraw(sprintf("ref/rx_preamble_%d.raw", i - 1));
  avgd = grpacc(x(ofst(i):end, :), 320 * 8, 1000);
  avgd = avgd - sum(avgd(1+320*7:320*8,:))/320;
  avgd = avgd * (160 / sum(avgd(161+320*4:320*5,1+(i<=16))));
  figure; plot([avgd, -0.5+(mod(floor((0:320*8-1)/320)',2)==1)]);
  
  dat(:, :, :, i) = reshape(avgd', [2 320 8]);
end
p = fopen('wfm_refs.raw', 'wb');
fwrite(p, dat,'float');
fclose(p);


