#!/usr/bin/env python3
"""Walk an Ogg FLAC capture and report every packet: page, size, and for metadata packets the FLAC
metadata block type and, for a VORBIS_COMMENT (4), the entries it carries.

Written 2026.11.14 to answer one question the device log could not: `deogg: ... meta 16100` says the
comment capture reached its 16 KB cap on distorsion, but not WHAT the oversized packet is.  A VORBIS_COMMENT
that carries a big PICTURE entry and a separate PICTURE block (type 6) need different fixes.

usage: python inspect_ogg_flac.py <capture.bin> [max_packets]
"""

import sys

META_NAMES = {0: "STREAMINFO", 1: "PADDING", 2: "APPLICATION", 3: "SEEKTABLE", 4: "VORBIS_COMMENT",
              5: "CUESHEET", 6: "PICTURE", 127: "INVALID"}


def le32(b, o):
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)


def pages(data):
    """Yield (page_index, header_type, granule, packets, segs) for every page."""
    pos = 0
    idx = 0
    while True:
        p = data.find(b"OggS", pos)
        if p < 0 or p + 27 > len(data):
            return
        hdr = data[p:p + 27]
        htype = hdr[5]
        granule = int.from_bytes(hdr[6:14], "little", signed=False)
        nsegs = hdr[26]
        if p + 27 + nsegs > len(data):
            return
        lacing = data[p + 27:p + 27 + nsegs]
        body = p + 27 + nsegs
        blen = sum(lacing)
        if body + blen > len(data):
            return
        yield idx, htype, granule, lacing, data[body:body + blen]
        pos = body + blen
        idx += 1


def packets(data):
    """Merge lacing into packets, carrying across pages.  Yields (page, end_page, size, bytes, continued)."""
    carry = b""
    start_page = 0
    for idx, htype, _granule, lacing, body in pages(data):
        off = 0
        if htype & 0x01:
            if not carry:
                start_page = idx          # a continuation with nothing carried: report it as such
        for seg_i, seg in enumerate(lacing):
            carry += body[off:off + seg]
            off += seg
            if seg < 255:
                yield start_page, idx, len(carry), carry, (start_page != idx)
                carry = b""
                start_page = idx
        if lacing and lacing[-1] == 255:
            pass                          # the packet continues: start_page stays
        elif carry:
            # a page whose last lacing was < 255 always closed its packet; nothing can be left
            carry = b""
            start_page = idx


def describe(pkt, size):
    if size < 6:
        return "short packet"
    if pkt[0] == 0x7F and pkt[1:5] == b"FLAC":
        major, minor, nhead = pkt[5], pkt[6], pkt[7] | (pkt[8] << 8)
        if len(pkt) < 13:
            return "FLAC mapping header only (%d.%d, %d headers)" % (major, minor, nhead)
        return "FLAC mapping %d.%d, %d headers; then " % (major, minor, nhead) + block(pkt, 9)
    if pkt[0] == 0xFF and (pkt[1] & 0xFC) == 0xF8:
        return "audio frame"
    return block(pkt, 0)


def block(pkt, o):
    if o + 4 > len(pkt):
        return "truncated block header"
    last = (pkt[o] >> 7) & 1
    btype = pkt[o] & 0x7F
    blen = (pkt[o + 1] << 16) | (pkt[o + 2] << 8) | pkt[o + 3]
    name = META_NAMES.get(btype, "type %d" % btype)
    txt = "%s%s block, %d bytes declared" % (name, " (last)" if last else "", blen)
    if btype == 4:
        txt += "; " + comment(pkt, o + 4, len(pkt))
    elif btype == 6:
        txt += "; " + picture(pkt, o + 4, len(pkt))
    return txt


def comment(pkt, o, total):
    end = total
    if o + 4 > end:
        return "truncated (no vendor length)"
    vlen = le32(pkt, o)
    o += 4
    if o + vlen > end:
        return "vendor truncated at %d bytes" % vlen
    vendor = pkt[o:o + vlen].decode("utf-8", "replace")
    o += vlen
    if o + 4 > end:
        return "vendor %r, count truncated" % vendor
    count = le32(pkt, o)
    o += 4
    got = []
    for _ in range(count):
        if o + 4 > end:
            got.append("<list truncated>")
            break
        sl = le32(pkt, o)
        o += 4
        if o + sl > end:
            got.append("<entry of %d bytes truncated>" % sl)
            break
        s = pkt[o:o + sl].decode("utf-8", "replace")
        o += sl
        got.append(s)
    return "vendor %r, %d entries: %s" % (vendor, count, "; ".join(got[:12]) if got else "(none)")


def picture(pkt, o, total):
    try:
        mime_len = (pkt[o] << 24) | (pkt[o + 1] << 16) | (pkt[o + 2] << 8) | pkt[o + 3]
        mime = pkt[o + 4:o + 4 + mime_len].decode("ascii", "replace")
        q = o + 4 + mime_len
        dlen = (pkt[q] << 24) | (pkt[q + 1] << 16) | (pkt[q + 2] << 8) | pkt[q + 3]
        mime += " / " + pkt[q + 4:q + 4 + dlen].decode("utf-8", "replace")
        q += 4 + dlen
        w = (pkt[q] << 24) | (pkt[q + 1] << 16) | (pkt[q + 2] << 8) | pkt[q + 3]
        h = (pkt[q + 4] << 24) | (pkt[q + 5] << 16) | (pkt[q + 6] << 8) | pkt[q + 7]
        return "picture %s %dx%d" % (mime, w, h)
    except Exception:
        return "picture (unparsed)"


def main():
    path = sys.argv[1]
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 10 ** 9
    data = open(path, "rb").read()
    print("%s: %d bytes" % (path, len(data)))
    n_audio = n_meta = 0
    for i, (p0, p1, size, pkt, cont) in enumerate(packets(data)):
        if i >= limit:
            print("...stopped at %d packets" % i)
            break
        kind = describe(pkt, size)
        tag = ""
        if kind.startswith("audio frame"):
            n_audio += 1
            if n_audio > 3:
                continue
            kind = "audio frame" + (" (first 3 shown)" if n_audio == 3 else "")
        else:
            n_meta += 1
            tag = "  <<< METADATA"
        span = "page %d" % p0 if p0 == p1 else "pages %d-%d" % (p0, p1)
        print("#%-4d %-14s %6d bytes%s%-5s %s" % (i, span, size, " cont" if cont else "", "", kind) + tag)
    print("audio packets: %d, metadata packets: %d" % (n_audio, n_meta))


if __name__ == "__main__":
    main()
