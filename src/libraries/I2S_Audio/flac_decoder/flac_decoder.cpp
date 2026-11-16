/*
 * flac_decoder.cpp
 * Java source code from https://www.nayuki.io/page/simple-flac-implementation
 * adapted to ESP32
 *
 * Created on: Jul 03,2020
 * Updated on: Mar 29,2025
 *
 * Author: Wolle
 *
 */
#include "flac_decoder.h"
#include "vector"
#include <stdio.h>          // snprintf, for FLACDeOggDebug()
using namespace std;

#if FLAC_PROFILE
// Cost of the two halves of a FLAC frame, in MICROSECONDS, reported once per i2s window.  A frame is ~90 ms of CPU,
// so micros() is easily accurate enough - and unlike the cycle counter it replaced, it cannot be wrong about what
// it is measuring.
static uint32_t s_pfResidUs = 0;
static uint32_t s_pfLpcUs   = 0;
static uint32_t s_pfFrames  = 0;
void FLACProfileGet(uint32_t* residUs, uint32_t* lpcUs, uint32_t* frames){
    *residUs = s_pfResidUs; *lpcUs = s_pfLpcUs; *frames = s_pfFrames;
    s_pfResidUs = 0; s_pfLpcUs = 0; s_pfFrames = 0;
}
// A stream that is switched away from before its report fires leaves these counters standing, and the next stream's
// report prints them as its own - measured 2026.11.14, an MP3 window reporting "block 4608, 0 ch, 0 bit, 0 Hz" from
// the FLAC stream before it.  Clearing on free is enough: the report is guarded by `if(pf)`.
void FLACProfileReset(void){
    s_pfResidUs = 0; s_pfLpcUs = 0; s_pfFrames = 0;
}
#endif

FLACFrameHeader_t*   FLACFrameHeader;
FLACMetadataBlock_t* FLACMetadataBlock;

vector<uint32_t> s_flacSegmTableVec;
vector<int32_t>  coefs;
vector<uint32_t> s_flacBlockPicItem;
uint64_t         s_flac_bitBuffer = 0;
uint32_t         s_flacBitrate = 0;
uint32_t         s_flacBlockPicLenUntilFrameEnd = 0;
uint32_t         s_flacCurrentFilePos = 0;
uint32_t         s_flacBlockPicPos = 0;
uint32_t         s_flacBlockPicLen = 0;
uint32_t         s_flacAudioDataStart = 0;
int32_t          s_flacRemainBlockPicLen = 0;
const uint16_t   s_flacOutBuffSize = 2048;
uint16_t         s_numOfOutSamples = 0;
uint16_t         s_flacValidSamples = 0;
uint16_t         s_rIndex = 0;
uint16_t         s_offset = 0;
uint8_t          s_flacStatus = 0;
uint8_t*         s_flacInptr;
float            s_flacCompressionRatio = 0;
uint8_t          s_flacBitBufferLen = 0;
bool             s_f_flacParseOgg = false;
bool             s_f_bitReaderError = false;
uint8_t          s_flac_pageSegments = 0;
char*            s_flacStreamTitle = NULL;
char*            s_flacVendorString = NULL;
bool             s_f_flacNewStreamtitle = false;
bool             s_f_flacFirstCall = true;
bool             s_f_oggWrapper = false;
bool             s_f_lastMetaDataBlock = false;
bool             s_f_flacNewMetadataBlockPicture = false;
uint8_t          s_flacPageNr = 0;
int32_t**        s_samplesBuffer = NULL;
uint16_t         s_maxBlocksize = MAX_BLOCKSIZE;
int32_t          s_nBytes = 0;
// The Ogg carry.  A page can cut a FLAC frame in half (Icecast writes ~64 KB pages and does not align packets),
// and the two halves are separated by the next page's header, so the frame cannot be decoded from the ring.  The
// tail goes here and the head is appended when the next page arrives; FLACDecodeNative() then always sees one
// contiguous, whole frame.  Allocated once with the sample buffers, from PSRAM, and never per stream.
#define  FLAC_CARRY_SIZE (MAX_BLOCKSIZE * 2)   // must exceed MAX_BLOCKSIZE: a whole frame has to fit with room to resume
uint8_t*         s_flacCarry = NULL;
uint32_t         s_flacCarryLen = 0;      // bytes accumulated for the frame in progress
uint32_t         s_flacCarryRead = 0;     // bytes FLACDecodeNative() has consumed from it (its s_nBytes equivalent)
// "The carry has bytes" and "the packet in it is whole" are different statements, and conflating them hands
// FLACDecodeNative() a frame fragment with no head: it reads past the end and the caller sees DECODER ASYNC about
// 1.3 KB before every page boundary.  Only the run that ENDS the packet sets this.
bool             s_f_flacCarryComplete = false;
bool             s_f_flacRunEndsPacket = true;      // the run about to be handed out finishes its packet
bool             s_f_flacPageEndsMidPacket = false; // this page's final lacing value is 255: the last run continues
bool             s_f_flacPageStartsContinued = false; // headerType bit 0: the next run continues the previous packet

//----------------------------------------------------------------------------------------------------------------------
//          O G G   D E - O G G E R
//----------------------------------------------------------------------------------------------------------------------
// The contract is in flac_decoder.h.  One connection runs at a time, so this state is per stream and is cleared by
// FLACDeOggReset() when a connection is opened.
enum deOggState_e : uint8_t { DEOGG_HEADER, DEOGG_LACING, DEOGG_PAYLOAD, DEOGG_PASS };
static deOggState_e s_dgState      = DEOGG_HEADER;
static uint8_t  s_dgHdr[27];        // capture: the page header, which may straddle two reads
static uint8_t  s_dgHdrN           = 0;
static uint8_t  s_dgLacing[255];    // capture: the lacing table
static uint8_t  s_dgLacingN        = 0;
static uint16_t s_dgRuns[255];      // per-packet lengths of the current page, merged from the lacing values
static uint16_t s_dgRunCount       = 0;
static uint16_t s_dgRunIdx         = 0;
static uint16_t s_dgRunLeft        = 0;
static uint16_t s_dgTailRun        = 0xFFFF; // the one run that is a packet's TAIL, not a packet start
static uint32_t s_dgPayLeft        = 0;
static bool     s_dgStrip          = false;  // page headers are being dropped
static bool     s_dgFirstPage      = true;
static uint8_t  s_dgPeek[2];                 // the first two bytes of the packet being examined
static uint8_t  s_dgPeekN          = 0;

// Mid-stream metadata: an Icecast Ogg stream carries a title change as a NEW COMMENT PACKET.  A packet that looks like
// metadata is captured here instead of emitted, and handed to the comment parser when its run ends.
// 16 KB covers an ordinary comment block; the parser tolerates truncation, so this is a memory decision.
#define DEOGG_META_MAX 16384
static uint8_t*  s_dgMeta          = NULL;   // PSRAM, allocated at boot
static uint32_t  s_dgMetaLen       = 0;
static bool      s_dgMetaOverrun   = false;
// Comment captures offered to the parser, reported in the reader's `deogg:` line.
static uint32_t  s_dgCommentHits   = 0;
// The extracted title lives here, not in s_flacStreamTitle: that one is freed by FLACDecoder_FreeBuffers() during
// setDecoderItems, which runs after the connect-time capture.  Audio reads this via FLACDeOggTitle().
static char      s_dgTitle[192];
static bool      s_dgTitleNew      = false;
enum deOggMeta_e : uint8_t { DEOGG_META_UNKNOWN = 0, DEOGG_META_AUDIO, DEOGG_META_CANDIDATE };
static deOggMeta_e s_dgMetaKind    = DEOGG_META_UNKNOWN;
// A capture continues on the next page: its continuation is appended rather than emitted as audio.
static bool      s_dgMetaPending   = false;
// headerType bit 0 of the current page, latched at lacing time for the payload loop.
static bool      s_dgContPage      = false;
// Reassemblies since the last reset, reported in the reader's `deogg:` line.
static uint32_t  s_dgReassembled   = 0;

/* Does this run start an Ogg FLAC metadata packet?  Only the mapping header (0x7F 'F' 'L' 'A' 'C') or a block header
   (type 0..6 with a non-zero length) qualify; everything else is AUDIO, because a false metadata deletes a whole frame
   from the ring while a false AUDIO only costs one resync.  Vorbis and Opus are refused by NAME: their headers start
   0x01/0x03/0x05 + "vorbis" or "Opus...", which reads as a valid block header and sent a Vorbis stream into the FLAC
   comment parser - a LoadProhibited on every preview of that station. */
