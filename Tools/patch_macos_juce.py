#!/usr/bin/env python3
"""Make pinned JUCE 6 window snapshots compatible with macOS 15+ SDKs."""
import sys
from pathlib import Path
for directory in sys.argv[1:]:
    path = Path(directory) / "modules/juce_gui_basics/native/juce_mac_Windowing.mm"
    with path.open(newline="") as f:
        original = f.read()
    newline = "\r\n" if "\r\n" in original else "\n"
    source = original.replace("\r\n", "\n")
    marker = "        // LMN-3: snapshot the local view on SDKs without CGWindowListCreateImage."
    if marker in source:
        continue
    start = source.index("        CGImageRef screenShot = CGWindowListCreateImage", source.index("static Image createNSWindowSnapshot"))
    end = source.index("        return result;", start)
    replacement = """#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
        // LMN-3: snapshot the local view on SDKs without CGWindowListCreateImage.
        NSView* view = [nsWindow contentView];
        NSBitmapImageRep* bitmapRep = [view bitmapImageRepForCachingDisplayInRect: [view bounds]];
        if (bitmapRep == nil)
            return {};
        [view cacheDisplayInRect: [view bounds] toBitmapImageRep: bitmapRep];
        Image result (Image::ARGB, (int) [bitmapRep pixelsWide], (int) [bitmapRep pixelsHigh], true);
        selectImageForDrawing (result);
        [bitmapRep drawInRect: NSMakeRect (0, 0, result.getWidth(), result.getHeight())];
        releaseImageAfterDrawing();
#else
""" + source[start:end] + "#endif\n\n"
    source = source[:start] + replacement + source[end:]
    with path.open("w", newline="") as f:
        f.write(source.replace("\n", newline))
    print("Patched", path)
