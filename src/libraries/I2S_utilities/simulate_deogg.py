#!/usr/bin/env python3
"""Replay FLACDeOgg() over a capture, byte for byte, and report every metadata classification.

Re-synced with flac_decoder.cpp on 2026.11.16: the classification now refuses Vorbis and Opus headers by
name (the crash of 2026.11.15) and the comment parse checks the vendor length against what is left of the
block, as FLACOfferCommentPacket() does.  Mirrored: capture-time emission, per-run classification, the
pending capture across page boundaries, the narrowed tail-run rule, and the 16 KB capture cap.

The C++ is the truth.  Any divergence here is a bug in this script, not a finding about the device.

usage: python simulate_deogg.py <capture.bin> [max_pages]
"""

import sys

META_NAMES = {0: "STREAMINFO", 1: "PADDING", 2: "APPLICATION", 3: "SEEKTABLE", 4: "VORBIS_COMMENT",
              5: "CUESHEET", 6: "PICTURE", 127: "INVALID"}


def le32(b, o):
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)


def is_sync(p, o):
    return p[o] == 0xFF and (p[o + 1] & 0xFC) == 0xF8


def looks_like_metadata(p):
    """The rule FLACDeOgg now uses: only a mapping header or a metadata block header is metadata, and a
    Vorbis or Opus header is refused BY NAME because it also satisfies the block-header test - capturing one
    and parsing it as FLAC is what produced the LoadProhibited.  Everything else is AUDIO: a false AUDIO is
    harmless, a false metadata deletes a frame from the ring."""
    if len(p) >= 5 and p[0] == 0x7F and p[1:5] == b"FLAC":
        return True
    if len(p) >= 7 and p[0] in (0x01, 0x03, 0x05) and p[1:7] == b"vorbis":
        return False
    if len(p) >= 8 and p[:8] in (b"OpusHead", b"OpusTags"):
        return False
    if len(p) < 4:
        return False
    if (p[0] & 0x7F) > 6:
        return False
    return ((p[1] << 16) | (p[2] << 8) | p[3]) != 0