static bool dgLooksLikeMetadata(const uint8_t* p, uint32_t n){
    if(n >= 5 && p[0] == 0x7F && p[1] == 'F' && p[2] == 'L' && p[3] == 'A' && p[4] == 'C') return true;
    if(n >= 7 && (p[0] == 0x01 || p[0] == 0x03 || p[0] == 0x05) && !memcmp(p + 1, "vorbis", 6)) return false;
    if(n >= 8 && (!memcmp(p, "OpusHead", 8) || !memcmp(p, "OpusTags", 8))) return false;
    if(n < 4) return false;
    if((p[0] & 0x7F) > 6) return false;                        // 7..126 reserved, 127 invalid
    return (((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]) != 0;
}

void FLACDeOggReset(void){
    s_dgState = DEOGG_HEADER; s_dgHdrN = 0; s_dgLacingN = 0; s_dgRunCount = 0; s_dgRunIdx = 0;
    s_dgRunLeft = 0; s_dgPayLeft = 0; s_dgStrip = false; s_dgFirstPage = true; s_dgPeekN = 0;
    s_dgMetaLen = 0; s_dgMetaOverrun = false; s_dgMetaKind = DEOGG_META_UNKNOWN;
    s_dgMetaPending = false; s_dgContPage = false; s_dgReassembled = 0; s_dgCommentHits = 0;
    s_dgTitle[0] = 0; s_dgTitleNew = false;
}

// The title a captured comment packet produced, or NULL.  Reading it consumes it.
const char* FLACDeOggTitle(void){
    if(!s_dgTitleNew) return NULL;
    s_dgTitleNew = false;
    return s_dgTitle;
}

bool FLACDeOggStripping(void){ return s_dgStrip; }

// Take the comment capture before the ring claims the largest contiguous block; called from Audio::initInBuff().
void FLACDeOggReserve(void){
    if(!s_dgMeta) s_dgMeta = (uint8_t*)ps_malloc(DEOGG_META_MAX);
}

// One line of state for the reader's diagnostic.  `state` is HEADER(0) LACING(1) PAYLOAD(2) PASS(3); PAY is the
// payload bytes still owed to this page, RUN is the index/count/remaining of its merged packet runs.  PASS is the
// important one: it is permanent, it is entered whenever the 27-byte capture does not say "OggS", and from then on
// the filter copies everything verbatim - page headers included - into the decoder.
void FLACDeOggDebug(char* out, size_t room){
    if(!out || !room) return;
    snprintf(out, room, "state %u strip %u pay %u run %u/%u left %u hdr %u lac %u meta %u%s cm %u rj %u%s",
             (unsigned)s_dgState, (unsigned)s_dgStrip, (unsigned)s_dgPayLeft,
             (unsigned)s_dgRunIdx, (unsigned)s_dgRunCount, (unsigned)s_dgRunLeft,
             (unsigned)s_dgHdrN, (unsigned)s_dgLacingN, (unsigned)s_dgMetaLen,
             s_dgMetaOverrun ? "!" : "", (unsigned)s_dgCommentHits, (unsigned)s_dgReassembled,
             s_dgMetaPending ? "c" : "");
}

uint32_t FLACDeOggCommentCount(void){ return s_dgCommentHits; }

// Does `s` (of length sl) start with `KEY=`?  Written out rather than using strncasecmp, which is not in every
// toolchain's <string.h> for the xtensa build.
static bool dgKeyIs(const char* s, uint32_t sl, const char* key){
    const size_t kl = strlen(key);
    if(sl < kl + 1) return false;
    for(size_t k = 0; k < kl; k++){
        char c = s[k];
        if(c >= 'a' && c <= 'z') c = (char)(c - 32);
        if(c != key[k]) return false;
    }
    return s[kl] == '=';
}
static uint32_t dgLe32(const uint8_t* p){
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
// A mid-stream metadata packet: 0x7F 'F' 'L' 'A' 'C', major, minor, header count (2), then a metadata block - or, for
// a plain FLAC stream, the block with no mapping header at all.  Only a VORBIS_COMMENT (type 4) carries a title, and
// only its text fields are taken: the picture bookkeeping in parseMetaDataBlockHeader() is expressed in ring offsets,
// which a packet captured outside the ring cannot supply, so it is deliberately not touched here.
static void FLACOfferCommentPacket(const uint8_t* p, int32_t len){
    if(!p || len < 13) return;
    int32_t pos = 0;
    if(p[0] == 0x7F && !memcmp(p + 1, "FLAC", 4)){
        pos = 9;                                            // the Ogg FLAC mapping header
        // The stream's FIRST packet carries the mapping header, then the native `fLaC` marker and a STREAMINFO
        // block - which is why pos 9 there is the letter 'f', reported as "type 102" before this.  A packet that
        // follows it (a track change's new comment) starts straight at its block header.
        if(len >= 13 && !memcmp(p + 9, "fLaC", 4)) pos = 13;
    }
    if(pos + 4 > len) return;
    const uint8_t  type = p[pos] & 0x7F;
    const uint32_t blen = ((uint32_t)p[pos + 1] << 16) | ((uint32_t)p[pos + 2] << 8) | (uint32_t)p[pos + 3];
    pos += 4;
    if(type != 4) return;
    // The block length is NOT required to fit the capture.  A source that embeds a PICTURE puts tens of KB in one
    // comment block, so requiring it to fit meant "no title at all" on those streams; here the entries that were
    // captured are parsed and the list simply ends where the capture does.  TITLE and ARTIST are conventionally the
    // first entries, which is what makes DEOGG_META_MAX a memory decision rather than a correctness one.
    const int32_t end = (blen <= (uint32_t)(len - pos)) ? (pos + (int32_t)blen) : len;
    if(pos + 4 > end) return;
    // The vendor string is the first length read out of the packet, and it is the one that used to be trusted: a value
    // read from foreign data (a Vorbis header) went negative as int32_t, slipped past the `> end` test and became a
    // pointer offset - the LoadProhibited.  A length that does not fit means this is not a FLAC comment.
    const uint32_t vlen = dgLe32(p + pos);
    if(vlen > (uint32_t)(end - pos - 4)) return;
    pos += 4 + (int32_t)vlen;
    if(pos + 4 > end) return;
    const uint32_t count = dgLe32(p + pos);
    pos += 4;
    const char* t = NULL; uint32_t tlen = 0;
    const char* a = NULL; uint32_t alen = 0;
    for(uint32_t k = 0; k < count && pos + 4 <= end; k++){
        const uint32_t sl = dgLe32(p + pos);
        pos += 4;
        if(sl > (uint32_t)(end - pos)) break;
        const char* s = (const char*)(p + pos);
        pos += (int32_t)sl;
        if(dgKeyIs(s, sl, "TITLE"))       { t = s + 6; tlen = sl - 6; }
        else if(dgKeyIs(s, sl, "ARTIST")) { a = s + 7; alen = sl - 7; }
    }
    if(!t && !a) return;
    // Written into the de-ogger's OWN buffer: see s_dgTitle.  s_flacStreamTitle belongs to the decoder and does not
    // exist yet at connect, which is when this matters most.  Copied by length, not by strncat: a comment field is
    // length-prefixed and need not be NUL-terminated, so strncat would read past it looking for one.
    size_t w = 0;
    s_dgTitle[0] = 0;
    if(a){
        size_t n2 = alen < 120 ? alen : 120;
        if(n2 > sizeof(s_dgTitle) - 1) n2 = sizeof(s_dgTitle) - 1;
        memcpy(s_dgTitle, a, n2); w = n2; s_dgTitle[w] = 0;
    }
    if(a && t && w + 3 < sizeof(s_dgTitle)){ memcpy(s_dgTitle + w, " - ", 3); w += 3; s_dgTitle[w] = 0; }
    if(t){
        size_t n2 = tlen < 120 ? tlen : 120;
        if(w + n2 > sizeof(s_dgTitle) - 1) n2 = sizeof(s_dgTitle) - 1 - w;
        memcpy(s_dgTitle + w, t, n2); w += n2; s_dgTitle[w] = 0;
    }
    s_dgTitleNew = true;
    // Kept as well, for the decoder's own metadata path: harmless there, and it is what a track change has always used.
    if(!s_flacStreamTitle) s_flacStreamTitle = flac_x_ps_calloc(256, 1);
    if(s_flacStreamTitle){
        strncpy(s_flacStreamTitle, s_dgTitle, 255);
        s_flacStreamTitle[255] = 0;
        s_f_flacNewStreamtitle = true;
    }
}

int32_t FLACDeOgg(uint8_t* dst, int32_t dstRoom, const uint8_t* src, int32_t len){
    /* ONE BYTE IN, AT MOST ONE BYTE OUT: output may never exceed input, which is why the caller reads the full
       `room`.  writeSpace() is contiguous-only, so a reserve subtracted from it latches the reader permanently. */
    if(dstRoom < len) return -1;                // callers cap `len` at the space they actually have
    int32_t out = 0, i = 0;
    while(i < len){
        if(s_dgState == DEOGG_PASS){
            memcpy(dst + out, src + i, len - i);
            out += len - i;
            break;
        }
        if(s_dgState == DEOGG_HEADER){
            uint8_t b = src[i++];
            s_dgHdr[s_dgHdrN++] = b;
            if(!s_dgStrip) dst[out++] = b;      // a page header belongs to the stream until the flip
            if(s_dgHdrN < 27) continue;
            if(memcmp(s_dgHdr, "OggS", 4) != 0){
                // Not Ogg at all - an MP3, an AAC stream, a page-less file.  Its 27 bytes are already forwarded;
                // stop looking.  That is the entire cost this imposes on a normal stream.
                s_dgState = DEOGG_PASS;
                continue;
            }
            if(s_dgHdr[26] == 0){
                // A page carrying no packets is legal Ogg and has no lacing table.  Consuming one byte as if it did
                // put the parser one byte out of phase and ended in a permanent DEOGG_PASS, which fed page headers
                // to the decoder as audio.
                s_dgHdrN = 0;
                s_dgState = DEOGG_HEADER;
                continue;
            }
            s_dgFirstPage = false;
            s_dgLacingN = 0;
            s_dgState = DEOGG_LACING;
            continue;
        }
        if(s_dgState == DEOGG_LACING){
            uint8_t b = src[i++];
            s_dgLacing[s_dgLacingN++] = b;
            if(!s_dgStrip) dst[out++] = b;      // so does the lacing table
            if(s_dgLacingN < s_dgHdr[26]) continue;
            // Merge the lacing values into per-packet runs, as FLACparseOGG() does.  They are needed only to know
            // where one packet ends and the next begins, because the flip is decided per packet.
            s_dgRunCount = 0;
            uint32_t run = 0;
            for(uint8_t k = 0; k < s_dgLacingN; k++){
                run += s_dgLacing[k];
                if(s_dgLacing[k] < 255){
                    if(s_dgRunCount < 255) s_dgRuns[s_dgRunCount++] = (uint16_t)(run > 0xFFFF ? 0xFFFF : run);
                    run = 0;
                }
            }
            if(run && s_dgRunCount < 255) s_dgRuns[s_dgRunCount++] = (uint16_t)(run > 0xFFFF ? 0xFFFF : run);
            s_dgPayLeft = 0;
            for(uint16_t k = 0; k < s_dgRunCount; k++) s_dgPayLeft += s_dgRuns[k];
            // A page whose last lacing value is 255 ends mid-packet, so its trailing run starts nothing; it is marked
            // here so the classifier treats it as a continuation rather than a packet start.
            s_dgTailRun = (s_dgLacingN && s_dgLacing[s_dgLacingN - 1] == 255 && s_dgRunCount)
                          ? (uint16_t)(s_dgRunCount - 1) : 0xFFFF;
            s_dgRunIdx = 0; s_dgRunLeft = s_dgRunCount ? s_dgRuns[0] : 0; s_dgPeekN = 0;
            // headerType bit 0 says this page continues the packet before it, so its first run has no packet start to
            // classify either.  Latched here because the payload loop needs it after the header is consumed.
            s_dgContPage = (s_dgHdr[5] & 0x01) != 0;
            // A pending capture survives the boundary only if this page really continues it; anything else means the
            // packet ended without a continuation, and half a block is worth nothing.
            if(s_dgMetaPending && !s_dgContPage){ s_dgMetaPending = false; s_dgMetaLen = 0; s_dgMetaOverrun = false; }
            if(!s_dgMetaPending){ s_dgMetaLen = 0; s_dgMetaOverrun = false; }
            // A continuation run is mid-packet: emitted, unless it completes a capture being reassembled.
            s_dgMetaKind = (!s_dgStrip)               ? DEOGG_META_UNKNOWN
                         : (s_dgContPage && !s_dgMetaPending) ? DEOGG_META_AUDIO
                         : (s_dgContPage &&  s_dgMetaPending) ? DEOGG_META_CANDIDATE
                         : DEOGG_META_UNKNOWN;
            s_dgHdrN = 0; s_dgLacingN = 0;
            s_dgState = s_dgPayLeft ? DEOGG_PAYLOAD : DEOGG_HEADER;
            continue;
        }
        // DEOGG_PAYLOAD
        if(s_dgPayLeft == 0){ s_dgState = DEOGG_HEADER; s_dgHdrN = 0; continue; }
        if(s_dgStrip){
            // From the flip on the output is payload and nothing else - but the pages still delimit PACKETS, and a
            // packet that is not a FLAC frame is a metadata packet.  So this walks run by run instead of copying the
            // rest of the page in one go.
            if(s_dgRunLeft == 0){                                   // a packet ended: start the next one
                if((uint16_t)(s_dgRunIdx + 1) < s_dgRunCount){
                    s_dgRunIdx++;
                    s_dgRunLeft = s_dgRuns[s_dgRunIdx];
                    // A page that both starts and ends mid-packet has one run, a continuation with no start to
                    // classify: emitted straight through.  Any other trailing run does start a packet.
                    const bool continuationOnly = (s_dgRunIdx == s_dgTailRun) && (s_dgRunIdx == 0) && s_dgContPage;
                    s_dgMetaKind = continuationOnly ? DEOGG_META_AUDIO : DEOGG_META_UNKNOWN;
                    if(!s_dgMetaPending){
                        s_dgMetaLen = 0;
                        s_dgMetaOverrun = false;
                    }
                }
                else if(!s_dgPayLeft){ s_dgState = DEOGG_HEADER; s_dgHdrN = 0; s_dgMetaLen = 0; continue; }
                else { s_dgRunLeft = (uint16_t)min(s_dgPayLeft, (uint32_t)0xFFFF); }   // defensive: no run list left
            }
            uint32_t n = (uint32_t)(len - i);
            if(n > s_dgPayLeft) n = s_dgPayLeft;
            if(n > s_dgRunLeft) n = s_dgRunLeft;
            if(s_dgMetaKind == DEOGG_META_UNKNOWN){
                // The peek decides and defaults to AUDIO (see dgLooksLikeMetadata()).  It never waits for more bytes
                // than the run holds: a one-byte packet is legal Ogg and waiting would stall the reader.
                if(n >= 2 && src[i] == 0xFF && (src[i + 1] & 0xFC) == 0xF8) s_dgMetaKind = DEOGG_META_AUDIO;
                else if(n >= 4) s_dgMetaKind = dgLooksLikeMetadata(src + i, n) ? DEOGG_META_CANDIDATE
                                                                             : DEOGG_META_AUDIO;
                else            s_dgMetaKind = DEOGG_META_AUDIO;    // too short to be either: never drop it
            }
            if(s_dgMetaKind == DEOGG_META_AUDIO){
                memcpy(dst + out, src + i, n);
                out += n;
            }
            else {                                                  // a metadata packet: captured, never emitted
                if(!s_dgMeta) s_dgMeta = (uint8_t*)ps_malloc(DEOGG_META_MAX);
                if(s_dgMeta && !s_dgMetaOverrun){
                    uint32_t room = (uint32_t)DEOGG_META_MAX - s_dgMetaLen;
                    uint32_t take = (n > room) ? room : n;
                    memcpy(s_dgMeta + s_dgMetaLen, src + i, take);
                    s_dgMetaLen += take;
                    if(take < n) s_dgMetaOverrun = true;
                }
                else if(!s_dgMeta) s_dgMetaOverrun = true;          // no PSRAM: drop it rather than feed the decoder
            }
            i += n;
            s_dgPayLeft -= n;
            s_dgRunLeft = (s_dgRunLeft > n) ? (uint16_t)(s_dgRunLeft - n) : 0;
            if(s_dgRunLeft == 0){
                // A packet the page cut in half is incomplete: keep the capture for the next page's continuation run.
                const bool openAtPageEnd = (s_dgRunIdx == s_dgTailRun);
                if(s_dgMetaKind == DEOGG_META_CANDIDATE && s_dgMeta && s_dgMetaLen && openAtPageEnd){
                    s_dgMetaPending = true;                 // keep s_dgMetaLen: the continuation appends to it
                }
                else {
                    // Offered even when the capture overran: the parser stops at the first entry that does not fit,
                    // and TITLE/ARTIST come first.
                    if(s_dgMetaKind == DEOGG_META_CANDIDATE && s_dgMeta && s_dgMetaLen){
                        FLACOfferCommentPacket(s_dgMeta, (int32_t)s_dgMetaLen);
                        s_dgCommentHits++;
                        if(s_dgMetaPending) s_dgReassembled++;    // this one was built from two pages
                    }
                    s_dgMetaPending = false;
                    s_dgMetaLen = 0;
                    s_dgMetaOverrun = false;
                }
            }
            continue;
        }
        uint8_t b = src[i];
        // The flip: the first packet that begins a FLAC frame (14-bit sync, 0xFF 0xF8..0xFB).  This is found by the
        // packet's own bytes rather than by a page flag, because the first audio packet can share its page with the
        // last metadata packet.  A continuation half never matches, which is correct - it is not a packet start.
        if(s_dgPeekN == 0) {
            s_dgPeek[0] = b; s_dgPeekN = 1;
            // A capture pending across the page boundary is not restarted: the connect-time comment spans two pages.
            if(!s_dgMetaPending){ s_dgMetaLen = 0; s_dgMetaOverrun = false; }
        }
        else if(s_dgPeekN == 1) {
            s_dgPeek[1] = b; s_dgPeekN = 2;
            if(s_dgPeek[0] == 0xFF && (s_dgPeek[1] & 0xFC) == 0xF8) {
                s_dgStrip = true;
                s_dgMetaLen = 0; s_dgMetaOverrun = false;   // from here the strip path owns that buffer
                s_dgMetaPending = false;
            }
        }
        // Every byte is still forwarded for the decoder's own Ogg parser, and also captured packet by packet: the
        // comment that carries the title at CONNECT is in here, and the decoder steps over it looking for a sync.
        dst[out++] = b; i++; s_dgPayLeft--;
        if(!s_dgStrip && s_dgMeta){
            if(s_dgMetaLen < DEOGG_META_MAX) s_dgMeta[s_dgMetaLen++] = b;
            else                             s_dgMetaOverrun = true;
        }
        if(s_dgRunLeft) s_dgRunLeft--;
        if(s_dgRunLeft == 0){
            const bool moreRuns       = ((uint16_t)(s_dgRunIdx + 1) < s_dgRunCount);
            const bool openAtPageEnd  = !moreRuns && (s_dgRunIdx == s_dgTailRun);
            // A packet that the page cut in half is not complete: keep the capture and let the next page's
            // continuation run append to it.  Offering it here is what produced `cm 2` and no title.
            if(!s_dgStrip && s_dgMeta && s_dgMetaLen && openAtPageEnd){
                s_dgMetaPending = true;
            }
            else {
                if(!s_dgStrip && s_dgMeta && s_dgMetaLen && dgLooksLikeMetadata(s_dgMeta, s_dgMetaLen)){
                    FLACOfferCommentPacket(s_dgMeta, (int32_t)s_dgMetaLen);
                    s_dgCommentHits++;
                }
                s_dgMetaPending = false;
                s_dgMetaLen = 0; s_dgMetaOverrun = false;
            }
            if(moreRuns){
                s_dgRunIdx++;
                s_dgRunLeft = s_dgRuns[s_dgRunIdx];
                s_dgPeekN = 0;                 // a new packet: examine its first two bytes as well
            }
        }
    }
    return out;
}
//----------------------------------------------------------------------------------------------------------------------
//          FLAC INI SECTION
//----------------------------------------------------------------------------------------------------------------------

// prefer PSRAM
#define __malloc_heap_psram(size) \
    heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT|MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT|MALLOC_CAP_INTERNAL)

bool FLACDecoder_AllocateBuffers(void){

    if(!FLACFrameHeader)    {FLACFrameHeader    = (FLACFrameHeader_t*)    __malloc_heap_psram(sizeof(FLACFrameHeader_t));}
    if(!FLACMetadataBlock)  {FLACMetadataBlock  = (FLACMetadataBlock_t*)  __malloc_heap_psram(sizeof(FLACMetadataBlock_t));}
    if(!s_flacStreamTitle)  {s_flacStreamTitle  = (char*)                 __malloc_heap_psram(256);}

    if(!FLACFrameHeader || !FLACMetadataBlock || !s_flacStreamTitle){
        log_e("not enough memory to allocate flacdecoder buffers");
        return false;
    }

    if(psramFound()){
        s_samplesBuffer = (int32_t**)ps_malloc(MAX_CHANNELS * sizeof(int32_t*));
        for (int32_t i = 0; i < MAX_CHANNELS; i++){
            s_samplesBuffer[i] = (int32_t*)ps_malloc(s_maxBlocksize * sizeof(int32_t));
            if(!s_samplesBuffer[i]){
                log_e("not enough memory to allocate flacdecoder buffers");
                return false;
            }
        }
    }
    else  {
        s_samplesBuffer = (int32_t**)malloc(MAX_CHANNELS * sizeof(int32_t*));
        for (int32_t i = 0; i < MAX_CHANNELS; i++){
            s_samplesBuffer[i] = (int32_t*)malloc(s_maxBlocksize * sizeof(int32_t));
            if(!s_samplesBuffer[i]){
                log_e("not enough memory to allocate flacdecoder buffers");
                return false;
            }
        }
    }

    if(!s_flacCarry){
        s_flacCarry = (uint8_t*)__malloc_heap_psram(FLAC_CARRY_SIZE);
        if(!s_flacCarry){ log_e("not enough memory to allocate the flac ogg carry"); return false; }
    }

    FLACDecoder_ClearBuffer();
    FLACDecoder_setDefaults();
    s_flacPageNr = 0;
    return true;
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoder_ClearBuffer(){
    memset(FLACFrameHeader,   0, sizeof(FLACFrameHeader_t));
    memset(FLACMetadataBlock, 0, sizeof(FLACMetadataBlock_t));

    if(s_samplesBuffer) {
        for (int32_t i = 0; i < MAX_CHANNELS; i++){
            memset(s_samplesBuffer[i], 0, s_maxBlocksize * sizeof(int32_t));
        }
    }

    s_flacSegmTableVec.clear(); s_flacSegmTableVec.shrink_to_fit();
    s_flacStatus = DECODE_FRAME;
    s_flacCarryLen = 0;
    s_flacCarryRead = 0;
    s_f_flacCarryComplete = false;
    // The page flags too: this runs on every resync, and a stale "this page starts with a continuation" makes the
    // next page's first run look orphaned - which the skip path then throws away, losing a real frame start and
    // shifting every packet after it.  That is a self-sustaining desync, not a one-off error.
    s_f_flacPageStartsContinued = false;
    s_f_flacPageEndsMidPacket = false;
    s_f_flacRunEndsPacket = true;
    return;
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoder_FreeBuffers(){
    if(FLACFrameHeader)    {free(FLACFrameHeader);    FLACFrameHeader    = NULL;}
    if(FLACMetadataBlock)  {free(FLACMetadataBlock);  FLACMetadataBlock  = NULL;}
    if(s_flacStreamTitle)  {free(s_flacStreamTitle);  s_flacStreamTitle  = NULL;}
    if(s_flacVendorString) {free(s_flacVendorString); s_flacVendorString = NULL;}
    if(s_flacCarry)        {free(s_flacCarry);        s_flacCarry        = NULL;}

    if(s_samplesBuffer){
        for (int32_t i = 0; i < MAX_CHANNELS; i++){
            if(s_samplesBuffer[i]){free(s_samplesBuffer[i]);}
        }
        free(s_samplesBuffer); s_samplesBuffer = NULL;
    }
    coefs.clear(); coefs.shrink_to_fit();
    s_flacSegmTableVec.clear(); s_flacSegmTableVec.shrink_to_fit();
    s_flacBlockPicItem.clear(); s_flacBlockPicItem.shrink_to_fit();
    #if FLAC_PROFILE
        FLACProfileReset();
    #endif
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoder_setDefaults(){
    coefs.clear(); coefs.shrink_to_fit();
    s_flacSegmTableVec.clear(); s_flacSegmTableVec.shrink_to_fit();
    s_flacBlockPicItem.clear(); s_flacBlockPicItem.shrink_to_fit();
    s_flac_bitBuffer = 0;
    s_flacBitrate = 0;
    s_flacBlockPicLenUntilFrameEnd = 0;
    s_flacCurrentFilePos = 0;
    s_flacBlockPicPos = 0;
    s_flacBlockPicLen = 0;
    s_flacRemainBlockPicLen = 0;
    s_flacAudioDataStart = 0;
    s_numOfOutSamples = 0;
    s_offset = 0;
    s_flacValidSamples = 0;
    s_rIndex = 0;
    s_flacStatus = DECODE_FRAME;
    s_flacCompressionRatio = 0;
    s_flacBitBufferLen = 0;
    s_flac_pageSegments = 0;
    s_f_flacNewStreamtitle = false;
    s_f_flacFirstCall = true;
    s_f_oggWrapper = false;
    s_f_lastMetaDataBlock = false;
    s_f_flacNewMetadataBlockPicture = false;
    s_f_flacParseOgg = false;
    s_f_bitReaderError = false;
    s_nBytes = 0;
    s_flacCarryLen = 0;
    s_flacCarryRead = 0;
    s_f_flacCarryComplete = false;
    s_f_flacRunEndsPacket = true;
    s_f_flacPageEndsMidPacket = false;
    s_f_flacPageStartsContinued = false;
}
//----------------------------------------------------------------------------------------------------------------------
//            B I T R E A D E R
//----------------------------------------------------------------------------------------------------------------------
const uint32_t mask[] = {0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f,
                         0x0000007f, 0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff,
                         0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
                         0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
                         0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

uint32_t readUint(uint8_t nBits, int32_t *bytesLeft){
    while (s_flacBitBufferLen < nBits){
        uint8_t temp = *(s_flacInptr + s_rIndex);
        s_rIndex++;
        (*bytesLeft)--;
        if(*bytesLeft < 0) { log_e("error in bitreader"); s_f_bitReaderError = true; break;}
        s_flac_bitBuffer = (s_flac_bitBuffer << 8) | temp;
        s_flacBitBufferLen += 8;
    }
    s_flacBitBufferLen -= nBits;
    uint32_t result = s_flac_bitBuffer >> s_flacBitBufferLen;
    if (nBits < 32)
        result &= mask[nBits];
    return result;
}

int32_t readSignedInt(int32_t nBits, int32_t* bytesLeft){
    int32_t temp = readUint(nBits, bytesLeft) << (32 - nBits);
    temp = temp >> (32 - nBits); // The C++ compiler uses the sign bit to fill vacated bit positions
    return temp;
}

int64_t readRiceSignedInt(uint8_t param, int32_t* bytesLeft){
    long val = 0;
    while (readUint(1, bytesLeft) == 0)
        val++;
    val = (val << param) | readUint(param, bytesLeft);
    return (val >> 1) ^ -(val & 1);
}

void alignToByte() {
    s_flacBitBufferLen -= s_flacBitBufferLen % 8;
}
//----------------------------------------------------------------------------------------------------------------------
//              F L A C - D E C O D E R
//----------------------------------------------------------------------------------------------------------------------
void FLACSetRawBlockParams(uint8_t Chans, uint32_t SampRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength){
    FLACMetadataBlock->numChannels = Chans;
    FLACMetadataBlock->sampleRate = SampRate;
    FLACMetadataBlock->bitsPerSample = BPS;
    FLACMetadataBlock->totalSamples = tsis;  // total samples in stream
    FLACMetadataBlock->audioDataLength = AuDaLength;
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoderReset(){ // set var to default
    FLACDecoder_setDefaults();
    FLACDecoder_ClearBuffer();
}
//----------------------------------------------------------------------------------------------------------------------
int32_t FLACFindSyncWord(unsigned char *buf, int32_t nBytes) {

    int32_t i = FLAC_specialIndexOf(buf, "OggS", nBytes);
    if(i == 0) {s_f_bitReaderError = false; return 0;}  // flag has ogg wrapper

    if(s_f_oggWrapper && i > 0){
        s_f_bitReaderError = false;
        return i;
    }
    else{
         /* find byte-aligned sync code - need 14 matching bits */
        for (i = 0; i < nBytes - 1; i++) {
            if ((buf[i + 0] & 0xFF) == 0xFF  && (buf[i + 1] & 0xFC) == 0xF8) { // <14> Sync code '11111111111110xx'
                if(i) FLACDecoderReset();
            //    s_f_bitReaderError = false;
                return i;
            }
        }
    }
    return -1;
}
//----------------------------------------------------------------------------------------------------------------------
boolean FLACFindMagicWord(unsigned char* buf, int32_t nBytes){
    int32_t idx = FLAC_specialIndexOf(buf, "fLaC", nBytes);
    if(idx >0){ // Metadatablock follows
        idx += 4;
        boolean lmdbf = ((buf[idx + 1] & 0x80) == 0x80); // Last-metadata-block flag
        uint8_t bt = (buf[idx + 1] & 0x7F); // block type
        uint32_t lomd = (buf[idx + 2] << 16) + (buf[idx + 3] << 8) + buf[idx + 4]; // Length of metadata to follow

        (void)lmdbf; (void)bt; (void)lomd;
        // log_i("Last-metadata-block flag: %d", lmdbf);
        // log_i("block type: %d", bt);
        // log_i("Length (in bytes) of metadata to follow: %d", lomd);
        return true;
    }
    return false;
}
//----------------------------------------------------------------------------------------------------------------------
char* FLACgetStreamTitle(){
    if(s_f_flacNewStreamtitle){
        s_f_flacNewStreamtitle = false;
        return s_flacStreamTitle;
    }
    return NULL;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t FLACparseOGG(uint8_t *inbuf, int32_t *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    s_f_flacParseOgg = false;
    int32_t idx = FLAC_specialIndexOf(inbuf, "OggS", 6);
    if(idx != 0) return ERR_FLAC_DECODER_ASYNC;

    uint8_t  version            = *(inbuf +  4); (void) version;
    uint8_t  headerType         = *(inbuf +  5); (void) headerType;
    uint64_t granulePosition    = (uint64_t)*(inbuf + 13) << 56;  // granule_position: an 8 Byte field containing -
             granulePosition   += (uint64_t)*(inbuf + 12) << 48;  // position information. For an audio stream, it MAY
             granulePosition   += (uint64_t)*(inbuf + 11) << 40;  // contain the total number of PCM samples encoded
             granulePosition   += (uint64_t)*(inbuf + 10) << 32;  // after including all frames finished on this page.
             granulePosition   += *(inbuf +  9) << 24;  // This is a hint for the decoder and gives it some timing
             granulePosition   += *(inbuf +  8) << 16;  // and position information. A special value of -1 (in two's
             granulePosition   += *(inbuf +  7) << 8;   // complement) indicates that no packets finish on this page.
             granulePosition   += *(inbuf +  6); (void) granulePosition;
    uint32_t bitstreamSerialNr  = *(inbuf + 17) << 24;  // bitstream_serial_number: a 4 Byte field containing the
             bitstreamSerialNr += *(inbuf + 16) << 16;  // unique serial number by which the logical bitstream
             bitstreamSerialNr += *(inbuf + 15) << 8;   // is identified.
             bitstreamSerialNr += *(inbuf + 14); (void) bitstreamSerialNr;
    uint32_t pageSequenceNr     = *(inbuf + 21) << 24;  // page_sequence_number: a 4 Byte field containing the sequence
             pageSequenceNr    += *(inbuf + 20) << 16;  // number of the page so the decoder can identify page loss
             pageSequenceNr    += *(inbuf + 19) << 8;   // This sequence number is increasing on each logical bitstream
             pageSequenceNr    += *(inbuf + 18); (void) pageSequenceNr;
    uint32_t CRCchecksum        = *(inbuf + 25) << 24;
             CRCchecksum       += *(inbuf + 24) << 16;
             CRCchecksum       += *(inbuf + 23) << 8;
             CRCchecksum       += *(inbuf + 22); (void) CRCchecksum;
    uint8_t  pageSegments       = *(inbuf + 26);        // giving the number of segment entries

    // read the segment table (contains pageSegments bytes),  1...251: Length of the frame in bytes,
    // 255: A second byte is needed.  The total length is first_byte + second byte
    s_flacSegmTableVec.clear();
    s_flacSegmTableVec.shrink_to_fit();
    for(int32_t i = 0; i < pageSegments; i++){
        int32_t n = *(inbuf + 27 + i);
        while(*(inbuf + 27 + i) == 255){
            i++;
            if(i == pageSegments) break;
            n+= *(inbuf + 27 + i);
        }
        s_flacSegmTableVec.insert(s_flacSegmTableVec.begin(), n);
    }
    // for(int32_t i = 0; i< s_flacSegmTableVec.size(); i++){log_w("%i", s_flacSegmTableVec[i]);}

    bool     continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    bool     firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    bool     lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)

    // Both ends of a page have to be remembered, because either can split a FLAC frame:
    //   - this page's FINAL LACING VALUE is 255, so the packet that starts here runs off the end of the page (the
    //     merged run lengths cannot answer this: the merge hides which values were 255);
    //   - headerType bit 0 says the FIRST run continues a packet from the previous page.
    // `continuedPage` used to be read and thrown away, which is why a split frame could never be recognised.
    s_f_flacPageEndsMidPacket  = (pageSegments > 0) && (*(inbuf + 27 + pageSegments - 1) == 255);
    s_f_flacPageStartsContinued = continuedPage;
    (void)lastPage;

    // log_w("firstPage %i, continuedPage %i, lastPage %i", firstPage, continuedPage, lastPage);

    if(firstPage) s_flacPageNr = 0;

    uint32_t headerSize = pageSegments + 27;

    *bytesLeft -= headerSize;
    s_flacCurrentFilePos += headerSize;
    return ERR_FLAC_NONE; // no error
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
vector<uint32_t> FLACgetMetadataBlockPicture(){
    if(s_f_flacNewMetadataBlockPicture){
        s_f_flacNewMetadataBlockPicture = false;
        return s_flacBlockPicItem;
    }
    if(s_flacBlockPicItem.size() > 0){
        s_flacBlockPicItem.clear();
        s_flacBlockPicItem.shrink_to_fit();
    }
    return s_flacBlockPicItem;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t parseFlacFirstPacket(uint8_t *inbuf, int16_t nBytes){ // 4.2.2. Identification header   https://xiph.org/flac/ogg_mapping.html

    int32_t ret = 0;
    int32_t idx = FLAC_specialIndexOf(inbuf, "fLaC", nBytes);
    //log_i("idx %i, nBytes %i", idx, nBytes);
    if(idx >= 0){ // FLAC signature found
        ret = idx + 4;
    }
    else {
        log_e("FLAC signature not found");
        ret = ERR_FLAC_DECODER_ASYNC;
    }
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t parseMetaDataBlockHeader(uint8_t *inbuf, int16_t nBytes){
    int8_t   ret = FLAC_PARSE_OGG_DONE;
    uint16_t pos = 0;
    int32_t  blockLength = 0;
    uint16_t minBlocksize = 0;
    uint16_t maxBlocksize = 0;
    uint32_t minFrameSize = 0;
    uint32_t maxFrameSize = 0;
    uint32_t sampleRate = 0;
    uint32_t vendorLength = 0;
    uint32_t commemtStringLength = 0;
    uint32_t userCommentListLength = 0;
    uint8_t  nrOfChannels = 0;
    uint8_t  bitsPerSample = 0;
    uint64_t totalSamplesInStream = 0;
    uint8_t  mdBlockHeader = 0;
    uint8_t  blockType = 0;
    uint8_t  bt = 0;
    char*    vb[8] = {0}; // vorbis comment

    enum {streamInfo, padding, application, seekTable, vorbisComment, cueSheet, picture};


    while(true){
        mdBlockHeader         = *(inbuf + pos);
        s_f_lastMetaDataBlock = mdBlockHeader & 0b10000000; //log_w("lastMdBlockFlag %i", s_f_lastMetaDataBlock);
        blockType             = mdBlockHeader & 0b01111111; //log_w("blockType %i", blockType);

        blockLength        = *(inbuf + pos + 1) << 16;
        blockLength       += *(inbuf + pos + 2) << 8;
        blockLength       += *(inbuf + pos + 3); //log_w("blockLength %i", blockLength);

        nBytes -= 4;
        pos += 4;

        switch(blockType) {
            case 0:
                bt = streamInfo;
                break;
            case 1:
                bt = padding;
            //  log_e("padding");
                return ERR_FLAC_NONE;
                break;
            case 2:
                bt = application;
                log_e("application");
                return ERR_FLAC_UNIMPLEMENTED;
                break;
            case 3:
                bt = seekTable;
                log_e("seekTable");
                return ERR_FLAC_UNIMPLEMENTED;
                break;
            case 4:
                bt = vorbisComment;
                break;
            case 5:
                bt = cueSheet;
                log_e("cueSheet");
                return ERR_FLAC_UNIMPLEMENTED;
                break;
            case 6:
                bt = picture;
                break;
            default:
                bt = streamInfo;
                //return ERR_FLAC_UNIMPLEMENTED;
                break;
        }

        switch(bt){
            case streamInfo:
                minBlocksize += *(inbuf + pos + 0) << 8;
                minBlocksize += *(inbuf + pos + 1);
                maxBlocksize += *(inbuf + pos + 2) << 8;
                maxBlocksize += *(inbuf + pos + 3);
                //log_i("minBlocksize %i", minBlocksize);
                //log_i("maxBlocksize %i", maxBlocksize);
                FLACMetadataBlock->minblocksize = minBlocksize;
                FLACMetadataBlock->maxblocksize = maxBlocksize;

                if(maxBlocksize > s_maxBlocksize){log_e("s_blocksize is too big"); return ERR_FLAC_BLOCKSIZE_TOO_BIG;}

                minFrameSize  = *(inbuf + pos + 4) << 16;
                minFrameSize += *(inbuf + pos + 5) << 8;
                minFrameSize += *(inbuf + pos + 6);
                maxFrameSize  = *(inbuf + pos + 7) << 16;
                maxFrameSize += *(inbuf + pos + 8) << 8;
                maxFrameSize += *(inbuf + pos + 9);
                //log_i("minFrameSize %i", minFrameSize);
                //log_i("maxFrameSize %i", maxFrameSize);
                FLACMetadataBlock->minframesize = minFrameSize;
                FLACMetadataBlock->maxframesize = maxFrameSize;

                sampleRate   =  *(inbuf + pos + 10) << 12;
                sampleRate  +=  *(inbuf + pos + 11) << 4;
                sampleRate  += (*(inbuf + pos + 12) & 0xF0) >> 4;
                //log_i("sampleRate %i", sampleRate);
                FLACMetadataBlock->sampleRate = sampleRate;

                nrOfChannels = ((*(inbuf + pos + 12) & 0x0E) >> 1) + 1;
                //log_i("nrOfChannels %i", nrOfChannels);
                FLACMetadataBlock->numChannels = nrOfChannels;

                bitsPerSample  =  (*(inbuf + pos + 12) & 0x01) << 5;
                bitsPerSample += ((*(inbuf + pos + 13) & 0xF0) >> 4) + 1;
                FLACMetadataBlock->bitsPerSample = bitsPerSample;
                if(bitsPerSample > 16) log_i("FLAC %u-bit source, output downshifted to 16", bitsPerSample);
                //log_i("bitsPerSample %i", bitsPerSample);

                totalSamplesInStream  = (uint64_t)(*(inbuf + pos + 17) & 0x0F) << 32;
                totalSamplesInStream += (*(inbuf + pos + 14)) << 24;
                totalSamplesInStream += (*(inbuf + pos + 14)) << 16;
                totalSamplesInStream += (*(inbuf + pos + 15)) << 8;
                totalSamplesInStream += (*(inbuf + pos + 16));
                //log_i("totalSamplesInStream %lli", totalSamplesInStream);
                FLACMetadataBlock->totalSamples = totalSamplesInStream;

                //log_i("nBytes %i, blockLength %i", nBytes, blockLength);
                pos += blockLength;
                nBytes -= blockLength;
                if(ret == FLAC_PARSE_OGG_DONE) return ret;
                break;

            case vorbisComment:                                // https://www.xiph.org/vorbis/doc/v-comment.html
                vendorLength  = *(inbuf + pos + 3) << 24;
                vendorLength += *(inbuf + pos + 2) << 16;
                vendorLength += *(inbuf + pos + 1) <<  8;
                vendorLength += *(inbuf + pos + 0);
                if(vendorLength > 1024){
                    log_e("vendorLength > 1024 bytes");
                }
                if(s_flacVendorString) {free(s_flacVendorString); s_flacVendorString = NULL;}
                s_flacVendorString = (char*) flac_x_ps_calloc(vendorLength + 1, sizeof(char));
                memcpy(s_flacVendorString, inbuf + pos + 4, vendorLength);
                //log_i("%s", s_flacVendorString);

                pos += 4 + vendorLength;
                userCommentListLength  = *(inbuf + pos + 3) << 24;
                userCommentListLength += *(inbuf + pos + 2) << 16;
                userCommentListLength += *(inbuf + pos + 1) <<  8;
                userCommentListLength += *(inbuf + pos + 0);

                pos += 4;
                commemtStringLength = 0;
                for(int32_t i = 0; i < userCommentListLength; i++){
                    commemtStringLength  = *(inbuf + pos + 3) << 24;
                    commemtStringLength += *(inbuf + pos + 2) << 16;
                    commemtStringLength += *(inbuf + pos + 1) <<  8;
                    commemtStringLength += *(inbuf + pos + 0);

                    if((FLAC_specialIndexOf(inbuf + pos + 4, "TITLE", 6) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "title", 6) == 0)){
                        vb[0] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 6), min((uint32_t)127, commemtStringLength - 6));
                        //log_w("TITLE: %s", vb[0]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "ARTIST", 7) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "artist", 7) == 0)){
                        vb[1] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 7), min((uint32_t)127, commemtStringLength - 7));
                        //log_w("ARTIST: %s", vb[1]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "GENRE", 6) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "genre", 6) == 0)){
                        vb[2] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 6), min((uint32_t)127, commemtStringLength - 6));
                        //log_w("GENRE: %s", vb[2]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "ALBUM", 6) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "album", 6) == 0)){
                        vb[3] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 6), min((uint32_t)127, commemtStringLength - 6));
                        //log_w("ALBUM: %s", vb[3]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "COMMENT", 8) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "comment", 8) == 0)){
                        vb[4] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 8), min((uint32_t)127, commemtStringLength - 8));
                        //log_w("COMMENT: %s", vb[4]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "DATE", 5) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "date", 5) == 0)){
                        vb[5] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 5), min((uint32_t)127, commemtStringLength - 12));
                        //log_w("DATE: %s", vb[5]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "TRACKNUMBER", 12) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "tracknumber", 12) == 0)){
                        vb[6] = flac_x_ps_strndup((const char*)(inbuf + pos + 4 + 12), min((uint32_t)127, commemtStringLength - 12));
                        //log_w("TRACKNUMBER: %s", vb[6]);
                    }
                    if((FLAC_specialIndexOf(inbuf + pos + 4, "METADATA_BLOCK_PICTURE", 23) == 0) || (FLAC_specialIndexOf(inbuf + pos + 4, "metadata_block_picture", 23) == 0)){
                        //log_w("METADATA_BLOCK_PICTURE found, commemtStringLength %i", commemtStringLength);
                        s_flacBlockPicLen = commemtStringLength - 23;
                        s_flacBlockPicPos = s_flacCurrentFilePos + pos + 4 + 23;
                        s_flacBlockPicLenUntilFrameEnd = nBytes - (pos + 23);
                        if(s_flacBlockPicLen < s_flacBlockPicLenUntilFrameEnd) s_flacBlockPicLenUntilFrameEnd = s_flacBlockPicLen;
                        s_flacRemainBlockPicLen = s_flacBlockPicLen - s_flacBlockPicLenUntilFrameEnd;
                        //log_i("s_flacBlockPicPos %i, s_flacBlockPicLen %i", s_flacBlockPicPos, s_flacBlockPicLen);
                        //log_i("s_flacBlockPicLenUntilFrameEnd %i, s_flacRemainBlockPicLen %i", s_flacBlockPicLenUntilFrameEnd, s_flacRemainBlockPicLen);
                        if(s_flacRemainBlockPicLen <= 0) s_f_lastMetaDataBlock = true; // exeption:: goto audiopage after commemt if lastMetaDataFlag is not set
                        if(s_flacBlockPicLen){
                            s_flacBlockPicItem.clear();
                            s_flacBlockPicItem.shrink_to_fit();
                            s_flacBlockPicItem.push_back(s_flacBlockPicPos);
                            s_flacBlockPicItem.push_back(s_flacBlockPicLenUntilFrameEnd);
                        }
                    }
                    pos += 4 + commemtStringLength;
                    //log_i("nBytes %i, pos %i, commemtStringLength %i", nBytes, pos, commemtStringLength);
                }
                memset(s_flacStreamTitle, 0, 256);
                if(vb[1] && vb[0]){ // artist and title
                    strcpy(s_flacStreamTitle, vb[1]);
                    strcat(s_flacStreamTitle, " - ");
                    strcat(s_flacStreamTitle, vb[0]);
                    s_f_flacNewStreamtitle = true;
                }
                else if(vb[1]){
                    strcpy(s_flacStreamTitle, vb[1]);
                    s_f_flacNewStreamtitle = true;
                }
                else if(vb[0]){
                    strcpy(s_flacStreamTitle, vb[0]);
                    s_f_flacNewStreamtitle = true;
                }
                for(int32_t i = 0; i < 8; i++){
                    if(vb[i]){free(vb[i]); vb[i] = NULL;}
                }

                if(!s_flacBlockPicLen && s_flacSegmTableVec.size() == 1) s_f_lastMetaDataBlock = true; // exeption:: goto audiopage after commemt if lastMetaDataFlag is not set
                if(ret == FLAC_PARSE_OGG_DONE) return ret;
                break;

            case picture:
                if(ret == FLAC_PARSE_OGG_DONE) return ret;
                break;

            default:
                return ret;
                break;
        }
    }
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
// The carry could not take a run, which means this frame cannot be reassembled.  Drop it and let the caller's
// normal accounting advance the ring; the next run starts a fresh frame.  Said once, because a stream that does
// this does it at every page boundary and the log is otherwise the flood this whole change exists to avoid.
static void dropSplitFrame(uint32_t runLen){
    static bool warned = false;
    if(!warned){
        warned = true;
        log_e("flac: a split frame needs %u bytes, more than the %u byte carry - dropping it",
              (unsigned)(s_flacCarryLen + runLen), (unsigned)FLAC_CARRY_SIZE);
    }
    s_flacCarryLen = 0;
    s_flacCarryRead = 0;
    s_f_flacCarryComplete = false;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t FLACDecode(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf){ //  MAIN LOOP

    int32_t                ret = 0;
    uint32_t           segmLen = 0;
    static uint32_t segmLenTmp = 0;

    if(s_f_flacFirstCall){ // determine if ogg or flag
        s_f_flacFirstCall = false;
        s_nBytes = 0;
        segmLenTmp = 0;
        if(FLAC_specialIndexOf(inbuf, "OggS", 5) == 0){
            s_f_oggWrapper = true;
            s_f_flacParseOgg = true;
        }
    }

    // Switch to native decoding once the de-ogger has removed the container - but only after the DECODER has parsed
    // the identification packet and every metadata block.
    //
    // The de-ogger is a producer and runs ahead: on Radio Paradise the flip is 321 bytes into the stream, well inside
    // the first read, so on the very first call the producer has already stripped while the decoder has not yet seen
    // STREAMINFO.  Keying this on the producer's state skipped the whole metadata phase, which is why every FLAC
    // stream went silent with no error and no "decoder: ... Hz" line on 2026.11.14 - while the very same stripped
    // bytes decoded perfectly under ffmpeg offline.
    //
    // s_flacPageNr only reaches 2 when the last metadata block has been parsed, so that is the gate.  Anything still
    // queued in the packet table describes a page that straddled the flip and its byte counts no longer match what
    // the de-ogger emits, so it is dropped; the decoder then resynchronises onto the next frame boundary, losing at
    // most one frame and never decoding from the middle of one.
    if(s_f_oggWrapper && FLACDeOggStripping() && s_flacPageNr == 2){
        s_f_oggWrapper = false;
        s_f_flacParseOgg = false;
        s_nBytes = 0;
        s_flacCarryLen = 0; s_flacCarryRead = 0; s_f_flacCarryComplete = false;
        s_flacSegmTableVec.clear(); s_flacSegmTableVec.shrink_to_fit();
        s_flacStatus = DECODE_FRAME;
        int32_t skip = 0;
        for(; skip + 1 < *bytesLeft; skip++){
            if(*(inbuf + skip) == 0xFF && (*(inbuf + skip + 1) & 0xFC) == 0xF8) break;
        }
        if(skip + 1 >= *bytesLeft) skip = 0;      // no frame in what we hold: leave the pointer, the caller refills
        *bytesLeft -= skip;
        s_flacCurrentFilePos += skip;
        if(s_flacAudioDataStart == 0) s_flacAudioDataStart = s_flacCurrentFilePos;
    }

    if(s_f_oggWrapper){

        if(segmLenTmp){ // can't skip more than 16K
            if(segmLenTmp > MAX_BLOCKSIZE){
                s_flacCurrentFilePos += MAX_BLOCKSIZE;
                *bytesLeft -= MAX_BLOCKSIZE;
                segmLenTmp -= MAX_BLOCKSIZE;
            }
            else{
                s_flacCurrentFilePos += segmLenTmp;
                *bytesLeft -= segmLenTmp;
                segmLenTmp  = 0;
            }
            return FLAC_PARSE_OGG_DONE;
        }

        // A frame reassembled from two pages.  Its bytes were accounted on the ring when they were copied, so this
        // decode must move neither *bytesLeft nor s_flacCurrentFilePos - the carry is not ring memory.  The pointer
        // advances with the read instead: FLACDecodeNative() re-derives its own pointer from the buffer it is given
        // on every call whose status is not OUT_SAMPLES, which is the same contract the ring path relies on.
        if(s_f_flacCarryComplete && s_flacCarryLen && s_flacCarryRead < s_flacCarryLen){
            if(s_flacAudioDataStart == 0) s_flacAudioDataStart = s_flacCurrentFilePos;
            int32_t cl = (int32_t)(s_flacCarryLen - s_flacCarryRead);
            ret = FLACDecodeNative(s_flacCarry + s_flacCarryRead, &cl, outbuf);
            s_flacCarryRead = s_flacCarryLen - (uint32_t)(cl > 0 ? cl : 0);
            if(cl < 0){ s_flacCarryLen = 0; s_flacCarryRead = 0; s_f_flacCarryComplete = false; return ERR_FLAC_DECODER_ASYNC; }
            // ERR_FLAC_NONE with the status back at DECODE_FRAME is the footer having been read: the frame is done.
            if(ret == ERR_FLAC_NONE && s_flacStatus == DECODE_FRAME){ s_flacCarryLen = 0; s_flacCarryRead = 0; s_f_flacCarryComplete = false; }
            return ret;
        }
        if(s_nBytes > 0){
            int16_t diff = s_nBytes;
            if(s_flacAudioDataStart == 0){
                s_flacAudioDataStart = s_flacCurrentFilePos;
            }
            ret = FLACDecodeNative(inbuf, &s_nBytes, outbuf);
            diff -= s_nBytes;
            s_flacCurrentFilePos += diff;
            *bytesLeft -= diff;
            return ret;
        }
        if(s_nBytes < 0){return ERR_FLAC_DECODER_ASYNC;}

        if(s_f_flacParseOgg == true){
            s_f_flacParseOgg = false;
            ret = FLACparseOGG(inbuf, bytesLeft);
            if(ret == ERR_FLAC_NONE) return FLAC_PARSE_OGG_DONE; // ok
            else return ret;  // error
        }
        //-------------------------------------------------------
        // A page with no segments is legal (a granule bump on its own).  back() on the empty vector used to be
        // undefined behaviour; re-parse the next page instead, which is what the caller will present anyway.
        if(s_flacSegmTableVec.empty()){ s_f_flacParseOgg = true; return FLAC_PARSE_OGG_DONE; }
        // Pops run in PAGE ORDER (the lacing walk inserts at the beginning), so the flag set by the parse is
        // consumed by exactly the page's first run - the only one that can continue a packet from the page before.
        const bool firstRunOfPage = s_f_flacPageStartsContinued;
        s_f_flacPageStartsContinued = false;
        segmLen = s_flacSegmTableVec.back();
        s_flacSegmTableVec.pop_back();
        const bool lastRunOfPage = s_flacSegmTableVec.empty();
        if(lastRunOfPage) s_f_flacParseOgg = true;
        // A run finishes its packet unless it is the page's last one and the page ended on a 255 lacing value.
        s_f_flacRunEndsPacket = !(lastRunOfPage && s_f_flacPageEndsMidPacket);
        //-------------------------------------------------------

        if(s_flacRemainBlockPicLen <= 0 && !s_f_flacNewMetadataBlockPicture) {
            if(s_flacBlockPicItem.size() > 0) { // get blockpic data
                // log_i("---------------------------------------------------------------------------");
                // log_i("metadata blockpic found at pos %i, size %i bytes", s_flacBlockPicPos, s_flacBlockPicLen);
                // for(int32_t i = 0; i < s_flacBlockPicItem.size(); i += 2) { log_i("segment %02i, pos %07i, len %05i", i / 2, s_flacBlockPicItem[i], s_flacBlockPicItem[i + 1]); }
                // log_i("---------------------------------------------------------------------------");
                s_f_flacNewMetadataBlockPicture = true;
            }
        }

        switch(s_flacPageNr) {
            case 0:
                ret = parseFlacFirstPacket(inbuf, segmLen);
                if(ret == segmLen) {
                    s_flacPageNr = 1;
                    ret = FLAC_PARSE_OGG_DONE;
                    break;
                }
                if(ret < 0){  // fLaC signature not found
                    break;
                }
                if(ret < segmLen){
                    segmLen -= ret;
                    *bytesLeft -= ret;
                    s_flacCurrentFilePos += ret;
                    inbuf += ret;
                    s_flacPageNr = 1;
                } /* fallthrough */
            case 1:
                if(s_flacRemainBlockPicLen > 0){
                    s_flacRemainBlockPicLen -= segmLen;
                    //log_i("s_flacCurrentFilePos %i, len %i, s_flacRemainBlockPicLen %i", s_flacCurrentFilePos, segmLen, s_flacRemainBlockPicLen);
                    s_flacBlockPicItem.push_back(s_flacCurrentFilePos);
                    s_flacBlockPicItem.push_back(segmLen);
                    if(s_flacRemainBlockPicLen <= 0){s_flacPageNr = 2;}
                    ret = FLAC_PARSE_OGG_DONE;
                    break;
                }
                ret = parseMetaDataBlockHeader(inbuf, segmLen);
                if(s_f_lastMetaDataBlock) s_flacPageNr = 2;
                break;
            case 2: {
                // The first run of a continuation page belongs to a frame that started before it.  With an empty
                // carry we never saw that page - a join mid-stream, or a page lost in transit - so the fragment is
                // dropped instead of being decoded as if it were a whole frame.  That decode is what used to fail
                // with ERR_FLAC_DECODER_ASYNC on every page boundary.
                const bool orphan = firstRunOfPage && (s_flacCarryLen == 0);
                const bool carryable = (segmLen <= MAX_BLOCKSIZE);
                if(!s_f_flacRunEndsPacket && carryable && !orphan){
                    // The page cut this frame: hold the tail until the next page brings the head.
                    if(s_flacCarryLen + segmLen > (uint32_t)FLAC_CARRY_SIZE){ dropSplitFrame(segmLen); break; }
                    memcpy(s_flacCarry + s_flacCarryLen, inbuf, segmLen);
                    s_flacCarryLen += segmLen;
                    s_flacCarryRead = 0;
                    s_f_flacCarryComplete = false;   // still a fragment: nothing may be decoded from it yet
                    s_nBytes = 0;
                    break;
                }
                if(s_flacCarryLen && carryable && !orphan){
                    // This run completes the frame that was started on the previous page.
                    if(s_flacCarryLen + segmLen > (uint32_t)FLAC_CARRY_SIZE){ dropSplitFrame(segmLen); break; }
                    memcpy(s_flacCarry + s_flacCarryLen, inbuf, segmLen);
                    s_flacCarryLen += segmLen;
                    s_flacCarryRead = 0;
                    s_f_flacCarryComplete = true;    // whole at last: the next call decodes it
                    s_nBytes = 0;
                    break;
                }
                s_flacCarryLen = 0;   // a fragment nobody can complete, or a run too large to carry
                s_flacCarryRead = 0;
                s_f_flacCarryComplete = false;
                if(orphan){
                    // There is no head to join this to and no previous page to blame - a join mid-stream, or a
                    // page lost in transit.  Skip its bytes: handing a headless fragment to the decoder is what
                    // produced DECODER ASYNC at every join.  `break` reaches the ring accounting, which advances
                    // past it without decoding anything.
                    s_nBytes = 0;
                    break;
                }
                s_nBytes = segmLen;
                return FLAC_PARSE_OGG_DONE;
            }
        }
        if(segmLen > MAX_BLOCKSIZE){
            segmLenTmp = segmLen;
            return FLAC_PARSE_OGG_DONE;
        }
        *bytesLeft -= segmLen;
        s_flacCurrentFilePos += segmLen;
        return ret;
    }
    ret = FLACDecodeNative(inbuf, bytesLeft, outbuf);
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t FLACDecodeNative(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf){

    int32_t bl = *bytesLeft;
    static int32_t sbl = 0;

    if(s_flacStatus != OUT_SAMPLES){
        s_rIndex = 0;
        s_flacInptr = inbuf;
    }

    while(s_flacStatus == DECODE_FRAME){// Read a ton of header fields, and ignore most of them
        int32_t ret = flacDecodeFrame (inbuf, bytesLeft);
        if(ret != 0) return ret;
        if(*bytesLeft < MAX_BLOCKSIZE) return FLAC_DECODE_FRAMES_LOOP; // need more data
        sbl += bl - *bytesLeft;
    }

    if(s_flacStatus == DECODE_SUBFRAMES){
        // Decode each channel's subframe, then skip footer
        int32_t ret = decodeSubframes(bytesLeft);
        if(ret != 0) return ret;
        s_flacStatus = OUT_SAMPLES;
        sbl += bl - *bytesLeft;
    }

    if(s_flacStatus == OUT_SAMPLES){  // Write the decoded samples
        // blocksize can be much greater than outbuff, so we can't stuff all in once
        // therefore we need often more than one loop (split outputblock into pieces)
        uint32_t blockSize;
        if(s_numOfOutSamples < s_flacOutBuffSize + s_offset) blockSize = s_numOfOutSamples - s_offset;
        else blockSize = s_flacOutBuffSize;

        for (int32_t i = 0; i < blockSize; i++) {
            for (int32_t j = 0; j < FLACMetadataBlock->numChannels; j++) {
                int32_t val = s_samplesBuffer[j][i + s_offset];
                if (FLACMetadataBlock->bitsPerSample == 8) val += 128;
                // A 24-bit source is scaled to the 16-bit output path here rather than being refused upstream.
                if (FLACMetadataBlock->bitsPerSample > 16) val >>= (FLACMetadataBlock->bitsPerSample - 16);
                outbuf[2*i+j] = val;
            }
        }

        s_flacValidSamples = blockSize * FLACMetadataBlock->numChannels;
        s_offset += blockSize;
        if(sbl > 0){
            s_flacCompressionRatio = (float)((s_flacValidSamples * 2) * FLACMetadataBlock->numChannels) / sbl; // valid samples are 16 bit
            sbl = 0;
            s_flacBitrate = FLACMetadataBlock->sampleRate * FLACMetadataBlock->bitsPerSample * FLACMetadataBlock->numChannels;
            s_flacBitrate /= s_flacCompressionRatio;
      //      log_e("s_flacBitrate %i, s_flacCompressionRatio %f, FLACMetadataBlock->sampleRate %i ", s_flacBitrate, s_flacCompressionRatio, FLACMetadataBlock->sampleRate);
        }
        if(s_offset != s_numOfOutSamples) return GIVE_NEXT_LOOP;
        if(s_offset > s_numOfOutSamples) { log_e("offset has a wrong value"); }
        s_offset = 0;
    }

    alignToByte();
    readUint(16, bytesLeft);
    #if FLAC_PROFILE
        s_pfFrames++;     // one completed frame, counted where the footer CRC is consumed
    #endif

//    s_flacCompressionRatio = (float)m_bytesDecoded / (float)s_numOfOutSamples * FLACMetadataBlock->numChannels * (16/8);
//    log_i("s_flacCompressionRatio % f", s_flacCompressionRatio);
    s_flacStatus = DECODE_FRAME;
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t flacDecodeFrame(uint8_t *inbuf, int32_t *bytesLeft){
    if(FLAC_specialIndexOf(inbuf, "OggS", *bytesLeft) == 0){ // async? => new sync is OggS => reset and decode (not page 0 or 1)
        FLACDecoderReset();
        s_flacPageNr = 2;
        return OGG_SYNC_FOUND;
    }
    readUint(14 + 1, bytesLeft); // synccode + reserved bit
    FLACFrameHeader->blockingStrategy = readUint(1, bytesLeft);
    FLACFrameHeader->blockSizeCode = readUint(4, bytesLeft);
    FLACFrameHeader->sampleRateCode = readUint(4, bytesLeft);
    FLACFrameHeader->chanAsgn = readUint(4, bytesLeft);
    FLACFrameHeader->sampleSizeCode = readUint(3, bytesLeft);
    /* Verdicts before anything acts on these codes.  A desynced stream produces bytes that look like a frame header,
       and the reserved values are the cheapest proof that this is not one.  Without this the decoder believed a
       garbage header and follow-on code populated STREAMINFO from it: a 48000 Hz stereo stream became
       `decoder: 96000 Hz, 1 ch, 8 bits -> I2S clock 96000 Hz` mid-playback on 2026.11.14 - an audible rate change
       driven entirely by junk.  Channel assignment 11..15 and sample sizes 3 and 7 are reserved by the format. */
    if(FLACFrameHeader->chanAsgn > 10)       return ERR_FLAC_RESERVED_CHANNEL_ASSIGNMENT;
    if(FLACFrameHeader->sampleSizeCode == 3 ||
       FLACFrameHeader->sampleSizeCode == 7) return ERR_FLAC_BITS_PER_SAMPLE_UNKNOWN;
    if(!FLACMetadataBlock->numChannels){
        if(FLACFrameHeader->chanAsgn == 0) FLACMetadataBlock->numChannels = 1;
        if(FLACFrameHeader->chanAsgn == 1) FLACMetadataBlock->numChannels = 2;
        if(FLACFrameHeader->chanAsgn > 7)  FLACMetadataBlock->numChannels = 2;
    }
    if(FLACMetadataBlock->numChannels < 1) return ERR_FLAC_UNKNOWN_CHANNEL_ASSIGNMENT;
    if(!FLACMetadataBlock->bitsPerSample){
        if(FLACFrameHeader->sampleSizeCode == 1) FLACMetadataBlock->bitsPerSample =  8;
        if(FLACFrameHeader->sampleSizeCode == 2) FLACMetadataBlock->bitsPerSample = 12;
        if(FLACFrameHeader->sampleSizeCode == 4) FLACMetadataBlock->bitsPerSample = 16;
        if(FLACFrameHeader->sampleSizeCode == 5) FLACMetadataBlock->bitsPerSample = 20;
        if(FLACFrameHeader->sampleSizeCode == 6) FLACMetadataBlock->bitsPerSample = 24;
    }
    /* 24-bit is accepted and narrowed at the write-out below, not refused.  mscp4.live-streams.nl's lounge.ogg is a
       24-bit Ogg FLAC stream, and the VS1053's FLAC plugin plays it (that path never looks at a sample depth), so
       this decoder was the only thing that could not.  It has no reason to refuse either: the sample buffers are
       int32_t and decodeSubframe()/the prediction path already take the depth as a parameter, so 24-bit frames
       decode exactly - only the read-out into the 16-bit output narrows.  Above 24 is still refused. */
    if(FLACMetadataBlock->bitsPerSample > 24) return ERR_FLAC_BITS_PER_SAMPLE_TOO_BIG;
    if(FLACMetadataBlock->bitsPerSample < 8 ) return ERR_FLAC_BITS_PER_SAMPLE_UNKNOWN;
    if(!FLACMetadataBlock->sampleRate){
        if(FLACFrameHeader->sampleRateCode == 1)  FLACMetadataBlock->sampleRate =  88200;
        if(FLACFrameHeader->sampleRateCode == 2)  FLACMetadataBlock->sampleRate = 176400;
        if(FLACFrameHeader->sampleRateCode == 3)  FLACMetadataBlock->sampleRate = 192000;
        if(FLACFrameHeader->sampleRateCode == 4)  FLACMetadataBlock->sampleRate =   8000;
        if(FLACFrameHeader->sampleRateCode == 5)  FLACMetadataBlock->sampleRate =  16000;
        if(FLACFrameHeader->sampleRateCode == 6)  FLACMetadataBlock->sampleRate =  22050;
        if(FLACFrameHeader->sampleRateCode == 7)  FLACMetadataBlock->sampleRate =  24000;
        if(FLACFrameHeader->sampleRateCode == 8)  FLACMetadataBlock->sampleRate =  32000;
        if(FLACFrameHeader->sampleRateCode == 9)  FLACMetadataBlock->sampleRate =  44100;
        if(FLACFrameHeader->sampleRateCode == 10) FLACMetadataBlock->sampleRate =  48000;
        if(FLACFrameHeader->sampleRateCode == 11) FLACMetadataBlock->sampleRate =  96000;
    }
    readUint(1, bytesLeft);
    uint32_t temp = (readUint(8, bytesLeft) << 24);
    temp = ~temp;
    uint32_t shift = 0x80000000; // Number of leading zeros
    int8_t count = 0;
    for(int32_t i=0; i<32; i++){
        if((temp & shift) == 0) {count++; shift >>= 1;}
        else break;
    }
    count--;
    for (int32_t i = 0; i < count; i++) readUint(8, bytesLeft);
    s_numOfOutSamples = 0;
    if (FLACFrameHeader->blockSizeCode == 1)
        s_numOfOutSamples = 192;
    else if (2 <= FLACFrameHeader->blockSizeCode && FLACFrameHeader->blockSizeCode <= 5)
        s_numOfOutSamples = 576 << (FLACFrameHeader->blockSizeCode - 2);
    else if (FLACFrameHeader->blockSizeCode == 6)
        s_numOfOutSamples = readUint(8, bytesLeft) + 1;
    else if (FLACFrameHeader->blockSizeCode == 7)
        s_numOfOutSamples = readUint(16, bytesLeft) + 1;
    else if (8 <= FLACFrameHeader->blockSizeCode && FLACFrameHeader->blockSizeCode <= 15)
        s_numOfOutSamples = 256 << (FLACFrameHeader->blockSizeCode - 8);
    else{
        return ERR_FLAC_RESERVED_BLOCKSIZE_UNSUPPORTED;
    }
    if(s_numOfOutSamples > MAX_OUTBUFFSIZE){
        log_e("Error: blockSizeOut too big ,%i bytes", s_numOfOutSamples);
        return ERR_FLAC_BLOCKSIZE_TOO_BIG;
    }
    if(FLACFrameHeader->sampleRateCode == 12)
        readUint(8, bytesLeft);
    else if (FLACFrameHeader->sampleRateCode == 13 || FLACFrameHeader->sampleRateCode == 14){
        readUint(16, bytesLeft);
    }
    readUint(8, bytesLeft);
    s_flacStatus = DECODE_SUBFRAMES;
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetOutputSamps(){
    uint32_t vs = s_flacValidSamples;
    s_flacValidSamples=0;
    return vs;
}
//----------------------------------------------------------------------------------------------------------------------
// Samples per channel of the frame just decoded.  The geometry line in the i2s report needs it to say whether a
// stream is 4096-block or something smaller, without anyone having to guess from a frame count and a rate.
uint16_t FLACGetBlockSize(){
    return s_numOfOutSamples;
}
//----------------------------------------------------------------------------------------------------------------------
uint64_t FLACGetTotoalSamplesInStream(){
    if(!FLACMetadataBlock) return 0;
    return FLACMetadataBlock->totalSamples;
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t FLACGetBitsPerSample(){
    if(!FLACMetadataBlock) return 0;
    /* The depth the OUTPUT samples have, not the depth the stream declares.  24-bit frames are shifted down on the way
       out, so publishing 24 here would make setBitsPerSample() refuse the stream outright (it accepts 8 and 16) and
       make the rest of the library treat the output as one byte per sample. */
    return FLACMetadataBlock->bitsPerSample > 16 ? 16 : FLACMetadataBlock->bitsPerSample;
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t FLACGetChannels(){
    if(!FLACMetadataBlock) return 0;
    return FLACMetadataBlock->numChannels;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetSampRate(){
    if(!FLACMetadataBlock) return 0;
    return FLACMetadataBlock->sampleRate;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetBitRate(){
    return s_flacBitrate;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetAudioDataStart(){
    return s_flacAudioDataStart;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetAudioFileDuration() {
    if(FLACGetSampRate()){ // DIV0
        uint32_t afd = FLACGetTotoalSamplesInStream()/ FLACGetSampRate(); // AudioFileDuration
        return afd;
    }
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeSubframes(int32_t* bytesLeft){
    if(FLACFrameHeader->chanAsgn <= 7) {
        for (int32_t ch = 0; ch < FLACMetadataBlock->numChannels; ch++)
            decodeSubframe(FLACMetadataBlock->bitsPerSample, ch, bytesLeft);
    }
    else if (8 <= FLACFrameHeader->chanAsgn && FLACFrameHeader->chanAsgn <= 10) {
        decodeSubframe(FLACMetadataBlock->bitsPerSample + (FLACFrameHeader->chanAsgn == 9 ? 1 : 0), 0, bytesLeft);
        decodeSubframe(FLACMetadataBlock->bitsPerSample + (FLACFrameHeader->chanAsgn == 9 ? 0 : 1), 1, bytesLeft);
        if(FLACFrameHeader->chanAsgn == 8) {
            for (int32_t i = 0; i < s_numOfOutSamples; i++)
                s_samplesBuffer[1][i] = (
                        s_samplesBuffer[0][i] -
                        s_samplesBuffer[1][i]);
        }
        else if (FLACFrameHeader->chanAsgn == 9) {
            for (int32_t i = 0; i < s_numOfOutSamples; i++)
                s_samplesBuffer[0][i] += s_samplesBuffer[1][i];
        }
        else if (FLACFrameHeader->chanAsgn == 10) {
            for (int32_t i = 0; i < s_numOfOutSamples; i++) {
                int32_t side =  s_samplesBuffer[1][i];
                int32_t right = s_samplesBuffer[0][i] - (side >> 1);
                s_samplesBuffer[1][i] = right;
                s_samplesBuffer[0][i] = right + side;
            }
        }
        else {
            log_e("unknown channel assignment, %i", FLACFrameHeader->chanAsgn);
            return ERR_FLAC_UNKNOWN_CHANNEL_ASSIGNMENT;
        }
    }
    else{
        log_e("Reserved channel assignment, %i", FLACFrameHeader->chanAsgn);
        return ERR_FLAC_RESERVED_CHANNEL_ASSIGNMENT;
    }
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeSubframe(uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft) {
    int8_t ret = 0;
    readUint(1, bytesLeft);                // Zero bit padding, to prevent sync-fooling string of 1s
    uint8_t type = readUint(6, bytesLeft); // Subframe type: 000000 : SUBFRAME_CONSTANT
                                           //                000001 : SUBFRAME_VERBATIM
                                           //                00001x : reserved
                                           //                0001xx : reserved
                                           //                001xxx : if(xxx <= 4) SUBFRAME_FIXED, xxx=order ; else reserved
                                           //                01xxxx : reserved
                                           //                1xxxxx : SUBFRAME_LPC, xxxxx=order-1

    int32_t shift = readUint(1, bytesLeft);    // Wasted bits-per-sample' flag:
                                           // 0 : no wasted bits-per-sample in source subblock, k=0
                                           // 1 : k wasted bits-per-sample in source subblock, k-1 follows, unary coded; e.g. k=3 => 001 follows, k=7 => 0000001 follows.
    if (shift == 1) {
        while (readUint(1, bytesLeft) == 0) { shift++;}
    }
    sampleDepth -= shift;

    if(type == 0){  // Constant coding
        int32_t s= readSignedInt(sampleDepth, bytesLeft);                                    // SUBFRAME_CONSTANT
        for(int32_t i = 0; i < s_numOfOutSamples; i++){
            s_samplesBuffer[ch][i] = s;
        }
    }
    else if (type == 1) {  // Verbatim coding
        for (int32_t i = 0; i < s_numOfOutSamples; i++)
            s_samplesBuffer[ch][i] = readSignedInt(sampleDepth, bytesLeft);                  // SUBFRAME_VERBATIM
    }
    else if (8 <= type && type <= 12){
        ret = decodeFixedPredictionSubframe(type - 8, sampleDepth, ch, bytesLeft);           // SUBFRAME_FIXED
        if(ret) return ret;
    }
    else if (32 <= type && type <= 63){
        ret = decodeLinearPredictiveCodingSubframe(type - 31, sampleDepth, ch, bytesLeft);   // SUBFRAME_LPC
        if(ret) return ret;
    }
    else{
        return ERR_FLAC_RESERVED_SUB_TYPE;
    }
    if(shift>0){
        for (int32_t i = 0; i < s_numOfOutSamples; i++){
            s_samplesBuffer[ch][i] <<= shift;
        }
    }
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t decodeFixedPredictionSubframe(uint8_t predOrder, uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft) {     // SUBFRAME_FIXED

    uint8_t ret = 0;
    for(uint8_t i = 0; i < predOrder; i++)
        s_samplesBuffer[ch][i] = readSignedInt(sampleDepth, bytesLeft); // Unencoded warm-up samples (n = frame's bits-per-sample * predictor order).
    ret = decodeResiduals(predOrder, ch, bytesLeft);
    if(ret) return ret;
    coefs.clear(); coefs.shrink_to_fit();
    if(predOrder == 0) coefs.resize(0);
    if(predOrder == 1) coefs.push_back(1);  // FIXED_PREDICTION_COEFFICIENTS
    if(predOrder == 2){coefs.push_back(2); coefs.push_back(-1);}
    if(predOrder == 3){coefs.push_back(3); coefs.push_back(-3); coefs.push_back(1);}
    if(predOrder == 4){coefs.push_back(4); coefs.push_back(-6); coefs.push_back(4); coefs.push_back(-1);}
    if(predOrder > 4) return ERR_FLAC_PREORDER_TOO_BIG; // Error: preorder > 4"
    restoreLinearPrediction(ch, 0);
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeLinearPredictiveCodingSubframe(int32_t lpcOrder, int32_t sampleDepth, uint8_t ch, int32_t* bytesLeft){

    int8_t ret = 0;
    for (int32_t i = 0; i < lpcOrder; i++){
        s_samplesBuffer[ch][i] = readSignedInt(sampleDepth, bytesLeft); // Unencoded warm-up samples (n = frame's bits-per-sample * lpc order).
    }
    int32_t precision = readUint(4, bytesLeft) + 1;                         // (Quantized linear predictor coefficients' precision in bits)-1 (1111 = invalid).
    int32_t shift = readSignedInt(5, bytesLeft);                            // Quantized linear predictor coefficient shift needed in bits (NOTE: this number is signed two's-complement).
    coefs.clear(); coefs.shrink_to_fit();
    for (uint8_t i = 0; i < lpcOrder; i++){
        coefs.push_back(readSignedInt(precision, bytesLeft));           // Unencoded predictor coefficients (n = qlp coeff precision * lpc order) (NOTE: the coefficients are signed two's-complement).
    }
    ret = decodeResiduals(lpcOrder, ch, bytesLeft);
    if(ret) return ret;
    restoreLinearPrediction(ch, shift);
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeResiduals(uint8_t warmup, uint8_t ch, int32_t* bytesLeft) {

    int32_t method = readUint(2, bytesLeft);                          // Residual coding method:
                                                                  // 00 : partitioned Rice coding with 4-bit Rice parameter; RESIDUAL_CODING_METHOD_PARTITIONED_RICE follows
                                                                  // 01 : partitioned Rice coding with 5-bit Rice parameter; RESIDUAL_CODING_METHOD_PARTITIONED_RICE2 follows
                                                                  // 10-11 : reserved
    if (method >= 2) {return ERR_FLAC_RESERVED_RESIDUAL_CODING;}
    uint8_t paramBits = method == 0 ? 4 : 5;                      // RESIDUAL_CODING_METHOD_PARTITIONED_RICE || RESIDUAL_CODING_METHOD_PARTITIONED_RICE2
    int32_t escapeParam = ( method == 0 ? 0xF : 0x1F);
    int32_t partitionOrder = readUint(4, bytesLeft);                  // Partition order
    int32_t numPartitions = 1 << partitionOrder;                      // There will be 2^order partitions.

    if (s_numOfOutSamples % numPartitions != 0){
        return ERR_FLAC_WRONG_RICE_PARTITION_NR;                  //Error: Block size not divisible by number of Rice partitions
    }
    int32_t partitionSize = s_numOfOutSamples / numPartitions;

    for (int32_t i = 0; i < numPartitions; i++) {
        int32_t start = i * partitionSize + (i == 0 ? warmup : 0);
        int32_t end = (i + 1) * partitionSize;

        int32_t param = readUint(paramBits, bytesLeft);
        if (param < escapeParam) {
            #if FLAC_PROFILE
                const uint32_t pf0 = (uint32_t)micros();
            #endif
            for (int32_t j = start; j < end; j++){
                if(s_f_bitReaderError) break;
                s_samplesBuffer[ch][j] = readRiceSignedInt(param, bytesLeft);
            }
            #if FLAC_PROFILE
                s_pfResidUs += (uint32_t)(micros() - pf0);
            #endif
        }
        else {
            int32_t numBits = readUint(5, bytesLeft);                 // Escape code, meaning the partition is in unencoded binary form using n bits per sample; n follows as a 5-bit number.
            for (int32_t j = start; j < end; j++){
                if(s_f_bitReaderError) break;
                s_samplesBuffer[ch][j] = readSignedInt(numBits, bytesLeft);
            }
        }
    }
    if(s_f_bitReaderError) return ERR_FLAC_BITREADER_UNDERFLOW;
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
void restoreLinearPrediction(uint8_t ch, uint8_t shift) {
    #if FLAC_PROFILE
        const uint32_t pf0 = (uint32_t)micros();
    #endif
    for (int32_t i = coefs.size(); i < s_numOfOutSamples; i++) {
        int32_t sum = 0;
        for (int32_t j = 0; j < coefs.size(); j++){
            sum += s_samplesBuffer[ch][i - 1 - j] * coefs[j];
        }
        s_samplesBuffer[ch][i] += (sum >> shift);
    }
    #if FLAC_PROFILE
        s_pfLpcUs += (uint32_t)(micros() - pf0);
    #endif
}
//----------------------------------------------------------------------------------------------------------------------
int32_t FLAC_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact){
    int32_t result = 0;  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int32_t i = 0; i < baselen - strlen(str); i++){
        result = i;
        for (int32_t j = 0; j < strlen(str) + exact; j++){
            if (*(base + i + j) != *(str + j)){
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
//----------------------------------------------------------------------------------------------------------------------
char* flac_x_ps_malloc(uint16_t len) {
    char* ps_str = NULL;
    if(psramFound()){ps_str = (char*) ps_malloc(len);}
    else            {ps_str = (char*)    malloc(len);}
    return ps_str;
}
//----------------------------------------------------------------------------------------------------------------------
char* flac_x_ps_calloc(uint16_t len, uint8_t size) {
    char* ps_str = NULL;
    if(psramFound()){ps_str = (char*) ps_calloc(len, size);}
    else            {ps_str = (char*)    calloc(len, size);}
    return ps_str;
}
//----------------------------------------------------------------------------------------------------------------------
char* flac_x_ps_strdup(const char* str) {
    char* ps_str = NULL;
    if(psramFound()) { ps_str = (char*)ps_calloc(strlen(str) + 1, sizeof(char)); }
    else { ps_str = (char*)calloc(strlen(str) + 1,  sizeof(char)); }
    strcpy(ps_str, str);
    return ps_str;
}
//----------------------------------------------------------------------------------------------------------------------
char* flac_x_ps_strndup(const char* str, uint16_t n) {
    char* ps_str = NULL;
    if(psramFound()) { ps_str = (char*)ps_calloc(n + 1, sizeof(char)); }
    else { ps_str = (char*)calloc(n + 1,  sizeof(char)); }
    strncpy(ps_str, str, n);
    return ps_str;
}
//----------------------------------------------------------------------------------------------------------------------
