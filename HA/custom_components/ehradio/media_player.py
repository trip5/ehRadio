import logging
import voluptuous as vol
import json
import urllib.request
import asyncio

from homeassistant.components import mqtt, media_source
from homeassistant.components.media_player.browse_media import async_process_play_media_url
from homeassistant.const import CONF_NAME
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.device_registry import DeviceInfo

from homeassistant.components.media_player import (
    PLATFORM_SCHEMA as MEDIA_PLAYER_PLATFORM_SCHEMA,
    BrowseMedia,
    MediaPlayerEntity,
    MediaPlayerEntityFeature,
    MediaPlayerState,
    MediaPlayerEnqueue,
    MediaType,
    RepeatMode,
)

VERSION = '2026.09.22'

_LOGGER      = logging.getLogger(__name__)

SUPPORT_EHRADIO = (
    MediaPlayerEntityFeature.PAUSE
    | MediaPlayerEntityFeature.PLAY
    | MediaPlayerEntityFeature.STOP
    | MediaPlayerEntityFeature.VOLUME_SET
    | MediaPlayerEntityFeature.VOLUME_STEP
    | MediaPlayerEntityFeature.TURN_OFF
    | MediaPlayerEntityFeature.TURN_ON
    
    | MediaPlayerEntityFeature.PREVIOUS_TRACK
    | MediaPlayerEntityFeature.NEXT_TRACK
    | MediaPlayerEntityFeature.SELECT_SOURCE
    | MediaPlayerEntityFeature.BROWSE_MEDIA
    | MediaPlayerEntityFeature.PLAY_MEDIA
)

DEFAULT_NAME = 'myradio'
CONF_MAX_VOLUME = 'max_volume'
CONF_ROOT_TOPIC = 'root_topic'
CONF_FALLBACK_IMAGE = 'fallback_image'
CONF_DEVICE_URL = 'device_url'
DEFAULT_FALLBACK_IMAGE = 'https://trip5.github.io/ehRadio/images/logo-color-square.png'

MEDIA_PLAYER_PLATFORM_SCHEMA = MEDIA_PLAYER_PLATFORM_SCHEMA.extend({
  vol.Required(CONF_ROOT_TOPIC, default="ehradio"): cv.string,
  vol.Optional(CONF_NAME, default=DEFAULT_NAME): cv.string,
  vol.Optional(CONF_MAX_VOLUME, default='42'): cv.string,
  vol.Optional(CONF_FALLBACK_IMAGE, default=DEFAULT_FALLBACK_IMAGE): cv.string,
  vol.Optional(CONF_DEVICE_URL, default=''): cv.string
})

# <root>/state carries one bare token in Home Assistant's own vocabulary; the device never localizes it.
STATE_TOKEN_MAP = {
  'off':       MediaPlayerState.STANDBY,
  'playing':   MediaPlayerState.PLAYING,
  'idle':      MediaPlayerState.IDLE,
  'buffering': MediaPlayerState.BUFFERING,
}

# <root>/availability is retained, and the device's last will publishes the offline half of it.
AVAILABILITY_OFFLINE = 'offline'

# Both station lists are served over HTTP; <root>/mode says which one is live, and <root>/ip where it is.
PLAYLIST_PATH_WEB = '/data/playlist.csv'
PLAYLIST_PATH_SD  = '/data/playlistsd.csv'

def setup_platform(hass, config, add_devices, discovery_info=None):
  root_topic = config.get(CONF_ROOT_TOPIC)
  name = config.get(CONF_NAME)
  max_volume = int(config.get(CONF_MAX_VOLUME, 42))
  fallback_image = config.get(CONF_FALLBACK_IMAGE, DEFAULT_FALLBACK_IMAGE)
  device_url = config.get(CONF_DEVICE_URL, '')
  playlist = []
  api = ehradioApi(root_topic, hass, playlist, device_url)
  add_devices([ehradioDevice(name, max_volume, fallback_image, device_url, api)], True)