def main():
    path = sys.argv[1]
    max_pages = int(sys.argv[2]) if len(sys.argv) > 2 else 10 ** 9
    data = open(path, "rb").read()

    strip = False          # page headers are being dropped
    pending = False        # a comment capture continues on the next page
    meta = b""             # the capture
    overrun = False
    reassembled = 0
    captured_total = 0
    emitted_total = 0
    title_hits = 0
    preflip_offers = 0
    preflip = b""          # the pre-flip accumulator, which LIVES ACROSS PAGES

    pos = 0
    page = 0
    while pos + 27 <= len(data) and page < max_pages:
        if data[pos:pos + 4] != b"OggS":
            print("! lost sync at page %d (offset %d)" % (page, pos))
            break
        hdr = data[pos:pos + 27]
        htype = hdr[5]
        nsegs = hdr[26]
        lacing = data[pos + 27:pos + 27 + nsegs]
        body = data[pos + 27 + nsegs:pos + 27 + nsegs + sum(lacing)]
        pos = pos + 27 + nsegs + sum(lacing)

        # merge lacing into per-packet runs, exactly as FLACDeOgg does
        runs = []
        run = 0
        for seg in lacing:
            run += seg
            if seg < 255:
                runs.append(run)
                run = 0
        if run:
            runs.append(run)
        tail = (len(runs) - 1) if (lacing and lacing[-1] == 255 and runs) else None
        cont_page = bool(htype & 0x01)

        if pending and not cont_page:
            pending = False
            meta = b""
            overrun = False
        if not pending:
            meta = b""
            overrun = False

        if not strip:
            # Mirrors the pre-flip capture: every byte is still emitted (so `emitted` must be identical to the
            # previous behaviour), each packet is accumulated across page boundaries, and a COMPLETE packet that
            # looks like metadata is offered.  `preflip` lives across pages; it is only cleared when a packet closes.
            off = 0
            for idx, rlen in enumerate(runs):
                seg = body[off:off + rlen]
                off += rlen
                if len(seg) >= 2 and is_sync(seg, 0):
                    strip = True
                    preflip = b""
                    break
                preflip += seg
                open_at_end = (idx == len(runs) - 1) and bool(lacing) and lacing[-1] == 255
                if open_at_end:
                    continue                       # the next page continues this packet
                if looks_like_metadata(preflip):
                    preflip_offers += 1
                    print("page %-4d PRE-FLIP offered %d bytes: %s"
                          % (page, len(preflip),
                             guess_title(preflip, 13 if preflip[:1] == b"\x7f" else 4)))
                preflip = b""
            emitted_total += len(body)
            page += 1
            continue

        # the first run of a continuation page is emitted through, unless it is a capture being continued
        kind0 = "AUDIO" if (cont_page and not pending) else ("CANDIDATE" if (cont_page and pending) else None)
        off = 0
        for idx, rlen in enumerate(runs):
            seg = body[off:off + rlen]
            off += rlen
            if idx == 0 and kind0:
                kind = kind0
            elif idx == 0:
                kind = None
            elif idx == tail and idx == 0 and cont_page:
                kind = "AUDIO"
            else:
                kind = None
            if kind is None:
                if len(seg) >= 2 and is_sync(seg, 0):
                    kind = "AUDIO"
                elif len(seg) >= 4 and looks_like_metadata(seg):
                    kind = "CANDIDATE"
                else:
                    kind = "AUDIO"

            note = ""
            if kind == "AUDIO":
                emitted_total += rlen
            else:
                room = 16384 - len(meta)
                take = min(rlen, room)
                meta += seg[:take]
                if take < rlen:
                    overrun = True
                captured_total += take
                # does the packet look like a comment?
                note = " first4=%s" % seg[:4].hex()
                if len(seg) >= 13:
                    o = 9 if (seg[0] == 0x7F and seg[1:5] == b"FLAC") else 0
                    if o + 4 <= len(seg):
                        btype = seg[o] & 0x7F
                        note += " block=%s" % META_NAMES.get(btype, btype)

            if idx == 0 or rlen > 2000 or kind != "AUDIO":
                print("page %-4d ht=%d cont=%d runs=%-2d run%-2d len=%-6d %-9s%s"
                      % (page, htype, cont_page, len(runs), idx, rlen, kind, note))

            open_at_end = (idx == tail)
            if open_at_end:
                if kind == "CANDIDATE":
                    pending = True
                else:
                    pending = False
            else:
                if kind == "CANDIDATE":
                    # the capture closes here: this is where FLACOfferCommentPacket() is called
                    o = 9 if (meta[:1] == b"\x7f" and meta[1:5] == b"FLAC") else 0
                    if not overrun and len(meta) >= o + 4:
                        btype = meta[o] & 0x7F
                        blen = (meta[o + 1] << 16) | (meta[o + 2] << 8) | meta[o + 3]
                        if btype == 4:
                            title_hits += 1
                            print("    -> COMMENT offered %d bytes, declares %d: %s"
                                  % (len(meta), blen, guess_title(meta, o + 4)))
                        else:
                            print("    -> offered %d bytes but block type %s"
                                  % (len(meta), META_NAMES.get(btype, btype)))
                    elif overrun:
                        print("    -> capture OVERRAN (%d+ bytes), never offered" % len(meta))
                    if pending:
                        reassembled += 1
                    pending = False
                    meta = b""
                    overrun = False
        page += 1

    print("pages %d, emitted %d, captured %d, comments offered %d (pre-flip %d), reassembled %d"
          % (page, emitted_total, captured_total, title_hits, preflip_offers, reassembled))


def guess_title(p, o):
    """The comment body at `o`, read the way FLACOfferCommentPacket() reads it: vendor length first and
    bounded by what is left of the block, then the entries, each bounded the same way.  The bound is not
    decoration - trusting that length is what turned Vorbis text into a pointer offset."""
    try:
        end = len(p)
        if o + 4 > end:
            return "(no body)"
        vlen = le32(p, o)
        if vlen > end - o - 4:
            return "(vendor length %d exceeds the %d left - not a FLAC comment)" % (vlen, end - o - 4)
        o += 4 + vlen
        if o + 4 > end:
            return "(truncated after the vendor string)"
        count = le32(p, o); o += 4
        out = []
        for _ in range(count):
            if o + 4 > end:
                break
            sl = le32(p, o); o += 4
            if sl > end - o:
                break
            out.append(p[o:o + sl].decode("utf-8", "replace"))
            o += sl
        return "; ".join(out[:4]) if out else "(no entries)"
    except Exception:
        return "(unparsed)"


if __name__ == "__main__":
    main()
