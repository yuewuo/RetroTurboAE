function bwrite(csvv, v)
csvwrite(csvv,v);
[n, m] = size(v);
f = fopen(sprintf('../inc_data/%s.test',csvv), 'w');
for i = 1 : n
  fprintf(f, '{');
  if m > 1
    fprintf(f, '%f,', v(i,1:m - 1));
  end
  fprintf(f, '%f},\n', v(i,m));
end
fclose(f);
end