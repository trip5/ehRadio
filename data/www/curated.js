const gateway = `ws://${window.location.hostname}/ws`;
let websocket;
let indexData = [];
let currentPlaylistData = null;
let currentPlaylistFile = '';
let loadedTimeout = null;

window.addEventListener('load', onCuratedLoad);

function onCuratedLoad(event) {
  // Set curated source info if available
  if (typeof curatedName !== 'undefined' && curatedName) {
    document.getElementById('curatedBy').innerHTML = `<a target="_blank" href="${curatedLink || '#'}">${escapeHtml(curatedName)}</a>`;
  }

  // Event listeners
  //document.getElementById('loadListsBtn').addEventListener('click', loadCuratedIndex);
  document.getElementById('replaceBtn').addEventListener('click', () => importPlaylist('replace'));
  document.getElementById('mergeBtn').addEventListener('click', () => importPlaylist('merge'));
  //document.getElementById('backToIndexBtn').addEventListener('click', backToIndex);
  document.getElementById('curateddone').addEventListener('click', () => { window.location.href = '/'; });

  // Initialize WebSocket
  initCuratedWebSocket();
}

function initCuratedWebSocket() {
  console.log('[Curated] Opening WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onCuratedWSOpen;
  websocket.onclose = onCuratedWSClose;
  websocket.onmessage = onCuratedWSMessage;
}

function onCuratedWSOpen(event) {
  console.log('[Curated] WebSocket connection opened');
}

function onCuratedWSClose(event) {
  console.log('[Curated] WebSocket connection closed');
  setTimeout(initCuratedWebSocket, 2000);
}

function onCuratedWSMessage(event) {
  try {
    const data = JSON.parse(event.data);
    
    if (data.curated_index_done) {
      console.log('[Curated] Index download complete, fetching index...');
      fetchIndex();
    } else if (data.curated_playlist_done) {
      console.log('[Curated] Playlist download complete, fetching playlist...');
      fetchPlaylist();
    } else if (data.curated_failed) {
      console.error('[Curated] Download failed');
      showLoadStatus('Failed to download. Check network connection.', true);
      enableLoadButton();
    } else if (data.curated_ready) {
      console.log('[Curated] Playlist ready for review, mode:', data.mode);
      // Store the mode for the editor
      sessionStorage.setItem('pl_import_mode', data.mode);
      sessionStorage.setItem('pl_import_review', 'true');
      // Redirect to player to open editor
      window.location.replace('/');
    } else if (data.curated_import_failed) {
      console.error('[Curated] Import failed');
      alert(t('msg_failed_prepare_playlist', 'Failed to prepare playlist. Please try again.'));
    }
  } catch (e) {
    console.error('[Curated] Error parsing WebSocket message:', e);
  }
}

function loadCuratedIndex(button) {
  // Only allow if button is in clickable state
  const currentState = button.value;
  if (currentState === 'Loading...' || currentState === 'Loaded') {
    return; // Do nothing if loading or loaded
  }
  
  console.log('[Curated] Requesting index download...');
  console.log('[Curated] WebSocket ready state:', websocket.readyState);
  
  setButtonState('loading');
  
  // Show loading spinner in index table
  const indexTable = document.getElementById('indexTable');
  indexTable.innerHTML = `
    <tr style="height: 100%;">
      <td colspan="3" style="height: 100%; text-align: center; vertical-align: middle;">
        <span id="loader"></span>
      </td>
    </tr>`;
  
  // Send WebSocket command to trigger download
  console.log('[Curated] Sending command: loadindex=');
  websocket.send('loadindex=');
  console.log('[Curated] Command sent');
}

function fetchIndex() {
  fetch('curated.json?t=' + Date.now())
    .then(response => {
      if (!response.ok) throw new Error('Failed to fetch index');
      return response.json();
    })
    .then(data => {
      indexData = data || [];
      displayIndex();
      setButtonState('loaded');
      
      // After 15 seconds, change to Reload
      if (loadedTimeout) clearTimeout(loadedTimeout);
      loadedTimeout = setTimeout(() => {
        setButtonState('reload');
      }, 15000);
    })
    .catch(error => {
      console.error('[Curated] Error fetching index:', error);
      alert(t('msg_error_loading_index', 'Error loading index file. Please try again.'));
      setButtonState('load');
    });
}

function displayIndex() {
  const indexTable = document.getElementById('indexTable');
  indexTable.innerHTML = '';
  
  if (!indexData || indexData.length === 0) {
    indexTable.innerHTML = '<tr class="line"><td colspan="3" class="importantmessage">' + t('msg_no_playlists', 'No playlists available') + '</td></tr>';
    return;
  }
  
  indexData.forEach((playlist, idx) => {
    const row = document.createElement('tr');
    row.className = 'line';
    row.innerHTML = `
      <td class="name">${escapeHtml(playlist.name || t('lbl_unnamed', 'Unnamed'))}</td>
      <td class="info">${escapeHtml(playlist.total ? playlist.total + ' ' + t('msg_stations', 'stations') : '')}</td>
      <td class="add">
        <button class="searchbutton addtoplaylist" data-index="${idx}" data-action="load">
          <svg viewBox="0 0 24 24" class="stroke">
            <path d="m19 14-7 7-7-7m7 7V3"/>
          </svg>
        </button>
      </td>
    `;
    indexTable.appendChild(row);
  });
  
  // Add click handler for Load buttons
  indexTable.addEventListener('click', (event) => {
    const button = event.target.closest('button[data-action="load"]');
    if (button) {
      const index = parseInt(button.dataset.index);
      loadPlaylist(indexData[index]);
    }
  });
}

function loadPlaylist(playlist) {
  if (!playlist || !playlist.json) {
    console.error('[Curated] Invalid playlist data');
    return;
  }
  
  currentPlaylistFile = playlist.json;
  console.log('[Curated] Requesting playlist download:', currentPlaylistFile);
  
  // Update section title
  document.getElementById('playlistSectionTitle').textContent = t('msg_loading', 'Loading') + ': ' + (playlist.name || t('lbl_unnamed', 'Unnamed'));
  
  // Show loading spinner in playlist table
  const playlistTable = document.getElementById('playlistTable');
  playlistTable.innerHTML = `
    <tr style="height: 100%;">
      <td colspan="3" style="height: 100%; text-align: center; vertical-align: middle;">
        <span id="loader"></span>
      </td>
    </tr>`;
  
  // Disable import buttons while loading
  setImportButtonsEnabled(false);
  
  // Send WebSocket command to trigger download
  websocket.send('loadplaylist=' + playlist.json);
}

function fetchPlaylist() {
  fetch('pl_import.json?t=' + Date.now())
    .then(response => {
      if (!response.ok) throw new Error('Failed to fetch playlist');
      return response.text();
    })
    .then(text => {
      // Parse JSONL format (JSON Lines)
      const lines = text.trim().split('\n');
      const stations = [];
      
      for (const line of lines) {
        const trimmed = line.trim();
        if (trimmed.length === 0) continue;
        
        try {
          const station = JSON.parse(trimmed);
          if (station.name && station.url) {
            stations.push(station);
          }
        } catch (e) {
          console.error('[Curated] Error parsing line:', trimmed, e);
        }
      }
      
      // Find the playlist name from indexData
      const playlist = indexData.find(p => p.json === currentPlaylistFile);
      const playlistName = playlist ? playlist.name : 'Playlist';
      
      currentPlaylistData = {
        name: playlistName,
        stations: stations
      };
      
      displayPlaylist(currentPlaylistData);
    })
    .catch(error => {
      console.error('[Curated] Error fetching playlist:', error);
      document.getElementById('playlistTable').innerHTML = '<tr class="line"><td class="importantmessage" colspan="3">' + t('msg_error_loading_playlist', 'Error loading playlist') + '</td></tr>';
      document.getElementById('playlistSectionTitle').textContent = t('msg_error_loading_playlist', 'Error loading playlist');
      setImportButtonsEnabled(false);
    });
}

function displayPlaylist(data) {
  const playlistTable = document.getElementById('playlistTable');
  playlistTable.innerHTML = '';
  
  // Update section title
  document.getElementById('playlistSectionTitle').textContent = data.name + ' (' + data.stations.length + ' ' + t('msg_stations', 'stations') + ')';
  
  const stations = data.stations || [];
  if (stations.length === 0) {
    playlistTable.innerHTML = '<tr class="line"><td colspan="3" class="importantmessage">' + t('msg_no_stations_in_playlist', 'No stations in this playlist') + '</td></tr>';
    setImportButtonsEnabled(false);
    return;
  }
  
  stations.forEach((station, idx) => {
    const row = document.createElement('tr');
    row.className = 'line';
    row.innerHTML = `
      <td class="preview"><button class="searchbutton preview" data-action="preview" data-index="${idx}">
        <svg viewBox="0 0 52 52" class="fill">
          <path d="M8,43.7V8.3c0-1,1.3-1.7,2.2-0.9l33.2,17.3c0.8,0.6,0.8,1.9,0,2.5L10.2,44.7C9.3,45.4,8,44.8,8,43.7z"/>
        </svg>
      </button></td>
      <td class="name">${escapeHtml(station.name || station.url)}</td>
      <td class="add"><button class="searchbutton addtoplaylist" data-action="add" data-index="${idx}">
        <svg viewBox="0 0 24 24" class="stroke">
          <path d="M5 12h7m7 0h-7m0 0V5m0 7v7"/>
        </svg>
      </button></td>
    `;
    playlistTable.appendChild(row);
  });

  // SVG icons from https://www.svgviewer.dev/s/474949/play & https://www.svgviewer.dev/s/495943/plus & https://www.svgviewer.dev/s/393355/down
  
  // Add click handler for both Preview and Add buttons
  playlistTable.addEventListener('click', (event) => {
    const button = event.target.closest('button[data-action]');
    if (button) {
      const index = parseInt(button.dataset.index);
      const action = button.dataset.action;
      const station = stations[index];
      if (station && station.url) {
        if (action === 'preview') {
          // Use sendStationAction from playstation.js for preview
          if (typeof sendStationAction === 'function') {
            sendStationAction(station.name || station.url, station.url, false);
          } else {
            console.error('[Curated] sendStationAction function not available');
          }
        } else if (action === 'add') {
          // Use sendStationAction from playstation.js to add to playlist
          if (typeof sendStationAction === 'function') {
            sendStationAction(station.name || station.url, station.url, true);
          } else {
            console.error('[Curated] sendStationAction function not available');
          }
        }
      }
    }
  });
  
  // Enable import buttons since we have data
  setImportButtonsEnabled(true);
}

function importPlaylist(mode) {
  if (mode !== 'replace' && mode !== 'merge') {
    console.error('[Curated] Invalid import mode:', mode);
    return;
  }
  
  // Verify that a playlist was actually loaded
  if (!currentPlaylistFile || !currentPlaylistData) {
    console.error('[Curated] No playlist loaded');
    alert(t('msg_load_playlist_first', 'Please load a playlist first by clicking on one from the list.'));
    return;
  }
  
  console.log('[Curated] Importing playlist with mode:', mode);
  
  // Send WebSocket command to import the playlist
  websocket.send('curated_import=' + mode);
}

// Button state management
function setButtonState(state) {
  const btn = document.getElementById('loadListsBtn');
  switch(state) {
    case 'load':
      btn.value = t('btn_load_index', 'Load Index');
      btn.disabled = false;
      break;
    case 'loading':
      btn.value = t('btn_loading_index', 'Loading Index...');
      btn.disabled = true;
      break;
    case 'loaded':
      btn.value = t('btn_index_loaded', 'Index Loaded');
      btn.disabled = true;
      break;
    case 'reload':
      btn.value = t('btn_reload_index', 'Reload Index');
      btn.disabled = false;
      break;
  }
}

// Import button management
function setImportButtonsEnabled(enabled) {
  document.getElementById('replaceBtn').disabled = !enabled;
  document.getElementById('mergeBtn').disabled = !enabled;
}

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}
