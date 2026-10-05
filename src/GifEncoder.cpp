#include "GifEncoder.h"

#include <QFile>
#include <QHash>
#include <QPainter>

#include <algorithm>

namespace GifEncoder
{
namespace
{

struct ColorCount
{
    quint8 r = 0, g = 0, b = 0;
    qint64 count = 0;
};

// A "box" in median-cut color quantization: a contiguous range [begin,end)
// into `colors`, sorted by whichever channel this box was last split on,
// representing a set of similar colors that will collapse to one palette
// entry (their population-weighted average).
struct Box
{
    int begin = 0;
    int end = 0;  // exclusive
};

int channelOf(const ColorCount &c, int channel)
{
    return channel == 0 ? c.r : channel == 1 ? c.g : c.b;
}

int channelRange(const QVector<ColorCount> &colors, int begin, int end, int channel)
{
    int lo = 255, hi = 0;
    for (int i = begin; i < end; ++i) {
        const int v = channelOf(colors[i], channel);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    return hi - lo;
}

// Repeatedly splits the box with the largest weighted color spread (range
// * total pixel population -- a color used by 10000 pixels matters more
// than one used by 1) along whichever channel has the widest range in
// that box, at the point that balances total pixel count on each side.
// Produces up to maxColors boxes; each becomes one palette entry.
QVector<Box> medianCut(QVector<ColorCount> &colors, int maxColors)
{
    QVector<Box> boxes;
    boxes.push_back({0, int(colors.size())});

    while (boxes.size() < maxColors) {
        int bestIdx = -1;
        int bestChannel = 0;
        qint64 bestScore = 0;
        for (int bi = 0; bi < boxes.size(); ++bi) {
            const Box &box = boxes[bi];
            if (box.end - box.begin <= 1)
                continue;
            const int rRange = channelRange(colors, box.begin, box.end, 0);
            const int gRange = channelRange(colors, box.begin, box.end, 1);
            const int bRange = channelRange(colors, box.begin, box.end, 2);
            const int channel = (rRange >= gRange && rRange >= bRange) ? 0 : (gRange >= bRange ? 1 : 2);
            const int range = channel == 0 ? rRange : channel == 1 ? gRange : bRange;
            qint64 population = 0;
            for (int i = box.begin; i < box.end; ++i)
                population += colors[i].count;
            const qint64 score = qint64(range) * population;
            if (score > bestScore) {
                bestScore = score;
                bestIdx = bi;
                bestChannel = channel;
            }
        }
        if (bestIdx < 0)
            break;  // every remaining box is a single color or zero-range -- no useful split left

        const Box box = boxes[bestIdx];
        std::sort(colors.begin() + box.begin, colors.begin() + box.end,
                  [bestChannel](const ColorCount &a, const ColorCount &b) {
                      return channelOf(a, bestChannel) < channelOf(b, bestChannel);
                  });
        // Split at the median *population*, not the median index, so the
        // two halves end up with roughly equal pixel counts even when a
        // handful of colors dominate the box.
        qint64 total = 0;
        for (int i = box.begin; i < box.end; ++i)
            total += colors[i].count;
        const qint64 half = total / 2;
        qint64 running = 0;
        int splitAt = box.begin + 1;
        for (int i = box.begin; i < box.end; ++i) {
            running += colors[i].count;
            if (running >= half) {
                splitAt = i + 1;
                break;
            }
        }
        splitAt = std::clamp(splitAt, box.begin + 1, box.end - 1);

        boxes[bestIdx] = {box.begin, splitAt};
        boxes.push_back({splitAt, box.end});
    }
    return boxes;
}

struct QuantizedFrame
{
    QVector<QRgb> palette;    // <=256 entries
    QVector<quint8> indices;  // width*height, one palette index per pixel
    int width = 0;
    int height = 0;
};

QuantizedFrame quantizeFrame(const QImage &imageIn)
{
    QuantizedFrame out;
    const QImage image = imageIn.convertToFormat(QImage::Format_RGB888);
    out.width = image.width();
    out.height = image.height();

    // Histogram every distinct 24-bit color actually present. A UI
    // screenshot -- flat widget backgrounds plus some antialiased text/
    // icon edges -- typically lands in the low thousands of distinct
    // colors at most, so this stays fast; there's no attempt to bound it
    // further (a photo-heavy capture would just take a bit longer).
    QHash<quint32, qint64> histogram;
    histogram.reserve(4096);
    for (int y = 0; y < out.height; ++y) {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < out.width; ++x) {
            const uchar *px = line + x * 3;
            const quint32 key = (quint32(px[0]) << 16) | (quint32(px[1]) << 8) | quint32(px[2]);
            ++histogram[key];
        }
    }

    QVector<ColorCount> colors;
    colors.reserve(histogram.size());
    for (auto it = histogram.constBegin(); it != histogram.constEnd(); ++it)
        colors.push_back({quint8(it.key() >> 16), quint8(it.key() >> 8), quint8(it.key()), it.value()});

    QHash<quint32, quint8> colorToIndex;
    colorToIndex.reserve(colors.size());

    if (colors.size() <= 256) {
        // Exact palette -- every distinct color gets its own entry, zero
        // quantization error.
        out.palette.reserve(colors.size());
        for (int i = 0; i < colors.size(); ++i) {
            out.palette.push_back(qRgb(colors[i].r, colors[i].g, colors[i].b));
            const quint32 key = (quint32(colors[i].r) << 16) | (quint32(colors[i].g) << 8) | quint32(colors[i].b);
            colorToIndex[key] = quint8(i);
        }
    } else {
        const QVector<Box> boxes = medianCut(colors, 256);
        out.palette.reserve(boxes.size());
        for (int bi = 0; bi < boxes.size(); ++bi) {
            const Box &box = boxes[bi];
            qint64 rSum = 0, gSum = 0, bSum = 0, total = 0;
            for (int i = box.begin; i < box.end; ++i) {
                rSum += qint64(colors[i].r) * colors[i].count;
                gSum += qint64(colors[i].g) * colors[i].count;
                bSum += qint64(colors[i].b) * colors[i].count;
                total += colors[i].count;
            }
            total = std::max<qint64>(total, 1);
            out.palette.push_back(qRgb(quint8(rSum / total), quint8(gSum / total), quint8(bSum / total)));
            for (int i = box.begin; i < box.end; ++i) {
                const quint32 key = (quint32(colors[i].r) << 16) | (quint32(colors[i].g) << 8) | quint32(colors[i].b);
                colorToIndex[key] = quint8(bi);
            }
        }
    }

    out.indices.resize(out.width * out.height);
    int outIdx = 0;
    for (int y = 0; y < out.height; ++y) {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < out.width; ++x) {
            const uchar *px = line + x * 3;
            const quint32 key = (quint32(px[0]) << 16) | (quint32(px[1]) << 8) | quint32(px[2]);
            out.indices[outIdx++] = colorToIndex.value(key, 0);
        }
    }
    return out;
}

// Packs LZW codes into a bitstream, LSB-first within each byte and
// LSB-first within each code -- the bit order GIF decoders expect.
class BitPacker
{
public:
    void writeCode(int code, int bits)
    {
        m_accum |= quint32(code) << m_bitCount;
        m_bitCount += bits;
        while (m_bitCount >= 8) {
            m_bytes.push_back(char(m_accum & 0xFF));
            m_accum >>= 8;
            m_bitCount -= 8;
        }
    }
    QByteArray finish()
    {
        if (m_bitCount > 0) {
            m_bytes.push_back(char(m_accum & 0xFF));
            m_accum = 0;
            m_bitCount = 0;
        }
        return m_bytes;
    }

private:
    QByteArray m_bytes;
    quint32 m_accum = 0;
    int m_bitCount = 0;
};

// Standard GIF-flavored LZW compression of one frame's palette-index
// stream (codes grow from minCodeSize+1 up to 12 bits; the dictionary
// resets via an explicit clear code once it would exceed 4096 entries,
// rather than the implicit behavior some other LZW variants use).
// `minCodeSize` is the same value the Image Descriptor's own
// "LZW Minimum Code Size" byte records right before the compressed data.
QByteArray lzwCompress(const QVector<quint8> &indices, int minCodeSize)
{
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    constexpr int kMaxDictSize = 4096;

    BitPacker packer;
    int codeSize = minCodeSize + 1;
    int nextCode = endCode + 1;
    // Trie-style dictionary keyed on (prefixCode << 8 | nextByte) -> code
    // -- avoids building actual byte-string keys per entry.
    QHash<quint32, int> dict;
    dict.reserve(kMaxDictSize);

    auto resetDict = [&]() {
        dict.clear();
        nextCode = endCode + 1;
        codeSize = minCodeSize + 1;
    };

    packer.writeCode(clearCode, codeSize);

    if (indices.isEmpty()) {
        packer.writeCode(endCode, codeSize);
        return packer.finish();
    }

    int prefix = indices[0];  // a code 0..clearCode-1 directly names a single-index string
    for (int i = 1; i < indices.size(); ++i) {
        const int c = indices[i];
        const quint32 key = (quint32(prefix) << 8) | quint32(c);
        const auto it = dict.constFind(key);
        if (it != dict.constEnd()) {
            prefix = it.value();
            continue;
        }

        packer.writeCode(prefix, codeSize);

        if (nextCode < kMaxDictSize) {
            dict.insert(key, nextCode);
            ++nextCode;
            // GIF's well-known LZW synchronization quirk: this encoder
            // creates one dictionary entry per *emission* (every miss,
            // including the very first one after a clear code), but a
            // standard decoder only starts inserting from its *second*
            // received code onward (nothing to combine with before that --
            // see the "KwKwK" special case decoders need for exactly this
            // reason). That makes this encoder's nextCode run structurally
            // one entry ahead of what a spec-compliant decoder's own
            // next-code counter computes at the same stream position, so
            // the switch to a wider code must be checked against
            // (nextCode - 1) -- equivalently, against (1<<codeSize)
            // instead of (1<<codeSize)-1 -- to land on the same code
            // decoders expect it at. Getting this wrong desyncs the
            // bitstream permanently from that point on (verified against
            // ImageMagick: frames decoded fine up to the first growth,
            // then came back "corrupt" with the naive -1 threshold).
            if (nextCode > (1 << codeSize) && codeSize < 12)
                ++codeSize;
        } else {
            packer.writeCode(clearCode, codeSize);
            resetDict();
        }
        prefix = c;
    }
    packer.writeCode(prefix, codeSize);
    packer.writeCode(endCode, codeSize);
    return packer.finish();
}

void writeDataSubBlocks(QFile &file, const QByteArray &data)
{
    qsizetype offset = 0;
    while (offset < data.size()) {
        const int chunk = int(std::min(qsizetype(255), data.size() - offset));
        const char len = char(chunk);
        file.write(&len, 1);
        file.write(data.constData() + offset, chunk);
        offset += chunk;
    }
    const char zero = 0;
    file.write(&zero, 1);  // block terminator
}

void writeU16LE(QFile &file, quint16 value)
{
    const char bytes[2] = {char(value & 0xFF), char((value >> 8) & 0xFF)};
    file.write(bytes, 2);
}

}  // namespace

