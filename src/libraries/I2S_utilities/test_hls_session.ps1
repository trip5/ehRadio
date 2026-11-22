# Does an scastream HLS chain need the session cookie on its playlist re-polls?
#
# The device's sequence is: entry playlist -> child playlist (with listeningSessionID) -> segment, and then it polls
# the playlist again for new segments.  That last poll is where it got a 404.  Plain curl fetches of the entry
# answered 200 each time, so the question this answers is whether the RE-POLL (after a segment has been consumed, with
# the ListenerInfo cookie the server sets) is the one that fails - and whether the cookie changes that.
#
# Transfers are done by curl.exe: PowerShell 5.1's Invoke-WebRequest returns a null .Content on this response.  Read-only.

$ErrorActionPreference = 'Continue'
$ua    = 'ehRadio/2026.11.09'
$entry = 'http://sa46.scastream.com.au/live/7noughties_128.stream/playlist.m3u8'
$jar   = Join-Path $env:TEMP 'ehradio_hls_jar.txt'
$body  = Join-Path $env:TEMP 'ehradio_hls_master.txt'
$body2 = Join-Path $env:TEMP 'ehradio_hls_media.txt'
Remove-Item $jar, $body, $body2 -ErrorAction SilentlyContinue

function Fetch($url, $out, $label, $useJar) {
    if ($useJar) {
        $code = & curl.exe -sS -A $ua -c $jar -b $jar -o $out -w "%{http_code}" --max-time 20 $url
    } else {
        $code = & curl.exe -sS -A $ua -o $out -w "%{http_code}" --max-time 20 $url
    }
    $len = 0
    if (Test-Path $out) { $len = (Get-Item $out).Length }
    "${label}: $code ($len bytes)"
}

'--- 1. entry playlist, new session (cookie jar written) ---'
Fetch $entry $body 'entry' $true

$master = Get-Content $body -Raw
# .Trim() matters: the playlist lines end CRLF and curl rejects a URL carrying a stray CR.
$child  = ((($master -split "`n") | Where-Object { $_ -match 'playlist\.m3u8\?' } | Select-Object -First 1)).Trim()
"child: $child"

'--- 2. child playlist, same session ---'
Fetch $child $body2 'child' $true

$media = Get-Content $body2 -Raw
$seg   = ((($media -split "`n") | Where-Object { $_ -match '^https?://' } | Select-Object -First 1)).Trim()
"segment: $seg"

'--- 3. one segment, same session ---'
$null = & curl.exe -sS -A $ua -c $jar -b $jar -o NUL -w "segment: %{http_code} (%{size_download} bytes)`n" --max-time 20 $seg

'--- 4. re-poll the child, WITH the session cookies ---'
$c1 = & curl.exe -sS -A $ua -c $jar -b $jar -o NUL -w "%{http_code}" --max-time 20 $child
"child with cookies:    $c1"

'--- 5. re-poll the child, WITHOUT any cookies ---'
$c2 = & curl.exe -sS -A $ua -o NUL -w "%{http_code}" --max-time 20 $child
"child without cookies: $c2"

'--- 6. re-poll the entry, WITHOUT any cookies ---'
$c3 = & curl.exe -sS -A $ua -o NUL -w "%{http_code}" --max-time 20 $entry
"entry without cookies: $c3"

'--- jar contents ---'
if (Test-Path $jar) { Get-Content $jar | Where-Object { $_ -notmatch '^#' } }

'--- verdict ---'
'4=200 and 5=404 -> the re-poll needs the cookie: a minimal cookie store is required.'
'4=200 and 5=200 -> the cookie is irrelevant and the device failure is on our side of the request.'
'4=404 and 5=404 -> the session id is one-shot: re-fetch the entry and use the new child each time.'
