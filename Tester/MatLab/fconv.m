function y = fconv(x, h)
  Ly=length(x)+length(h)-1;  
  Ly2=pow2(nextpow2(Ly));    	                      
  y=(ifft(fft(x, Ly2).* fft(h, Ly2), Ly2));      
  y=y(1:1:Ly);               
end