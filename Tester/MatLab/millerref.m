function s= millerref(a, delay, spb)
  k = reshape(a(delay: delay + spb * 22 - 1), [spb, 22]);
  f = @(i) (reshape(k(:,i),[length(i) * spb 1]));
  s = struct('T11',f(13),'T0011',f(1:2),'T0001',f(11:12),'T1001',0.5*(f(6:7)+f(17:18)),'T100001',0.5*(f(14:16)+f(19:21)),'T100011',0.5*(f(3:5)+f(8:10)));
  %clipboard('copy', jsonencode(s));
end
% sed 's/{\|}//g;s/\[/{/g;s/\]/}/g;s/"T/[T/g;s/1"/1]/g;s/:/=/g' 