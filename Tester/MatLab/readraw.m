function a=readraw(fn, row,type)
  if nargin==2|| strcmp(type,'float')
    width=4; type='float';
  elseif strcmp(type,'int16')
    width=2;
  end
  b = fopen(fn, 'rb');
  fseek(b, 0, 'eof');
  sz = ftell(b) / (row * width);
  frewind(b);
  a = fread(b, [row, sz], type)';
  fclose(b);
end
