%IQ demod, find the carrier phase with the maximal directional variance.
function y = cph(x)
  n = length(x);
  ctn = @(x) x - sum(x) / n;
  I = ctn(x);
  [v, e]=eig(I'*I);
  [~,ind] = sort(diag(e),'desc');
  y =  I * v(:,ind(1));
end