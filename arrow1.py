import subprocess
import tempfile
import numpy as np
import medussa as m

p = subprocess.Popen('arrow1 --version', shell=True, stdout=subprocess.PIPE)
std_out, std_err = p.communicate()
__version__ = std_out

def arrow1(outwav, outports, inports, inwav=None, fs=44100., buffsize=65536):
    """arrow1 - play and record multi-channel sound using jack

    """
    if isinstance(outwav, np.ndarray):
        outfile = tempfile.NamedTemporaryFile(suffix='.wav',delete=False)
        outfilename = outfile.name
        m.write_wav(outfilename,outwav,fs)
    else:
        outfilename = outwav

    if inwav:
        infilename = inwav
    else:
        infile = tempfile.NamedTemporaryFile(suffix='.wav',delete=False)
        infilename = infile.name

    cmd = 'arrow1 --in={} --out={} --buffer={:} {} {}'.format(','.join(inports), ','.join(outports), buffsize, outfilename, infilename)
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    std_out, _ = p.communicate()
    print(std_out)
    if not inwav:
        ar,fs = m.read_file(infilename)
        return ar,fs
    else:
        return None

def get_ports():
    cmd = 'arrow1 --ports'
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    std_out, _ = p.communicate()
    return std_out
