k=readraw('bin/pulse_4b_ref9_4.refraw',2);
k=reshape(k', [2 320 2 8 32]);
k = reshape(k(2,:,:,:,1), 320, 2, 8);

ptot = sum(reshape(k(:,2,:)-k(:,1,:),[320,8]),2)/8;
[mf, ~] = wh(ptot, 20, norm(pulse));

for i = 1 : 8
    for j = 1 : 2
        subplot(2, 8, j * 8 + i - 8);
        tot1 = conv(k(:,j,i), mf);
        %plot(tot1);
        stem(tot1(20:20:end));
    end
end
%pulse = [0.0338707268238068;0.0353870242834091;0.0340017415583134;0.0319590196013451;0.0311061721295118;0.0301813390105963;0.0268164593726397;0.0216878149658442;0.0183908324688673;0.0202559977769852;0.0284051280468702;0.0414503589272499;0.0567826479673386;0.0732044652104378;0.0927613452076912;0.120142355561256;0.159457147121429;0.210955858230591;0.271200835704803;0.334972530603409;0.396377623081207;0.451458901166916;0.501184403896332;0.550137519836426;0.602638483047485;0.659914791584015;0.718832910060883;0.774060487747192;0.822389781475067;0.863352954387665;0.897119700908661;0.924164593219757;0.944927036762238;0.959087252616882;0.967278242111206;0.972163379192352;0.97626918554306;0.980771899223328;0.986045122146606;0.991015017032623;0.993312299251556;0.99197393655777;0.9888516664505;0.986606180667877;0.986210942268372;0.987205505371094;0.990012764930725;0.995871067047119;1.00332832336426;1.00757372379303;1.00565481185913;1.00057709217072;0.997870683670044;0.998792171478271;0.99924772977829;0.995830297470093;0.990368783473969;0.987224280834198;0.988417863845825;0.992830574512482;0.998196601867676;1.00223195552826;1.00315070152283;1.00123155117035;0.999431610107422;1.0003422498703;1.00302016735077;1.00457012653351;1.00387036800385;1.00225222110748;1.00110816955566;1.00000131130219;0.997771859169006;0.994731545448303;0.992499947547913;0.992415428161621;0.994327962398529;0.995984852313995;0.995225548744202;0.99337512254715;0.9933762550354;0.994829893112183;0.994090855121613;0.989354491233826;0.984036803245544;0.983355820178986;0.987391293048859;0.991392433643341;0.993058502674103;0.994262337684631;0.996558487415314;0.999260723590851;1.0005716085434;0.999437570571899;0.997132182121277;0.9957355260849;0.995754718780518;0.996274769306183;0.995433270931244;0.99185574054718;0.987105786800385;0.983882904052734;0.982655823230743;0.982848405838013;0.984125554561615;0.985086381435394;0.984012961387634;0.980787694454193;0.976923584938049;0.97457891702652;0.974506497383118;0.974718809127808;0.973117709159851;0.970587253570557;0.969494163990021;0.96998792886734;0.969307065010071;0.964863896369934;0.957351922988892;0.950438559055328;0.94735723733902;0.947433769702911;0.946648478507996;0.94240939617157;0.935853242874146;0.929350674152374;0.924769401550293;0.92277204990387;0.921308398246765;0.917723476886749;0.912480294704437;0.907555043697357;0.903770864009857;0.901347100734711;0.898928463459015;0.893471598625183;0.885056138038635;0.878232359886169;0.875985860824585;0.875525534152985;0.870937049388885;0.859493672847748;0.845704436302185;0.836881339550018;0.83415549993515;0.831611573696136;0.825024545192719;0.817878663539886;0.816086888313293;0.818133771419525;0.815331161022186;0.803096950054169;0.786704361438751;0.774387180805206;0.769132852554321;0.767810642719269;0.765679657459259;0.760650932788849;0.752971649169922;0.74316793680191;0.733016431331635;0.725703716278076;0.722052097320557;0.7187260389328;0.711927533149719;0.701897442340851;0.692577183246613;0.686911880970001;0.684351980686188;0.682464063167572;0.678179383277893;0.669484317302704;0.658657968044281;0.650659143924713;0.647147834300995;0.645008325576782;0.640031635761261;0.630745768547058;0.620043575763702;0.612637400627136;0.610119521617889;0.60972273349762;0.6079141497612;0.604029536247253;0.599850416183472;0.596249163150787;0.592164576053619;0.587066471576691;0.582357823848724;0.579475045204163;0.577120959758759;0.572173893451691;0.564586520195007;0.55813604593277;0.554731369018555;0.552227914333344;0.548318028450012;0.542995750904083;0.537806868553162;0.533696353435516;0.529342591762543;0.52359402179718;0.518168687820435;0.514730870723724;0.511732697486877;0.506169319152832;0.49688783288002;0.486467152833939;0.479326009750366;0.476798951625824;0.476278871297836;0.474922329187393;0.471183121204376;0.464666366577148;0.457048207521439;0.451232880353928;0.448242038488388;0.445563048124313;0.439693123102188;0.430985361337662;0.424015939235687;0.421223849058151;0.41848224401474;0.410080015659332;0.397441446781158;0.38898754119873;0.389645874500275;0.39470037817955;0.396399468183517;0.392115026712418;0.38419497013092;0.37547355890274;0.366203606128693;0.354985445737839;0.342705994844437;0.333239585161209;0.329182028770447;0.329126477241516;0.329727113246918;0.329082071781158;0.327369093894958;0.324595212936401;0.319740027189255;0.312939941883087;0.306459993124008;0.302420169115067;0.300453275442123;0.298341900110245;0.294975489377975;0.291097730398178;0.286703318357468;0.28075760602951;0.27421036362648;0.270066767930984;0.269677132368088;0.270288795232773;0.266973108053207;0.257918238639832;0.247168451547623;0.240711197257042;0.240893170237541;0.244843855500221;0.246654316782951;0.241854563355446;0.231302946805954;0.220697104930878;0.215675190091133;0.216094851493835;0.215883821249008;0.21077673137188;0.203344613313675;0.197530344128609;0.192629426717758;0.185405701398849;0.175511658191681;0.167605146765709;0.166347235441208;0.168191537261009;0.163752898573875;0.150892615318298;0.138818264007568;0.136556327342987;0.142073556780815;0.145217031240463;0.140063419938087;0.131232932209969;0.126303896307945;0.126266658306122;0.126932471990585;0.125320613384247;0.121250368654728;0.115914076566696;0.111175559461117;0.108625151216984;0.108051411807537;0.107288241386414;0.104355566203594;0.0994288772344589;0.0941831022500992;0.0902023017406464;0.0884304046630859;0.0887594446539879;0.0898024588823318;0.089766301214695;0.087774321436882;0.0848094820976257;0.083019532263279;0.0834129601716995;0.0854400098323822;0.088214248418808;0.0895530581474304;0.0863001942634583;0.0791160836815834;0.0734469965100288;0.0730002671480179;0.075901672244072;0.0782314836978912;0.0780538022518158;0.0756261944770813;0.0717431604862213];
%pulse = [1:3:240 240:-1:1]; pulse = pulse / sum(pulse) * length(pulse) + randn(1,length(pulse)) * 0.01;
%pulse = pu
[m, d] = wh(pulse,20, sqrt(pulse'*pulse));
function [mf, disc]= wh(p, d, g)
  zr = 0;
% 4000 sp, 250 pulse = 32, sp2 = 80000, deci = 20.
% n + kd, k = [-n/d + 1, n/d - 1], 2 * n / d - 1 of len
  %plot(p); figure;
  n = length(p); n1 = n / d;
  ac = conv(p, flipud(p));
  acd = ac(n + d * (- n1 + 1 : n1 - 1));
  %stem(acd);figure;
  %zplane(acd'); figure;
  z = sort(tf2zpk(acd, 1), 'ascend', 'ComparisonMethod','abs');
  mf=flipud(filter(1, upsample(poly(z(1 : n1 - 1)), d), [p; zeros(zr, 1)])) * g; %magic
  %the maximum phase REVERSED!
  %plot(mf); hold on; plot(p * g); figure;
  tot = conv(p,mf); % plot((tot));  hold on; plot((ac)); figure
  disc = tot(20:20:end);%stem(disc);figure;
  [~,i]=max(tot)
end