class ehradioApi():
  def __init__(self, root_topic, hass, playlist, device_url):
    self.hass = hass
    self.mqtt = mqtt
    self.root_topic = root_topic.strip('/')
    self.playlist = playlist
    self.playlisturl = ""
    self.device_url = device_url  # user-configured; otherwise built from the ip topic
    self.ip = ""
    self.mode = 0                 # 0 web radio, 1 SD card, as published on the mode topic

  def base_url(self):
    if self.device_url:
      return self.device_url
    if self.ip:
      return f"http://{self.ip}/"
    return ""

  def playlist_path(self):
    return PLAYLIST_PATH_SD if self.mode else PLAYLIST_PATH_WEB

  async def set_command(self, command):
    try:
      self.mqtt.async_publish(self.root_topic + '/command', command)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', command)

  async def set_volume(self, volume):
    command = "vol " + str(volume)
    try:
      self.mqtt.async_publish(self.root_topic + '/command', command)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', command)
      
  def fetch_data(self):
    try:
      html = urllib.request.urlopen(self.playlisturl).read().decode("utf-8")
      return str(html)
    except Exception as e:
      _LOGGER.error(f"Unable to fetch playlist from {self.playlisturl}: " + str(e))
      return ""
        
  async def set_source(self, source):
    number = source.split('.')
    command = "play " + number[0]
    try:
      self.mqtt.async_publish(self.root_topic + '/command', command)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', command)

  async def set_browse_media(self, media_content_id):
    try:
      self.mqtt.async_publish(self.root_topic + '/command', media_content_id)
    except:
      await self.mqtt.async_publish(self.hass, self.root_topic + '/command', media_content_id)
      
  async def load_playlist(self):
    base = self.base_url()
    if not base:
      _LOGGER.warning("Playlist not fetched yet: no ip topic received and no device_url configured")
      return
    self.playlisturl = base.rstrip('/') + self.playlist_path()
    file = await self.hass.async_add_executor_job(self.fetch_data)
    if not file:
      return  # keep the station list we already have rather than clearing it on a transient failure
    counter = 1
    self.playlist.clear()
    for line in file.split('\n'):
      res = line.split('\t')
      if res[0] != "":
        station = str(counter) + '. ' + res[0]
        self.playlist.append(station)
        counter=counter+1

