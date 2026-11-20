#!/usr/bin/env python3
"""
Measure every stream in an ehRadio playlist and rewrite its third column with a per-station dB cut.

USAGE:
    python playlist_tool.py [file] [--estimate | --calculate] [options]

TARGET:
    [file]           Playlist to rewrite (default data/data/playlist.csv)
                     This tool only ever works on the one file it is given

THE THIRD COLUMN:
    dB0 .. dB24      Measured; dB0 means no cut is needed
    0                Not measured - the device learns it on the next play
    (any other)      Foreign, e.g. a yoRadio or ka-radio `ovol` in someone else's units - cleared to 0
    The "dB" PREFIX matters: yoRadio reads this field with atoi(), so atoi("dB12") is 0.

THE CUT:
    cut = measured - ref, clamped 0..DBCUT_MAX.  Attenuation only, so a stream can only be brought DOWN.

MODES, AND WHY THERE ARE TWO:
    The radio learns in two stages and only writes the file from the second, so a number from this tool is
    worth what the stage behind it is worth.  Both modes below are the radio's own arithmetic:

    --estimate   30 seconds per stream.  The ESTIMATE stage: three 10 s windows, each measured on its own so
                 a preroll advert cannot poison the number, chained with the half-step rule - half of the
                 remaining gap, rounded up.  This is what the radio APPLIES while it gathers data.  Good
                 enough to listen to, and NOT what the radio records.
    --calculate  5 minutes 30 seconds per stream.  The first CALCULATION: the mean from the 30 s mark to the
                 end of the sample, moved half-way from what the row already holds (or taken whole when the
                 row holds nothing).  This is the number the radio writes at 5:30 - the only length that
                 matches what the radio records, and therefore the one worth recording here.

    With neither flag the tool offers both, with the run time each would take.

MEASUREMENT, IN THE FIRMWARE'S OWN UNITS:
    ~46 ms blocks of raw s16le PCM; block level = 10*log10(mean(sample^2) / 32768^2), blocks below -50 dBFS
    ignored.  Deliberately NOT EBU R128 LUFS: K-weighting reads 2-4 dB high on music, and the reference
    (AUTO_DBCUT_REF) is a dBFS number.

SCHEDULING:
    Every stream is probed first: a two-second ffmpeg run that reads the codec line, so a station that will
    not answer is known in seconds instead of after a whole sample.  The bitrate it reports is what the
    scheduler works with, because what runs out is kbit/s in flight (--max-kbps), not a number of streams: a
    1400 kbit/s FLAC leaves no room for a second one but plenty for an MP3, and the next stream that FITS is
    started as soon as one finishes, so the queue keeps moving instead of waiting at the head.

    The budget itself is measured rather than guessed: a short parallel download from a plain file endpoint
    reports what the connection can do, and half of that becomes the budget.  A station cannot be used for
    this - an icecast stream delivers at exactly its encoded rate whatever the link is doing.

OPTIONS:
    --ref -18        Target level in dBFS (the radio's AUTO_DBCUT_REF)
    --estimate       The 30 s estimate stage: what the radio applies, not what it records
    --calculate      The 5 min 30 s first calculation: what the radio records at 5:30
    --timeout        Per-stream timeout (default: the mode's length + 15 s)
    --retries 4      Attempts after a failure, with 5, 10, 20 and 30 s gaps (0 disables)
    --max-kbps auto  Link budget in kbit/s: streams start only while their total bitrate fits in it.
                     'auto', the default, measures the connection and uses HALF of it
    --no-speed-test  Skip that measurement; the budget is then 2000, or whatever --max-kbps says
    --jobs 16        Hard ceiling on streams at once, clamped 1..32
    --speed-url      File endpoint used for the measurement (%d = bytes), default Cloudflare
    --speed-bytes 25 MB, --speed-streams 4, --speed-seconds 5
                     Shape of the measurement: total bytes, parallel connections, and a hard stop
    --all            Re-measure stations that already carry a cut

EXAMPLES:
    # Both modes offered, with the run time each would take
    py playlist_tool.py

    # The faithful run: the number the radio would write at 5:30
    py playlist_tool.py --calculate

    # A quick look at what the radio applies in its first 30 s
    py playlist_tool.py --estimate

    # Re-measure everything, including stations that already carry a value
    py playlist_tool.py --calculate --all

NOTE:
    The result goes to <file>.tmp first and only moves over the original when you answer y; n leaves the
    temp file in place.  Ctrl-C kills the running ffmpeg and leaves both untouched.  A stream that fails
    is retried --retries times first, because one refused connection is not a dead station: the reason
    shown then carries the attempt count.  A stream that fails its PROBE never reaches the measurement
    stage at all, so a dead address costs seconds rather than a wasted sample.  One that still will not
    answer is left unmeasured and gets
    " (404?)" appended to its name - once, however many times this runs, and not at all if the name
    already ends with "?)" (the WebUI Validator writes its own localized marker there).
    Requires ffmpeg on PATH.  Standard library only.
"""

