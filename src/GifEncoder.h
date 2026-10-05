#pragma once

#include <QImage>
#include <QString>
#include <QVector>

#include <functional>

// Self-contained animated GIF (GIF89a) writer: median-cut color
// quantization down to <=256 colors per frame (each frame gets its own
// local color table -- see writeAnimatedGif()'s comment for why) plus the
// standard GIF LZW compressor, implemented from scratch with no
// dependency on any external library or tool. Unlike the optional
// ffmpeg-based path in AnimationExportDialog (MP4/WebM), this always
// works, which is why it's the one format AnimationExportDialog never
// disables.
namespace GifEncoder
{
// Writes `frames` (in order) as a looping animated GIF to `outputPath`,
// each frame held on screen for `delayCentiseconds` (GIF's own unit,
// 1/100 second). Frames may differ in size (e.g. a ManualRecorder
// recording of a window the user resized mid-capture) -- each is padded
// onto a shared canvas sized to the largest frame seen, anchored at its
// top-left corner with a white margin, so nothing is ever cropped.
// Returns false (and sets *errorOut, if given) on failure: empty input,
// the output file couldn't be opened for writing, or `progressCallback`
// asked to stop (see below) -- any partially-written output file is
// removed in that case.
//
// `progressCallback`, if given, is invoked once before encoding each
// frame as (frameIndexFrom0, totalFrames); encoding continues only while
// it returns true, so a caller driving this from the UI thread can call
// QCoreApplication::processEvents() from inside it to update a progress
// bar and notice a Cancel click, then return false to abort.
bool writeAnimatedGif(const QString &outputPath, const QVector<QImage> &frames, int delayCentiseconds,
                       QString *errorOut = nullptr,
                       const std::function<bool(int, int)> &progressCallback = {});
}  // namespace GifEncoder
