import subprocess
import tempfile
import os
import time
import numpy as _np
#import scipy.io.wavfile as _wf
#import wavefile as wf            # pip install wavefile (dependency: libsndfile)
import pysndfile                 # pip install pysndfile (dependency: libsndfile)
import psutil
import shutil

start_jack = ['/usr/bin/jackd', '-P70', '-dfirewire', '-r44100', '-p1024', '-n3', '&']


with subprocess.Popen(['arrow1', '--version'], stdout=subprocess.PIPE, universal_newlines=True) as _p:
    __version__, _ = _p.communicate()


def jack_running(name="jackd"):
    """Checks to see if the jack audio server is running, returns process id if so, -1 if not

    """
    ret = -1
    for p in psutil.process_iter():
        if name in p.name():
            ret = p.pid
            break
    return ret


def play_rec(play=None, rec=None, input_ports=None, output_ports=None, duration_secs=None, start_offset_secs=None, fs=None):
    """Frontend to arrow1 - play and record multi-channel sound using Jack

    Parameters
    ----------
    play: array or str
        numpy arrays are assumed to be data for playback, otherwise a path to a soundfile
        or a file-like object. Use None for recording only
    rec: bool or str
        if True, then record audio and return as numpy array; if evaluates to
        False then perform only playback; otherwise assume it's a path of wav file to
        record or file-like object
    input_ports: str
        Names of Jack capture ports, as a comma-separated str; if not set use arrow1 
        defaults (stereo with 2 first ports). Use get_ports function to get port names.
    output_ports: str
        Names of Jack playback ports; if not set use arrow1 defaults.
    duration_secs: float
        max duration of record/playback in seconds.
    start_offset_secs: float
        start playback of play at this offset.
    fs: int
        Sampling frequency. Required if play is numpy array (must match Jack 
        engine sample rate), otherwise ignored.
    
    returns
    -------
    arr: array
        if rec parameter is True the recorded audio is returned as numpy array, 
        otherwise None

    """

    play_cleanup = False
    if isinstance(play, _np.ndarray):
        f = tempfile.NamedTemporaryFile(suffix='.wav', delete=False)
        pysndfile.sndio.write(f.name, play, fs, format='wav')
        play = f.name
        play_cleanup = True

    rec_cleanup = False
    if rec is True:
        rec = tempfile.NamedTemporaryFile(suffix='.wav', delete=False).name
        rec_cleanup = True

    args=['arrow1']
    if input_ports:
        args.append(f"--in={','.join(input_ports)}")
    if output_ports:
        args.append(f"--out={','.join(output_ports)}")
    if play:
        args.append(f"--read-file={play}")
    if rec:
        args.append(f"--write-file={rec}")
    if duration_secs is not None:
        args.append(f"--duration={duration_secs}")
    if start_offset_secs is not None:
        args.append(f"--start={start_offset_secs}")

    with subprocess.Popen(args, stdout=subprocess.PIPE, universal_newlines=True) as p:
        std_out, _ = p.communicate()
        print(std_out)

    if play_cleanup:
        os.remove(play)

    if rec_cleanup:
        arr, fs, enc_ = pysndfile.sndio.read(rec.name)
        os.remove(rec)
        return arr, fs

def get_ports():
    with subprocess.Popen(['arrow1', '--channels'], stdout=subprocess.PIPE, universal_newlines=True) as p:
        std_out, _ = p.communicate()
        return std_out


def play_rec_multi(play=None, rec=None, input_ports=None, output_ports=None, duration_secs=None, start_offset_secs=None, fs=None, pause=0):
    """Makes successive play_rec calls when relevant parameters are lists
        and returns a 2d array with each recording along dim 1

    """

    progress_len = 10

    play_l = play if isinstance(play, list) else [play]
#    rec_l  = rec if isinstance(rec, list) else [rec]
    in_l   = input_ports if isinstance(input_ports, list) else [input_ports]
    out_l  = output_ports if isinstance(output_ports, list) else [output_ports]

    n = max(len(play_l), len(in_l), len(out_l))

    rec_l = []

    for i in range(n):
        this_play = play_l[min(len(play_l)-1, i)]
        this_in = in_l[min(len(in_l)-1, i)]
        this_out = out_l[min(len(out_l)-1, i)]

        if n > 1 and pause == 0:
            ret = input(f"({i+1} of {n}: Hit any key")
        elif n > 1 and pause is not None:
            start = time.perf_counter()
            while time.perf_counter() < start + pause:
                sec = round(pause-(time.perf_counter()-start))
                per = round(((pause - sec)/pause)*progress_len)
                rem = progress_len-per
                print(f"\r{i+1} of {n}: [{'#'*per}{' '*rem}] {sec} sec remaining", end="")
                time.sleep(.1)
                print(f"\r{' '*80}\r", end="")

        ret = play_rec(play=this_play, rec=True, input_ports=this_in, output_ports=this_out,
                 duration_secs=duration_secs, start_offset_secs=start_offset_secs, fs=fs
                 )
        rec_l.append(ret)

    return rec_l
        #print(f"play={this_play}, rec={this_rec}, input_ports={this_in}, output_ports={this_out}, duration_secs={duration_secs}, start_offset_secs={start_offset_secs}, fs={fs}")

