function gr = grpacc(dat, pergroup, numgroup) % dat: 2col vector
  dat1 = dat(:,1)+1i*dat(:,2);
  rs = reshape(dat1(1 : pergroup * numgroup), pergroup, numgroup);
  gr1 = (sum(rs') / numgroup)';
  gr = [real(gr1), imag(gr1)];
end