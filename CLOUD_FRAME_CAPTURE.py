"""Build fresh, small water-camera JPEGs for GitHub Pages without a home PC.

Run this on GitHub Actions every five minutes. A failed source writes a zero
timestamp and no image. Devices must reject stale/failed timestamps.
"""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import shutil
import subprocess
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

SOURCES = (
    "https://s90.ipcamlive.com/streams/5asxkbexh0py8flva/stream.m3u8",
    "http://101.109.253.60:8999/playlist.m3u8",
)
REFERERS = ("https://www.ipcamlive.com/68c13eaeec104", "http://101.109.253.60:8999/")
MAX_JPEG = 60_000


def ffmpeg_binary():
    binary = shutil.which("ffmpeg")
    if binary:
        return binary
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


def inspect_playlist(camera):
    """Log a small public playlist sample after FFmpeg fails (no secrets)."""
    request = Request(SOURCES[camera], headers={
        "User-Agent": "Mozilla/5.0", "Referer": REFERERS[camera],
    })
    try:
        with urlopen(request, timeout=8) as response:
            sample = response.read(160)
            print(f"camera {camera+1}: playlist HTTP={response.status} "
                  f"type={response.headers.get('Content-Type', '?')} "
                  f"bytes_read={len(sample)} starts_m3u={sample.startswith(b'#EXTM3U')}",
                  flush=True)
    except (HTTPError, URLError, TimeoutError, OSError) as error:
        print(f"camera {camera+1}: playlist GET failed: "
              f"{type(error).__name__}: {error}", flush=True)


def one_frame(exe, camera, synthetic=False):
    cmd = [exe, "-hide_banner", "-nostdin", "-loglevel", "warning"]
    if synthetic:
        cmd += ["-f", "lavfi", "-i", f"testsrc2=size=640x360:rate=1:duration=1"]
    else:
        cmd += ["-rw_timeout", "8000000", "-user_agent", "Mozilla/5.0",
                "-referer", REFERERS[camera], "-i", SOURCES[camera]]
    cmd += ["-an", "-frames:v", "1", "-q:v", "5", "-f", "image2pipe",
            "-vcodec", "mjpeg", "pipe:1"]
    try:
        result = subprocess.run(cmd, capture_output=True, timeout=35)
    except (subprocess.TimeoutExpired, OSError) as error:
        print(f"camera {camera+1}: cannot capture: "
              f"{type(error).__name__}: {error}", flush=True)
        if not synthetic:
            inspect_playlist(camera)
        return None
    if result.returncode or not result.stdout.startswith(b"\xff\xd8"):
        details = result.stderr.decode("utf-8", "replace").strip()[-2000:]
        print(f"camera {camera+1}: capture failed rc={result.returncode} "
              f"stdout_bytes={len(result.stdout)} "
              f"header={result.stdout[:12].hex()} "
              f"ffmpeg={exe} stderr={details or '(empty)'}", flush=True)
        if not synthetic:
            inspect_playlist(camera)
        return None
    return result.stdout


def crop_and_resize(exe, frame, zoom):
    crop = "" if zoom == 1 else (
        f"crop=iw/{zoom}:ih/{zoom}:(iw-ow)/2:(ih-oh)/2,")
    vf = crop + "scale=320:128:force_original_aspect_ratio=increase,crop=320:128"
    cmd = [exe, "-hide_banner", "-nostdin", "-loglevel", "error", "-i", "pipe:0",
           "-an", "-frames:v", "1", "-vf", vf, "-q:v", "8",
           "-f", "image2pipe", "-vcodec", "mjpeg", "pipe:1"]
    try:
        result = subprocess.run(cmd, input=frame, capture_output=True, timeout=15)
    except (subprocess.TimeoutExpired, OSError):
        return None
    data = result.stdout
    if result.returncode or len(data) > MAX_JPEG or not (
            data.startswith(b"\xff\xd8") and data.endswith(b"\xff\xd9")):
        return None
    return data


def build(output, synthetic=False):
    exe = ffmpeg_binary()
    output.mkdir(parents=True, exist_ok=True)
    for camera in range(2):
        # Remove old images before capture, so failed runs cannot publish old frames.
        paths = [output / f"{camera+1}-z{zoom}.jpg" for zoom in range(1, 5)]
        for path in paths:
            path.unlink(missing_ok=True)
        (output / f"{camera+1}.meta").write_text("0\n", encoding="ascii")
        frame = one_frame(exe, camera, synthetic=synthetic)
        images = [crop_and_resize(exe, frame, zoom) if frame else None for zoom in range(1, 5)]
        if not all(images):
            print(f"camera {camera+1}: unavailable; meta=0", flush=True)
            continue
        for path, data in zip(paths, images):
            path.write_bytes(data)
        timestamp = int(time.time())
        (output / f"{camera+1}.meta").write_text(f"{timestamp}\n", encoding="ascii")
        print(f"camera {camera+1}: {datetime.fromtimestamp(timestamp,timezone.utc).isoformat()} "
              f"JPEG {[len(data) for data in images]} bytes", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--self-test", action="store_true", help="Use synthetic pictures, never publish")
    args = parser.parse_args()
    build(args.output, synthetic=args.self_test)