class ehradioDevice(MediaPlayerEntity):
  def __init__(self, name, max_volume, fallback_image, device_url, api):
    self._name = name
    self.api = api
    self._state = MediaPlayerState.OFF
    self._current_source = None
    self._media_title = ''
    self._track_artist = ''
    self._track_album_name = ''
    self._entity_picture = None
    self._volume = 0
    self._max_volume = max_volume
    self._fallback_image = fallback_image
    self._device_url = device_url
    self._available = True      # optimistic until the availability topic says otherwise
    self._revision = ''

  @property
  def device_info(self) -> DeviceInfo:
    device_url = self.api.base_url() or self._device_url
    return DeviceInfo(
        identifiers={("ehradio", self._name)},
        name=self._name,
        manufacturer="ehRadio",
        model="ESP32 Internet Radio",
        configuration_url=device_url if device_url else None,
    )

  @property
  def available(self):
    return self._available

  def _schedule_update(self):
    try:
      self.async_schedule_update_ha_state()
    except Exception:
      pass

  async def refresh_playlist(self):
    await self.api.load_playlist()

  async def async_added_to_hass(self):
    await asyncio.sleep(5)
    root = self.api.root_topic
    await mqtt.async_subscribe(self.api.hass, root + '/availability', self.availability_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, root + '/state', self.state_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, root + '/status', self.status_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, root + '/volume', self.volume_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, root + '/mode', self.mode_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, root + '/ip', self.ip_listener, 0, "utf-8")
    await mqtt.async_subscribe(self.api.hass, root + '/playlist', self.playlist_listener, 0, "utf-8")

  async def availability_listener(self, msg):
    self._available = (msg.payload != AVAILABILITY_OFFLINE)
    if self._available:
      await self.refresh_playlist()  # a fresh session may have come up with a new address or mode
    self._schedule_update()

  async def state_listener(self, msg):
    state = STATE_TOKEN_MAP.get(msg.payload)
    if state is not None:
      self._state = state
      self._schedule_update()

  async def status_listener(self, msg):
    try:
      js = json.loads(msg.payload)
    except Exception as e:
      _LOGGER.error("Unable to parse the status payload: " + str(e))
      return
    station_name = js.get('name', '')
    track_title = js.get('title', '')
    self._media_title = station_name or track_title
    self._track_artist = track_title if station_name else None
    self._current_source = f"{js.get('station', 0)}. {station_name}"
    self._entity_picture = js.get('image_url') or None
    if js.get('max_volume'):
      self._max_volume = int(js['max_volume'])  # the device knows its own VOLUME_SCALE
    self._schedule_update()

  async def volume_listener(self, msg):
    try:
      self._volume = int(msg.payload) / self._max_volume
    except (TypeError, ValueError, ZeroDivisionError):
      return
    self._schedule_update()

  async def mode_listener(self, msg):
    try:
      mode = int(msg.payload)
    except (TypeError, ValueError):
      return
    if mode == self.api.mode:
      return
    self.api.mode = mode
    await self.refresh_playlist()  # the active station list is a different file on the SD card
    self._schedule_update()

  async def ip_listener(self, msg):
    ip = str(msg.payload).strip()
    if not ip or ip == self.api.ip:
      return
    self.api.ip = ip
    await self.refresh_playlist()
    self._schedule_update()

  async def playlist_listener(self, msg):
    revision = str(msg.payload)
    if revision == self._revision:
      return
    self._revision = revision
    await self.refresh_playlist()
    self._schedule_update()

  @property
  def supported_features(self):
    return SUPPORT_EHRADIO

  @property
  def name(self):
    return self._name

  @property
  def media_title(self):
    return self._media_title

  @property
  def media_content_type(self):
    return MediaType.MUSIC

  @property
  def media_artist(self):
    return self._track_artist

  @property
  def entity_picture(self):
    return self._entity_picture if self._entity_picture else self._fallback_image

  @property
  def media_image_remotely_accessible(self):
    return True

  @property
  def unique_id(self):
    return f"ehradio_{self._name}"

  @property
  def extra_state_attributes(self):
    attrs = {}
    device_url = self.api.base_url() or self._device_url
    if device_url:
        attrs["device_url"] = device_url
    attrs["playlist_source"] = "SD card" if self.api.mode else "Web radio"
    return attrs

  @property
  def state(self):
    return self._state

  @property
  def volume_level(self):
    return self._volume

  async def async_set_volume_level(self, volume):
    await self.api.set_volume(round(volume * self._max_volume,1))

  @property
  def source(self):
    return self._current_source

  @property
  def source_list(self):
    return self.api.playlist

  async def async_browse_media(
    self, media_content_type: str | None = None, media_content_id: str | None = None
  ) -> BrowseMedia:
    return await media_source.async_browse_media(
      self.hass,
      media_content_id,
    )

  async def async_play_media(
    self,
    media_type: str,
    media_id: str,
    enqueue: MediaPlayerEnqueue | None = None,
    announce: bool | None = None, **kwargs
  ) -> None:
    if media_source.is_media_source_id(media_id):
      media_type = MediaType.URL
      play_item = await media_source.async_resolve_media(self.hass, media_id, self.entity_id)
      media_id = async_process_play_media_url(self.hass, play_item.url)
    await self.api.set_browse_media(media_id)
    
  async def async_select_source(self, source):
    await self.api.set_source(source)
    self._current_source = source

  async def async_volume_up(self):
      newVol = float(self._volume) + 0.05
      await self.async_set_volume_level(newVol)
      self._volume = newVol

  async def async_volume_down(self):
      newVol = float(self._volume) - 0.05
      await self.async_set_volume_level(newVol)
      self._volume = newVol

  async def async_media_next_track(self):
      await self.api.set_command("next")

  async def async_media_previous_track(self):
      await self.api.set_command("prev")

  async def async_media_stop(self):
      await self.api.set_command("stop")
      self._state = MediaPlayerState.IDLE

  async def async_media_play(self):
      await self.api.set_command("start")
      self._state = MediaPlayerState.PLAYING

  async def async_media_pause(self):
      await self.api.set_command("stop")
      self._state = MediaPlayerState.IDLE
  
  async def async_turn_off(self):
      # The radio has no power switch: "off" means entering standby, and the state topic confirms it.
      await self.api.set_command("startstandby")
      self._state = MediaPlayerState.STANDBY

  async def async_turn_on(self, **kwargs):
      await self.api.set_command("stopstandby")
      self._state = MediaPlayerState.IDLE
