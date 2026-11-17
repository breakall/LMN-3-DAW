#!/usr/bin/env python3
"""Generate an original four-second phrase for the tape editing demo."""
import math
import struct
import sys
import wave
from pathlib import Path
path = Path(sys.argv[1])
path.parent.mkdir(parents=True, exist_ok=True)
if not path.exists():
    notes = [60, 64, 67, 72, 67, 64, 62, 59]
    data = bytearray()
    for i in range(192000):
        t = (i % 24000) / 48000
        frequency = 440 * 2 ** ((notes[i // 24000] - 69) / 12)
        envelope = min(t / .012, 1) * math.exp(-t * 6)
        sample = int(32767 * .22 * envelope *
                     (math.sin(2 * math.pi * frequency * t) +
                      .25 * math.sin(4 * math.pi * frequency * t)))
        data.extend(struct.pack("<hh", sample, sample))
    with wave.open(str(path), "wb") as output:
        output.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        output.writeframes(data)
