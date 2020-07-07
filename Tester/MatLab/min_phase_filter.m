

%% min_phase_filter computes the minimum phase counterpart of given filter.
% --- Minimum phase filter ------%
% A system with all of its poles and zeroes
% strictly inside the unit circle is called a
% MINIMUM PHASE system.
% This code is for stable filters. For poles outside the unit circle,
% there will be error.
%
%  Modified:
%
%   November 8 2013
%
%  Author:
%
%    Jigar Gada (Acknowledgement : Prof Richard Leahy)
%
%  Parameters:

%    Input: Vectors b and a, b -> coeffiecients of zeros
%       a --> coeffiecients of poles
%   e.g.
%   for the following transfer function,
%   H(z) = 2.5 + 0.5*z^-1 + 0.35*z^-2 + 5.47*z^-3
%             + 5.47*z^-4  + 0.35*z^-5  + 0.5*z^-6 + 2.5*z^-7;
%   The values of b and a are as follows:
%   b = [2.5 0.5 0.35 5.47 5.47 0.35 0.5 2.5];
%   a = [1];
%
%    Output: Plots of original and minimum phase filter.
%
%%--------------------------------------------------------------------------

function [] = min_phase_filter(b,a)

[b,a] = eqtflength(b,a);
% Get the poles and zeros from the transfer function. 
[z,p,k] = tf2zp(b,a);
len = length(z);
figure(1);
zplane(b,a)
title('Z plot of Original filter');
figure(2);
freqz(b,a);
title('Frequency response of Original filter');
zMag = abs(z);
pMag = abs(p);

% If poles outside the unit circle, exit
if(find(pMag >= 1))
    disp('poles outside/on the unit Circle, Unstable system');
    disp('Exiting the code');
    return;   
end
km = k;

z_minP = zeros(len,1);
for i = 1:len
%     if magnitude of zero greater than 1, replce 
%         the zero with its inverse conjugate and
%         scale the gain parameter.
    if(zMag(i) > 1)
        z_minP(i) = 1/conj(z(i));
        km = k*conj(z(i));
    else
        z_minP(i) = z(i);
    end
end

% Get the transfer function from the poles and zeros. 
[bmin, amin] = zp2tf(z_minP,p,km);
figure(3);
zplane(bmin,amin);
title('Z plot of Minimum phase filter');
figure(4);
freqz(bmin,amin);
title('Frequency response of Minimum phase filter');


end







