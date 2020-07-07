%payload should be struct(bps, period, dut, d) or struct array.
function a1 = channel_preamble(payload, repeat, gap)
	hwlibinit; 
	tag_samp_append = @(x) calllib('librtm', 'tag_samp_append', x.bps, ...
	 length(x.d), int32(x.d), x.period, x.duty);
  tag_samp_append(struct('bps', 2,  'period', 32, 'duty', 32, ...
	 'd', [0, 1, 3,   2, 2, 2, 2,   1, 1, 1, 1,   2, 2, 2, 2,  0, 0, 0])); % preamble
	if (gap > 0)
		tag_samp_append(struct('bps', 2,  'period', gap, 'duty', gap, 'd', 0));
	end
	arrayfun(tag_samp_append, repmat(payload, 1, repeat));
	buffer = libpointer('singlePtr', zeros(2, calllib('librtm', 'rx_samp2recv')));
	
	fprintf('reader return with preamble snr %f\n', calllib('librtm', 'channel', 5.0, buffer));
	a1 = buffer.Value'; clear buffer;
end

%{
cnst = @(bps) struct('bps', bps, 'period', 800, 'duty', 800, 'd', bin2gray(0:2^bps-1,'qam', 2^bps))
channel_preamble(cnst(4), 5, 0)
channel_preamble(cnst(8), 5, 0)
%}