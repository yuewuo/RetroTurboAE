"""
In order to fully utilize multiple cores to do emulations or data processing, this script use process pool to do all the commands in parallel
You should copy this file and rename it, just like what you do with lua scripts
"""

# YOUR CODE BEGINS HERE

PROCESS_COUNT = 11
SCRIPT_FILE = "200201_threads_demo.lua"  # your lua script filename
def SCRIPT_INLINE(evaluate_idx, evaluate_cnt):  # your inline lua script
    return "evaluate_idx = %d; evaluate_cnt = %d" % (evaluate_idx, evaluate_cnt)

def jobs():  # register jobs to finish
    # for evaluate_idx in range(1):
    for evaluate_idx in range(20):
        job = Job()
        job.output_filename = "job_%d.txt" % evaluate_idx
        job.script_inline = SCRIPT_INLINE(evaluate_idx, 10)
        job.register()

# YOUR CODE ENDS HERE

import multiprocessing, subprocess, os, sys, signal
job_list = []
class Job:
    def __init__(self):
        self.output_filename = None
        self.script_inline = None
    def register(self):
        assert self.output_filename is not None
        assert self.script_inline is not None
        global job_list
        self.job_idx = len(job_list)
        job_list.append(self)
def signal_handler(signum, frame):
    sys.exit(0)
def process(job):
    global job_list
    global SCRIPT_FILE
    # signal.signal(signal.SIGINT, signal_handler)
    outfile = open(job.output_filename, 'w')
    print("job", job.job_idx, "running")
    try:
        sid_assigned = "%04d" % job.job_idx
        subprocess.run(["python", "../WebGUI/lua/execlua_new_process.py", SCRIPT_FILE, job.script_inline, sid_assigned], stdout=outfile)
    except KeyboardInterrupt:
        print("job", job.job_idx, "exited")
    else:
        print("job", job.job_idx, "finished")
if __name__ == "__main__":
    jobs()
    original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    pool = multiprocessing.Pool(processes=PROCESS_COUNT)
    signal.signal(signal.SIGINT, original_sigint_handler)
    try:
        pool.map(process, job_list)
    except KeyboardInterrupt:
        print("Caught KeyboardInterrupt, terminating workers")
        pool.terminate()
    else:
        pool.close()
    pool.close()
    pool.join()
