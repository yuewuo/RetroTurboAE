%% The main function, including bits per symbol, ebno and rolloff factor for pulse shaping


function BER = ber(BPS, ebn0, rolloff) 
	Nb = 80000;% total number of bits
	SPS = 4; %sample-per-symbol
	%BPS = 2;
	MO = 2^BPS; %modulation order
	Ns = Nb / BPS; %number of symbols
	% ebn0 = -0;
	bits  = randi([0,1],1,Nb); %generating random bits
	high_order = bi2de(reshape(bits, [BPS, Ns])')'; %high order mapping
	
	symbols = conv(qammod(high_order, MO)); % TODO should bin2gray(., 'qam', MO)
	%and send to lib to encode as tag samples.
	%genqamdemod also accept symbols from 0..m-1.
	



	%Must use complex type to simulate bandpass transmission (two sidebands)
	%at baseband TODO replace with actual
	symbol_span = 30;
	pulse = rcosdesign(rolloff, symbol_span, SPS, 'sqrt');
	analog = fconv(pulse, upsample(symbols, SPS)); %Pulse shaping DONE BY LCD

	recv = chnn(complex(analog), ebn0, SPS, BPS); %Bandwidth limited AWGN Channel
	%TODO call library to receive rx.

	%Must convert the sample to complex number type, or BPSK will fail
	matched = fconv(pulse, recv); %Matched filter at the receiver

	res_start = 1 + symbol_span * SPS;
	recv_samp = matched(res_start : SPS : res_start + SPS * (Ns - 1)); %Sampling
	recv_bits = reshape(de2bi(qamdemod(recv_samp, MO), BPS)', [1, Nb]); %Demodulation TODO genqamdemod. bit by bit? yes.
	%
	BER = 1 - sum(recv_bits == bits) / Nb;
end


function scat(recv_samp)
	scatter(real(recv_samp), imag(recv_samp),'marker','.');
	set(gca, 'xlim', [-2, 2]);
	set(gca, 'ylim', [-2, 2]);
	xlabel('In-Phase');
	ylabel('Quadrature');
end
%% decision feedback equalizer

%% Zero-forcing equalizer
function E = zfeq(pr, n, main_idx, samp)
%usage: pr = [1, f1]; E = zfeq(pr, 3, 1); conv(pr, send_samp), conv(E,
% recv_samp)
  pad1 = n - main_idx;
  if (pad1 >= 0)
    pr = [zeros(1, pad1), pr];
  else 
    pr = pr(1-pad1:end);
  end
  pad2 = 2 * n - 1 - length(pr);
  if pad2 >= 0
    pr = [pr, zeros(1, pad2)];
  else
    pr = pr(1:2 * n - 1);
  end
  pr = fliplr(pr);
  % pr size 2n - 1
  % n is odd number
  
  mid = ceil(n / 2);
  eq = zeros(n, n + 1);
  for i = 1 : n
    eq(i,:) = [pr(n + 1 - i : 2 * n - i), (i == mid)];
  end
  
  eq = rref(eq);
  eq = eq(:, n + 1)';
  E = conv(samp, eq);
  l = floor(n / 2);
  E = E(main_idx + l: end - l);
end