import argparse
import concurrent.futures
import math
import os
import queue
import re
import statistics
import struct
import subprocess
import sys
import threading
import time
import urllib.request

DBCUT_MAX = 24
DEFAULT_FILE = os.path.join("data", "data", "playlist.csv")
ESTIMATE_WINDOWS = 3                    # the radio's estimate stage: three windows, each measured on its own
ESTIMATE_WINDOW_SECONDS = 10
ESTIMATE_SECONDS = ESTIMATE_WINDOWS * ESTIMATE_WINDOW_SECONDS      # 30
CALCULATE_SECONDS = ESTIMATE_SECONDS + 300                          # 330: 30 s of estimates, then 5 min
CALCULATE_FROM = float(ESTIMATE_SECONDS)                            # where the calculation's mean starts
SAMPLE_RATE = 44100
BLOCK_BYTES = 8192                      # ~46 ms of s16 stereo, close to the firmware's 50 ms block
BYTES_PER_SECOND = SAMPLE_RATE * 4      # s16 x 2 channels
GATE_DBFS = -50.0                       # the firmware ignores blocks below this
REFERENCE_DBFS = -18.0                  # AUTO_DBCUT_REF, overridden by --ref
RETRY_DELAYS = (5, 10, 20, 30)          # gap before each retry; the last value repeats if --retries is higher
DEFAULT_MAX_KBPS = 2000                 # the budget when the link cannot be measured
ASSUMED_KBPS = 320                      # what a stream costs when the probe does not say
PROBE_SECONDS = 2                       # how much audio the probe asks for
JOBS_MIN, JOBS_MAX = 1, 32
SPEED_URL = "https://speed.cloudflare.com/__down?bytes=%d"   # a file endpoint, built to be downloaded fast
SPEED_BYTES = 25000000                  # total to pull across all the speed streams
SPEED_STREAMS = 4                       # parallel connections in the speed test
SPEED_SECONDS = 5                       # and a hard stop, because a slow link would take minutes otherwise
SPEED_HEADERS = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ehRadio-playlist-tool"}
BUDGET_SHARE = 0.5                      # use half of what the link managed: the budget is about avoiding congestion
TAG = None                              # compiled below


# --- the grammar, mirroring Utility::parsePlaylistLine() in src/core/utility.cpp -------------------

def parse_line(line):
    """(name, url, dBcut) or None.  Accepts the new space shape and the old TAB shape."""
    text = (line or "").strip()
    if not text:
        return None
    if "\t" in text:
        fields = text.split("\t")
        if len(fields) != 3:
            return None
        name, url, value = fields
    else:
        parts = text.rsplit(None, 2)
        if len(parts) != 3:
            return None
        name, url, value = parts
    name, url = name.strip(), url.strip()
    if not name or not url:
        return None
    return name, url, cut_from_token(value)


def cut_from_token(tok):
    """-1 for anything that is not our tagged token, else the clamped value."""
    text = (tok or "").strip()
    if len(text) < 3 or text[0] not in "dD" or text[1] not in "bB":
        return -1
    digits = text[2:]
    if not digits.isdigit():
        return -1
    return max(0, min(DBCUT_MAX, int(digits)))


def format_line(name, url, dBcut):
    return "%s %s %s" % (name, url, "0" if dBcut < 0 else "dB%d" % dBcut)


