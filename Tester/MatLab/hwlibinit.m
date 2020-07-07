function hwlibinit(~)
	if not(libisloaded('librtm'))
		addpath('bin')
		loadlibrary('librtm', 'rtm.h');
%DLLEXPORT  void init(const char* reader_port, const char* tag_port, const char* ref_filename, const char* log_filename);
		calllib('librtm', 'init', 'COM26', ...
		'refn4.ref', sprintf('%s.lib.log', datestr(now, 'yyyymmdd_HHMMSS')));
		fprintf('reader gain set to %f\n', calllib('librtm', 'gain_reset'));
	end
end