import subprocess
import tempfile
import os
import numpy as _np
import scipy.io.wavfile as _wf


with subprocess.Popen(['arrow1', '--version'], stdout=subprocess.PIPE, universal_newlines=True) as _p:
    __version__, _ = _p.communicate()


def play_rec(play=None, rec=None, input_ports=None, output_ports=None, duration_secs=None, start_offset_secs=None, fs=None):
    """Frontend to arrow1 - play and record multi-channel sound using Jack

    :param play: if None - perform only recording; if numpy array - data for playback; 
    otherwise assume it's a path of audio file to play or file-like object. 
    :param rec: if True - record audio and return as numpy array; if evaluates to 
    False - perform only playback; otherwise assume it's a name of wav file to 
    record or file-like object.
    :param input_ports: names of Jack capture ports; if not set use arrow1 defaults
    (stereo with 2 first ports).
    :param output_ports: names of Jack playback ports; if not set use arrow1 defaults.
    :param duration_secs: max duration of record/playback in seconds.
    :param start_offset_secs: start playback of play at this offset.
    :param fs: required if play is numpy array (must match Jack engine sample rate), 
    otherwise ignored. 
    :returns: if rec is True the recorded audio as numpy array, otherwise None.
    """
    play_cleanup = False
    if isinstance(play, _np.ndarray):
        f = tempfile.NamedTemporaryFile(suffix='.wav', delete=False)
        _wf.write(f, fs, play)
        play = f.name
        play_cleanup = True

    rec_cleanup = False
    if rec is True:
        rec = tempfile.NamedTemporaryFile(suffix='.wav', delete=False).name
        rec_cleanup = True

    args=['arrow1']
    if input_ports:
        args.append('--in={}'.format(','.join(input_ports)))
    if output_ports:
        args.append('--out={}'.format(','.join(output_ports)))
    if play:
        args.append('--input-file={}'.format(play))
    if rec:
        args.append('--output-file={}'.format(rec))
    if duration_secs is not None:
        args.append('--duration={}'.format(duration_secs))
    if start_offset_secs is not None:
        args.append('--start={}'.format(start_offset_secs))

    with subprocess.Popen(args, stdout=subprocess.PIPE, universal_newlines=True) as p:
        std_out, _ = p.communicate()
        print(std_out)

    if play_cleanup:
        os.remove(play)

    if rec_cleanup:
        fs, arr = _wf.read(rec)
        os.remove(rec)
        return arr, fs

def get_ports():
    with subprocess.Popen(['arrow1', '--ports'], stdout=subprocess.PIPE, universal_newlines=True) as p:
        std_out, _ = p.communicate()
        return std_out