bool writeAnimatedGif(const QString &outputPath, const QVector<QImage> &frames, int delayCentiseconds,
                       QString *errorOut, const std::function<bool(int, int)> &progressCallback)
{
    auto fail = [&](const QString &msg, bool removePartialFile = false) {
        if (errorOut)
            *errorOut = msg;
        if (removePartialFile)
            QFile::remove(outputPath);
        return false;
    };

    if (frames.isEmpty())
        return fail(QStringLiteral("フレームがありません"));

    // ManualRecorder re-queries the target window's bounds every frame so
    // it keeps following the window if it moves or gets resized mid-
    // recording -- real recordings of a manually-resized window routinely
    // end up with frames of differing sizes. Rather than reject that, pad
    // every frame onto a shared canvas sized to the largest one seen
    // (anchored top-left, white fill for the margin), so no frame's
    // content is ever cropped.
    int width = 0, height = 0;
    for (const QImage &f : frames) {
        width = std::max(width, f.width());
        height = std::max(height, f.height());
    }
    if (width <= 0 || height <= 0)
        return fail(QStringLiteral("フレームの画像サイズが不正です"));

    QVector<QImage> normalized;
    normalized.reserve(frames.size());
    for (const QImage &f : frames) {
        if (f.width() == width && f.height() == height) {
            normalized.push_back(f);
            continue;
        }
        QImage canvas(width, height, QImage::Format_RGB32);
        canvas.fill(Qt::white);
        QPainter painter(&canvas);
        painter.drawImage(0, 0, f);
        painter.end();
        normalized.push_back(canvas);
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("出力ファイルを開けませんでした: %1").arg(outputPath));

    file.write("GIF89a", 6);
    writeU16LE(file, quint16(width));
    writeU16LE(file, quint16(height));
    // Packed fields: no global color table (each frame below carries its
    // own local one instead), background color index 0, square pixels.
    const char screenDescriptorPacked = 0x00;
    file.write(&screenDescriptorPacked, 1);
    const char zero2[2] = {0, 0};
    file.write(zero2, 2);

    // Netscape 2.0 application extension -- the de facto standard way to
    // mark a GIF as looping; loop count 0 means "loop forever".
    file.write("\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00", 19);

    for (int frameIdx = 0; frameIdx < normalized.size(); ++frameIdx) {
        if (progressCallback && !progressCallback(frameIdx, normalized.size())) {
            file.close();
            return fail(QStringLiteral("キャンセルされました"), /*removePartialFile=*/true);
        }

        const QuantizedFrame q = quantizeFrame(normalized[frameIdx]);

        int minCodeSize = 2;
        while ((1 << minCodeSize) < q.palette.size() && minCodeSize < 8)
            ++minCodeSize;

        // Graphic Control Extension: disposal method 1 ("leave frame as
        // is" -- fine since every frame here is a full, independent
        // window capture), no transparency, this frame's hold time.
        file.write("\x21\xF9\x04", 3);
        const char gcePacked = 0x04;  // disposal method 1, shifted into bits 2-4
        file.write(&gcePacked, 1);
        writeU16LE(file, quint16(delayCentiseconds));
        const char transparentIndex = 0;
        file.write(&transparentIndex, 1);
        const char blockTerm = 0;
        file.write(&blockTerm, 1);

        // Image Descriptor.
        const char imageSep = 0x2C;
        file.write(&imageSep, 1);
        writeU16LE(file, 0);  // left
        writeU16LE(file, 0);  // top
        writeU16LE(file, quint16(width));
        writeU16LE(file, quint16(height));
        const int tableBits = minCodeSize;  // local color table holds 2^tableBits entries
        const char idPacked = char(0x80 | (tableBits - 1));  // local color table present, size field = bits-1
        file.write(&idPacked, 1);

        const int paletteEntries = 1 << tableBits;
        for (int i = 0; i < paletteEntries; ++i) {
            const QRgb c = i < q.palette.size() ? q.palette[i] : qRgb(0, 0, 0);
            const char rgb[3] = {char(qRed(c)), char(qGreen(c)), char(qBlue(c))};
            file.write(rgb, 3);
        }

        const char lzwMinCode = char(minCodeSize);
        file.write(&lzwMinCode, 1);
        writeDataSubBlocks(file, lzwCompress(q.indices, minCodeSize));
    }

    const char trailer = 0x3B;
    file.write(&trailer, 1);
    file.close();
    return true;
}

}  // namespace GifEncoder