def read_playlist(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape", newline="") as fh:
        raw = fh.read()
    lines = raw.replace("\r\n", "\n").split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return lines


def write_playlist(path, lines):
    with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
        fh.write("\r\n".join(lines) + "\r\n")


# --- measurement, in the firmware's own units ------------------------------------------------------

def _pump(stream, sink):
    """Feed blocks to a queue so the caller can time out.  A pipe read() blocks with no deadline of its
    own, so a stream that connects and then says nothing would hang the whole run without this."""
    try:
        while True:
            chunk = stream.read(BLOCK_BYTES)
            if not chunk:
                break
            sink.put(chunk)
    except Exception:
        pass
    sink.put(None)


def half_step(anchor, target):
    """Half of the remaining gap, rounded up in its direction - the radio's rule (7 -> 4, 3 -> 2, 1 -> 1)."""
    gap = target - anchor
    step = (gap + (1 if gap > 0 else -1)) // 2
    return max(0, min(DBCUT_MAX, anchor + step))


def level_to_cut(level, ref):
    """A stream LOUDER than the reference is the one that gets cut."""
    return max(0, min(DBCUT_MAX, int(round(level - ref))))


def estimate_cut(blocks, ref):
    """(cut, level): the radio's ESTIMATE stage, simulated window by window.

    Three windows, each measured from its own samples so a preroll advert cannot poison the number, chained
    with the half-step rule from no cut at all - the state a station is in the first time it is measured."""
    applied = 0
    levels = []
    for w in range(ESTIMATE_WINDOWS):
        window = [db for t, db in blocks
                  if w * ESTIMATE_WINDOW_SECONDS <= t < (w + 1) * ESTIMATE_WINDOW_SECONDS]
        if not window:
            continue
        mean = statistics.fmean(window)
        levels.append(mean)
        applied = half_step(applied, level_to_cut(mean, ref))
    if not levels:
        return None, None
    return applied, statistics.fmean(levels)


def calculate_cut(blocks, ref, stored):
    """(cut, level): the radio's first CALCULATION, simulated.

    One mean from the 30 s mark to the end of the sample, moved half-way from what the row already holds.  A
    row that holds nothing takes the calculation whole, because there is nothing to lean on."""
    sampled = [db for t, db in blocks if t >= CALCULATE_FROM]
    if not sampled:
        return None, None
    mean = statistics.fmean(sampled)
    target = level_to_cut(mean, ref)
    return (target if stored < 0 else half_step(stored, target)), mean


def measure(url, seconds, keep_from, timeout, stop=None):
    """(blocks, reason): the stream's block levels in the firmware's units, or (None, reason).

    Reads raw s16le PCM rather than asking ffmpeg for a loudness figure, because the firmware's statistic
    (mean of gated block levels) is not any of the ones ffmpeg reports.  `stop` is the run's cancel flag:
    the wait is sliced so a Ctrl-C does not have to outlast the stream."""
    cmd = ["ffmpeg", "-hide_banner", "-nostdin", "-v", "error", "-t", str(seconds), "-i", url,
           "-vn", "-f", "s16le", "-acodec", "pcm_s16le", "-ac", "2", "-ar", str(SAMPLE_RATE), "-"]
    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        return None, "ffmpeg not found"

    scale = 1.0 / (32768.0 * 32768.0)
    blocks = []
    pos_bytes = 0
    deadline = time.monotonic() + timeout
    sink = queue.Queue()
    threading.Thread(target=_pump, args=(proc.stdout, sink), daemon=True).start()
    try:
        while True:
            if stop is not None and stop.is_set():
                proc.kill()                     # the run is being cancelled: abandon this stream at once
                return None, "cancelled"
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None, "timeout after %d s" % timeout
            try:
                chunk = sink.get(timeout=min(remaining, 0.25))
            except queue.Empty:
                continue                        # the deadline is checked at the top of the loop
            if chunk is None:
                break
            if len(chunk) % 2:
                chunk = chunk[:-1]
            count = len(chunk) // 2
            if count:
                squares = sum(v * v for v in struct.unpack("<%dh" % count, chunk))
                seconds_in = pos_bytes / float(BYTES_PER_SECOND)
                if seconds_in >= keep_from and squares:
                    mean_square = (squares / float(count)) * scale
                    if mean_square > 0.0:
                        db = 10.0 * math.log10(mean_square)
                        if db >= GATE_DBFS:         # the firmware's gate, block by block
                            blocks.append((seconds_in, db))
            pos_bytes += len(chunk)
    except KeyboardInterrupt:
        proc.kill()
        proc.wait()
        raise
    finally:
        try:
            proc.stdout.close()
        except Exception:
            pass
        if proc.poll() is None:
            proc.wait(timeout=5)

    if not blocks:
        return None, "no usable audio"
    return blocks, None


def measure_task(args, index, url, stored, stop=None):
    """(index, cut, level, why): measure one station in the current mode, retrying a failure.

    One refused connection is not a dead station - a busy icecast slot or a slow mount costs a minute of
    patience here, and the alternative is a real station labelled (404?).  Only failures retry, so a
    healthy stream still returns on its first attempt."""
    attempt = 0
    while True:
        blocks, why = measure(url, args.seconds, args.keep_from, args.timeout, stop)
        if why is None:
            if args.mode == "calculate":
                cut, level = calculate_cut(blocks, args.ref, stored)
            else:
                cut, level = estimate_cut(blocks, args.ref)
            if cut is not None:
                return index, cut, level, None
            why = "no usable audio"
        if attempt >= args.retries or why == "cancelled":
            if attempt:
                why = "%s (%d tries)" % (why, attempt + 1)
            return index, None, None, why
        delay = RETRY_DELAYS[min(attempt, len(RETRY_DELAYS) - 1)]
        if stop is not None:
            if stop.wait(delay):
                return index, None, None, "%s (%d tries)" % (why, attempt + 1)
        else:
            time.sleep(delay)
        attempt += 1


def probe(url, timeout, stop=None):
    """(kbps, why): what this stream costs, from a two-second listen.

    ffmpeg prints the codec line on stderr - "Audio: mp3, 44100 Hz, stereo, 128 kb/s" - and writes nothing.
    An address that will not answer is worth knowing about in two seconds rather than after a whole sample,
    and the bitrate is what the scheduler budgets with."""
    cmd = ["ffmpeg", "-hide_banner", "-nostdin", "-t", str(PROBE_SECONDS), "-i", url,
           "-vn", "-f", "null", "-"]
    try:
        proc = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=timeout,
                              text=True, encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return None, "ffmpeg not found"
    except subprocess.TimeoutExpired:
        return None, "no answer in %d s" % timeout
    if stop is not None and stop.is_set():
        return None, "cancelled"
    info = proc.stderr or ""
    found = re.search(r"Audio:\s*([A-Za-z0-9_]+)[^\n]*?(\d+)\s*kb/s", info)
    if found:
        return max(8, int(found.group(2))), None
    if re.search(r"Audio:\s*[A-Za-z0-9_]+", info):
        return ASSUMED_KBPS, None            # it answered, but did not say what it costs
    return None, "no stream at that address"


def probe_task(args, index, url, stop=None):
    """Probe with the same patience a measurement gets: one refusal is not a dead station."""
    attempt = 0
    while True:
        kbps, why = probe(url, max(10, PROBE_SECONDS * 6), stop)
        if why is None or attempt >= args.retries or why == "cancelled":
            if why is not None and attempt:
                why = "%s (%d tries)" % (why, attempt + 1)
            return index, kbps, why
        delay = RETRY_DELAYS[min(attempt, len(RETRY_DELAYS) - 1)]
        if stop is not None:
            if stop.wait(delay):
                return index, None, "%s (%d tries)" % (why, attempt + 1)
        else:
            time.sleep(delay)
        attempt += 1


def estimate_waves(kbps_list, budget, jobs):
    """How many rounds the first-fit scheduler will need, assuming equal sample lengths.

    It models the same rule the scheduler uses - take the first stream that fits, at least one whatever it
    costs - so a FLAC-heavy list predicts itself instead of being guessed at."""
    left = list(kbps_list)
    waves = 0
    while left:
        used = 0
        taken = []
        for cost in left:
            if len(taken) >= jobs:
                break
            if not taken or used + cost <= budget:
                used += cost
                taken.append(cost)
        if not taken:                        # impossible: the first rule always takes one
            taken = [left[0]]
        for cost in taken:
            left.remove(cost)
        waves += 1
    return waves


def _download(url, seconds, out, why):
    """Pull as hard as this connection allows for `seconds`, and hand back the byte count.

    The User-Agent matters: a CDN front end answers a bare urllib request with 403 Forbidden, which is the
    same lesson the stream user-agents in config.h record."""
    got = 0
    deadline = time.monotonic() + seconds
    try:
        request = urllib.request.Request(url, headers=SPEED_HEADERS)
        with urllib.request.urlopen(request, timeout=max(2, seconds)) as resp:
            while time.monotonic() < deadline:
                chunk = resp.read(65536)
                if not chunk:
                    break
                got += len(chunk)
    except Exception as exc:
        why.append("%s: %s" % (type(exc).__name__, exc))
    out.append(got)


def speed_test(url_template, streams, total_bytes, seconds):
    """(kbit/s measured, why).  Parallel downloads from a FILE endpoint, never from a station.

    An icecast stream cannot measure a link: it delivers at exactly its encoded rate, so a 128 kbit/s
    station reports 128 kbit/s on a gigabit connection.  The speed test therefore goes somewhere built to
    be downloaded fast, and the stations are left alone."""
    streams = max(1, streams)
    url = url_template % max(100000, total_bytes // streams)
    got = []
    why = []
    began = time.monotonic()
    threads = [threading.Thread(target=_download, args=(url, seconds, got, why), daemon=True)
               for _ in range(streams)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    elapsed = max(0.001, time.monotonic() - began)
    total = sum(got)
    if total <= 0:
        return None, (why[0] if why else "no answer from the speed endpoint")
    # The clock starts before the TLS handshake, so the figure under-reads a fast link slightly.  For a
    # budget that is the safe direction to be wrong in.
    return (total * 8.0 / 1000.0) / elapsed, None


def human_rate(kbps):
    return "%.0f kbit/s" % kbps if kbps < 1000 else "%.1f Mbit/s" % (kbps / 1000.0)


# --- presentation ----------------------------------------------------------------------------------

def describe_mode(mode, ref):
    """Say which stage the number is, because a 30 s estimate and a 5:30 calculation are different things."""
    if mode == "calculate":
        return ("Reference: %.1f dBFS.  Each stream: the mean from %d s to %d s, moved half-way from what the row "
                "holds - the radio's own first calculation, which is the value it records."
                % (ref, int(CALCULATE_FROM), CALCULATE_SECONDS))
    return ("Reference: %.1f dBFS.  Each stream: three 10 s windows, each measured on its own and chained with the "
            "half-step rule - the radio's ESTIMATE stage, which it applies while learning but never records." % ref)


def human_duration(seconds):
    whole = max(1, int(round(seconds)))
    if whole < 60:
        return "%d second%s" % (whole, "" if whole == 1 else "s")
    minutes, secs = divmod(whole, 60)
    if minutes < 60:
        tail = (" %d second%s" % (secs, "" if secs == 1 else "s")) if secs else ""
        return "%d minute%s%s" % (minutes, "" if minutes == 1 else "s", tail)
    hours, minutes = divmod(minutes, 60)
    tail = (" %d minute%s" % (minutes, "" if minutes == 1 else "s")) if minutes else ""
    return "%d hour%s%s" % (hours, "" if hours == 1 else "s", tail)


def ask(prompt):
    try:
        return input(prompt).strip().lower()
    except EOFError:
        return ""


def main(argv=None):
    ap = argparse.ArgumentParser(description="Measure streams and write the dB cut column.")
    ap.add_argument("file", nargs="?", default=DEFAULT_FILE,
                    help="playlist to rewrite (default %s)" % DEFAULT_FILE)
    ap.add_argument("--ref", type=float, default=REFERENCE_DBFS,
                    help="target level in dBFS, the radio's AUTO_DBCUT_REF (default %.0f)" % REFERENCE_DBFS)
    ap.add_argument("--estimate", action="store_true",
                    help="the 30 s estimate stage: what the radio applies while learning, not what it records")
    ap.add_argument("--calculate", action="store_true",
                    help="the 5 min 30 s first calculation: what the radio records at 5:30")
    ap.add_argument("--timeout", type=int, default=0,
                    help="per-stream timeout in seconds (default: the mode's length + 15)")
    ap.add_argument("--retries", type=int, default=4,
                    help="attempts after a failure, with %s second gaps, clamped 0..9 (default 4)"
                         % "/".join(str(d) for d in RETRY_DELAYS))
    ap.add_argument("--max-kbps", default="auto",
                    help="link budget in kbit/s, or 'auto' for half of the measured link speed (default auto)")
    ap.add_argument("--no-speed-test", action="store_true",
                    help="skip the link measurement; the budget is then %d, or whatever --max-kbps says"
                         % DEFAULT_MAX_KBPS)
    ap.add_argument("--speed-url", default=SPEED_URL,
                    help="file endpoint for the measurement, where %%d is the byte count (default Cloudflare)")
    ap.add_argument("--speed-bytes", type=int, default=SPEED_BYTES,
                    help="total bytes to pull across the speed streams (default %d)" % SPEED_BYTES)
    ap.add_argument("--speed-streams", type=int, default=SPEED_STREAMS,
                    help="parallel connections in the measurement (default %d)" % SPEED_STREAMS)
    ap.add_argument("--speed-seconds", type=int, default=SPEED_SECONDS,
                    help="stop the measurement after this many seconds (default %d)" % SPEED_SECONDS)
    ap.add_argument("--jobs", type=int, default=16,
                    help="hard ceiling on streams at once, clamped %d..%d (default 16)" % (JOBS_MIN, JOBS_MAX))
    ap.add_argument("--all", action="store_true", help="re-measure stations that already carry a cut")
    args = ap.parse_args(argv)

    try:                                   # a cp949/cp1252 console must not die on a station name
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    if args.estimate and args.calculate:
        print("--estimate and --calculate are different lengths; pick one.")
        return 2
    args.mode = "calculate" if args.calculate else ("estimate" if args.estimate else None)

    if args.jobs < JOBS_MIN or args.jobs > JOBS_MAX:
        clamped = max(JOBS_MIN, min(JOBS_MAX, args.jobs))
        print("--jobs %d is outside %d..%d, using %d." % (args.jobs, JOBS_MIN, JOBS_MAX, clamped))
        args.jobs = clamped

    if args.speed_streams < 1 or args.speed_streams > 16:
        clamped = max(1, min(16, args.speed_streams))
        print("--speed-streams %d is outside 1..16, using %d." % (args.speed_streams, clamped))
        args.speed_streams = clamped
    if args.speed_seconds < 1 or args.speed_seconds > 30:
        clamped = max(1, min(30, args.speed_seconds))
        print("--speed-seconds %d is outside 1..30, using %d." % (args.speed_seconds, clamped))
        args.speed_seconds = clamped
    if args.speed_bytes < 1000000 or args.speed_bytes > 400000000:
        clamped = max(1000000, min(400000000, args.speed_bytes))
        print("--speed-bytes %d is outside 1..400 MB, using %d." % (args.speed_bytes, clamped))
        args.speed_bytes = clamped

    if args.retries < 0 or args.retries > 9:
        clamped = max(0, min(9, args.retries))
        print("--retries %d is outside 0..9, using %d." % (args.retries, clamped))
        args.retries = clamped

    if not os.path.exists(args.file):
        print("No such file: %s" % args.file)
        return 2

    lines = read_playlist(args.file)
    rows = {}
    todo = []
    for i, line in enumerate(lines):
        parsed = parse_line(line)
        if not parsed:
            continue
        name, url, cut = parsed
        rows[i] = (name, url, cut)
        if cut < 0 or args.all:
            todo.append(i)

    if not todo:
        print("Nothing to do: every row already carries a cut.  Use --all to re-measure them.")
        return 0

    results = {}
    cuts = {}
    failures = {}
    stop = threading.Event()

    # --- what can this connection actually carry? --------------------------------------------------
    # A station cannot answer that - an icecast stream delivers at exactly its encoded rate, so a 128 kbit/s
    # stream reports 128 kbit/s on a gigabit link.  The measurement goes to a plain file endpoint instead, and
    # the budget becomes half of what it managed: the budget exists to stop streams slowing each other down,
    # and being cautious costs a little time and nothing else.
    link_kbps = None
    if not args.no_speed_test:
        print("Measuring the connection (%d streams at once, up to %d s)..."
              % (args.speed_streams, args.speed_seconds), flush=True)
        link_kbps, why = speed_test(args.speed_url, args.speed_streams, args.speed_bytes, args.speed_seconds)
        if link_kbps is None:
            print("Could not measure the connection: %s." % why)
        else:
            print("Connection measured at about %s." % human_rate(link_kbps))

    if str(args.max_kbps).lower() == "auto":
        if link_kbps:
            args.max_kbps = int(link_kbps * BUDGET_SHARE)
            print("Budget: %s, half of the link (%d streams at most)." % (human_rate(args.max_kbps), args.jobs))
        else:
            args.max_kbps = DEFAULT_MAX_KBPS
            print("Budget: %s, the default, since the link could not be measured." % human_rate(args.max_kbps))
    else:
        try:
            args.max_kbps = int(args.max_kbps)
        except ValueError:
            print("--max-kbps %s is neither a number nor 'auto', using %d." % (args.max_kbps, DEFAULT_MAX_KBPS))
            args.max_kbps = DEFAULT_MAX_KBPS
        args.max_kbps = max(64, min(1000000, args.max_kbps))
        if link_kbps and args.max_kbps > link_kbps * BUDGET_SHARE:
            print("Warning: --max-kbps %s is more than half of the %s the link managed - streams may slow each "
                  "other down." % (human_rate(args.max_kbps), human_rate(link_kbps)))

    # --- probe: is the stream alive, and what does it cost? ----------------------------------------
    ahead = max(1, min(len(todo), args.jobs, max(1, args.max_kbps // ASSUMED_KBPS)))
    waves_probe = int(math.ceil(len(todo) / float(ahead)))
    print()
    print("Probing %d stream%s of %d row(s), up to %d at a time, will take %s to %s." % (
        len(todo), "" if len(todo) == 1 else "s", len(rows), ahead,
        human_duration(waves_probe * 3), human_duration(waves_probe * 6)))
    if ask("Are you ready? [y/N]: ") not in ("y", "yes"):
        print("Nothing done.")
        return 0
    began = time.monotonic()
    kbps_of = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=ahead) as pool:
        futures = [pool.submit(probe_task, args, i, rows[i][1], stop) for i in todo]
        for fut in concurrent.futures.as_completed(futures):
            index, kbps, why = fut.result()
            if why is None:
                kbps_of[index] = kbps
            else:
                results[index] = None           # the writer turns this into a (404?) marker
                failures[index] = why
    silent = sum(1 for i in todo if i not in kbps_of)
    print()
    print("Probed in %s: %d stream%s ready%s." % (
        human_duration(max(1, time.monotonic() - began)), len(kbps_of), "" if len(kbps_of) == 1 else "s",
        '; %d did not answer and will be marked "(404?)"' % silent if silent else ""))
    todo = [i for i in todo if i in kbps_of]

    # --- which mode?  The two lengths the radio uses, and what each one costs ------------------------
    if todo:
        # The estimate simulates the scheduler on the bitrates just probed, so a FLAC-heavy list predicts itself
        # instead of being guessed at; the 320 kbit/s assumption only survives for a stream that did not say.
        waves = estimate_waves([kbps_of[i] for i in todo], args.max_kbps, args.jobs)
        at_once = max(1, int(round(len(todo) / float(waves))))

        if args.mode is None:
            print()
            print("Choose a mode to test %d station%s  (%d at a time on a %s budget)."
                  % (len(todo), "" if len(todo) == 1 else "s", at_once, human_rate(args.max_kbps)))
            print()
            for number, mode, length in ((1, "estimate", ESTIMATE_SECONDS), (2, "calculate", CALCULATE_SECONDS)):
                print("%d. %-9s %s each would require about %s." % (
                    number, mode.capitalize() + ":", human_duration(length),
                    human_duration(waves * (length + 2))))
            print()
            choice = ask("Choose the mode [1/2/Enter = Abort]: ")
            if choice not in ("1", "2"):
                print("Nothing done.")
                return 0
            args.mode = "estimate" if choice == "1" else "calculate"

        args.seconds = CALCULATE_SECONDS if args.mode == "calculate" else ESTIMATE_SECONDS
        args.keep_from = 0.0 if args.mode == "estimate" else CALCULATE_FROM - 1.0
        # A timeout shorter than the sample guarantees that EVERY stream fails, which is what a 45 s default
        # did to a long run.  It is derived from the mode unless the caller insists.
        if args.timeout <= args.seconds:
            wanted = args.seconds + 15
            if args.timeout > 0:
                print("--timeout %d is not longer than the %s mode, using %d." % (args.timeout, args.mode, wanted))
            args.timeout = wanted

        run_for = waves * (args.seconds + 2)           # +2 s per stream for connecting
        print()
        line = "%d station%s will be tested for %s each. Total time needed to complete: %s." % (
            len(todo), "" if len(todo) == 1 else "s", human_duration(args.seconds), human_duration(run_for))
        if at_once > 1:
            line += "  (%d at a time on a %s budget)" % (at_once, human_rate(args.max_kbps))
        print(line)
        if max(kbps_of[i] for i in todo) > args.max_kbps:
            print("At least one stream needs more than the whole budget, so it will run on its own.")
        if ask("Are you ready? [y/N]: ") not in ("y", "yes"):
            print("Nothing done.")
            return 0

    print()
    print(describe_mode(args.mode, args.ref))

    total = len(todo)
    done = [0]

    def emit(index, cut, level, why):
        """One full line, always in playlist order, with the share of the run that is finished."""
        results[index] = level
        if why is not None:
            failures[index] = why
            tail = why
        else:
            cuts[index] = cut
            tail = "%6.1f dBFS -> %s %2d dB" % (level, args.mode, cut)
        done[0] += 1
        print("  %-40s %4dk  %-34s [%3d%%]" % (rows[index][0][:40], kbps_of.get(index, 0), tail,
                                               int(round(100.0 * done[0] / total))), flush=True)

    # Start the next stream that FITS rather than the next in the list, so a FLAC blocks another FLAC but not
    # an MP3 behind it, and the queue keeps moving.  Results are still printed in playlist order: a station
    # that finishes early waits for the ones ahead of it, and the percentage says how far along the run is.
    # The stop event exists because a retry can be waiting 30 s: Ctrl-C must not have to sit through it.
    if todo:
        order = {idx: n for n, idx in enumerate(todo)}
        active = {}
        waiting = list(todo)
        buffered = {}
        nxt = [0]
        pool = concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs)

        def flush():
            while nxt[0] in buffered:
                index, cut, level, why = buffered.pop(nxt[0])
                nxt[0] += 1
                emit(index, cut, level, why)

        try:
            while waiting or active:
                filling = True
                while filling and waiting:
                    filling = False
                    used = sum(active.values())
                    for pos, idx in enumerate(waiting):
                        if len(active) >= args.jobs:
                            break
                        cost = kbps_of[idx]
                        if not active or used + cost <= args.max_kbps:
                            active[pool.submit(measure_task, args, idx, rows[idx][1], rows[idx][2], stop)] = cost
                            waiting.pop(pos)
                            filling = True
                            break
                if not active:
                    break                        # nothing running and nothing fits: a budget this small
                got, _ = concurrent.futures.wait(list(active),
                                                 return_when=concurrent.futures.FIRST_COMPLETED)
                for fut in got:
                    active.pop(fut)
                    index, cut, level, why = fut.result()
                    buffered[order[index]] = (index, cut, level, why)
                flush()
        except KeyboardInterrupt:
            stop.set()                          # workers see this in the read loop and between retry gaps
            pool.shutdown(wait=True)
            raise
        finally:
            pool.shutdown(wait=True)

    changed = 0
    for i, (name, url, cut) in rows.items():
        if i in cuts:
            cut = cuts[i]
            if name.endswith(" (404?)"):
                name = name[:-len(" (404?)")]        # that marker was ours, and the stream answered this time
        elif i in results:
            # A timeout is not a dead station, it is a run we did not finish - most likely because --timeout
            # was shorter than --seconds.  Marking those would condemn a whole playlist over a bad flag.
            if not failures.get(i, "").startswith("timeout") and not name.rstrip().endswith("?)"):
                # The test is "already ends with ?)", not "(404?)", because the WebUI Validator writes its own
                # localized message there and every one of them ends that way - one marker per station.
                name = name + " (404?)"
                cut = -1
        # A row that was never tried, or one we could not finish, keeps the value the file already had.
        new = format_line(name, url, cut)
        if new != lines[i].strip():
            lines[i] = new
            changed += 1

    tmp = args.file + ".tmp"
    write_playlist(tmp, lines)
    print("\n%d row(s) changed. New playlist written to %s" % (changed, tmp))

    if ask("Replace %s? [y/N]: " % args.file) in ("y", "yes"):
        os.replace(tmp, args.file)
        print("%s -> %s" % (tmp, args.file))
    else:
        print("Temp file not deleted.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nCancelled: the child process was killed and the playlist is untouched.")
        sys.exit(130)
