function millercheck(start, len, spb, wf, truth, decoded)
  close all;
  bits = len * 8;
  f=@(p) reshape(de2bi(sscanf(p, '%02X', len))', [bits 1]);
  a2 = cph(wf); 
  plot(a2);hold on;
  plot(conv([1,-1], mod(1:length(a2),spb)==mod(start,spb)))
  textpos = ((1:bits)-0.5)*spb+start;
  text(textpos, 0.4*ones(1,bits), num2str(f(truth))); 
  text(textpos, 0.6*ones(1,bits), num2str(f(decoded))); 
end