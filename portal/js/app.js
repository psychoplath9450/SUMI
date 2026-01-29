// =============================================================================
// CONFIGURATION
// =============================================================================
// SUMI v2.1.30 - E-ink Optimized Plugins
const PLUGINS = {
  core: [
    {id:'library', name:'Library', icon:'📚', desc:'Browse and read books'},
    {id:'flashcards', name:'Cards', icon:'🎴', desc:'Spaced repetition learning'},
    {id:'notes', name:'Notes', icon:'📝', desc:'Quick text notes'},
    {id:'images', name:'Images', icon:'🖼️', desc:'View images from SD'},
    {id:'maps', name:'Maps', icon:'🗺️', desc:'Offline map viewer'}
  ],
  games: [
    {id:'chess', name:'Chess', icon:'♟️', desc:'Play against AI'},
    {id:'checkers', name:'Checkers', icon:'🔴', desc:'Classic checkers'},
    {id:'sudoku', name:'Sudoku', icon:'🔢', desc:'Number puzzles'},
    {id:'minesweeper', name:'Mines', icon:'💣', desc:'Find the mines'},
    {id:'solitaire', name:'Solitaire', icon:'🃏', desc:'Klondike solitaire'},
    {id:'cube3d', name:'Demo', icon:'🧪', desc:'Development testing demos'}
  ],
  tools: [
    {id:'tools', name:'Tools', icon:'🔧', desc:'Calculator, Timer, Stopwatch'},
    {id:'todo', name:'To-Do', icon:'✅', desc:'Task management'}
  ],
  widgets: [
    {id:'weather', name:'Weather', icon:'🌤️', desc:'Current conditions & forecast'},
    {id:'book', name:'Book', icon:'📖', desc:'Current book progress'},
    {id:'orient', name:'Rotate', icon:'🔄', desc:'Quick orientation toggle'}
  ],
  system: [
    {id:'settings', name:'Settings', icon:'⚙️', desc:'WiFi, Display, System options', locked:true}
  ]
};

// Default apps: Widgets first (all 3), then apps, then settings
const DEFAULTS = ['weather','book','library','flashcards','chess','sudoku','settings'];

// =============================================================================
// STATE
// =============================================================================
let selected = new Set(DEFAULTS);
let originalSelected = new Set(DEFAULTS);
let processingState = {
  active: false,
  current: 0,
  total: 0,
  currentBook: '',
  currentStep: ''
};
let builderConfig = {
  // Theme & Layout
  theme: 0,
  iconStyle: 'rounded',
  orientation: 'portrait',
  grid: '2x3',
  fontSize: 12,
  
  // Home screen indicators
  showStatusBar: true,
  showBattery: true,
  showClock: true,
  showWifi: false,
  
  // Lock screen
  lockStyle: 'shuffle',
  showBatteryLock: true,
  
  // Sleep screen
  sleepStyle: 'shuffle',
  sleepPhotoSource: 'shuffle',
  showBatterySleep: true,
  sleepMinutes: 15,
  wakeButton: 'any'
};
let originalConfig = {...builderConfig};
let previewMode = 'home';
let pendingWifiSSID = '';
let wifiConnected = false;  // Track WiFi connection status
let fileCache = { books: [], images: [], maps: [], flashcards: [], notes: [] };

// =============================================================================
// IMMEDIATE NETWORK DETECTION
// =============================================================================
// If hostname is 192.168.4.1, we're DEFINITELY on hotspot - no async needed
// This runs immediately before any other code, so all functions have correct value
window.isOnHotspot = (window.location.hostname === '192.168.4.1');
window.hasInternetAccess = !window.isOnHotspot;
console.log('[PORTAL] Immediate detection: hostname =', window.location.hostname, 'isOnHotspot =', window.isOnHotspot);

// =============================================================================
// API
// =============================================================================
async function api(url, method='GET', data=null) {
  try {
    const opts = {method};
    if (data) {
      opts.headers = {'Content-Type': 'application/json'};
      opts.body = JSON.stringify(data);
    }
    const res = await fetch(url, opts);
    return res.ok ? await res.json() : null;
  } catch(e) {
    console.error('API Error:', e);
    return null;
  }
}

// =============================================================================
// NAVIGATION
// =============================================================================
function toggleSidebar() {
  document.getElementById('sidebar').classList.toggle('open');
}

function showPage(pageId) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
  document.getElementById('page-' + pageId)?.classList.add('active');
  document.querySelector(`.nav-item[data-page="${pageId}"]`)?.classList.add('active');
  document.getElementById('sidebar').classList.remove('open');
  
  // Load page-specific data
  if (pageId === 'summary') refreshSummary();
  if (pageId === 'wifi') loadWifiStatus();
  if (pageId === 'bluetooth') loadBT();
  if (pageId === 'files') refreshFiles('books');
}

document.querySelectorAll('.nav-item').forEach(item => {
  item.addEventListener('click', () => showPage(item.dataset.page));
});

// =============================================================================
// TOAST
// =============================================================================
function toast(msg, type='info') {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast show ' + type;
  setTimeout(() => t.classList.remove('show'), 3000);
}

// =============================================================================
// LOAD STATUS
// =============================================================================
async function loadStatus() {
  const d = await api('/api/status');
  if (!d) return;
  
  const bat = d.battery?.percent ?? '--';
  document.getElementById('statBattery').textContent = bat + '%';
  document.getElementById('headerBattery').textContent = '🔋 ' + bat + '%';
  document.getElementById('powerPercent').textContent = bat + '%';
  document.getElementById('powerVolts').textContent = (d.battery?.voltage ?? 0).toFixed(2) + ' V';
  
  const storage = d.storage?.sd_total_mb ? (d.storage.sd_total_mb / 1024).toFixed(1) + ' GB' : 'No SD';
  document.getElementById('statStorage').textContent = storage;
  
  // Track WiFi connection state
  wifiConnected = d.wifi?.connected ?? false;
  document.getElementById('statWifi').textContent = wifiConnected ? 'Connected' : 'AP Mode';
  document.getElementById('headerStatus').textContent = wifiConnected ? 'Connected' : 'Setup Mode';
  const statusDot = document.getElementById('statusDot');
  if (statusDot) statusDot.classList.toggle('disconnected', !wifiConnected);
  
  // Update connection banner based on ACTUAL WiFi state
  updateConnectionBanner(wifiConnected, d.wifi?.ssid, d.wifi?.ip);
  
  // Check for WiFi warnings after status update
  checkWifiWarnings();
  
  const uptime = Math.floor((d.uptime || 0) / 60);
  document.getElementById('statUptime').textContent = uptime + ' min';
  document.getElementById('aboutUptime').textContent = uptime + ' min';
  
  document.getElementById('infoFirmware').textContent = d.firmware || '--';
  document.getElementById('aboutFirmware').textContent = d.firmware || '--';
  document.getElementById('infoIP').textContent = d.wifi?.ip || '192.168.4.1';
  document.getElementById('infoMem').textContent = Math.round((d.free_heap || 0) / 1024) + ' KB';
  document.getElementById('aboutMem').textContent = Math.round((d.free_heap || 0) / 1024) + ' KB';
  document.getElementById('aboutSD').textContent = storage;
  
  // Also update summary page if visible
  if (document.getElementById('page-summary')?.classList.contains('active')) {
    refreshSummary();
  }
}

// =============================================================================
// MY SUMI SUMMARY
// =============================================================================
async function refreshSummary() {
  try {
    // Get device status
    const status = await api('/api/status');
    if (status) {
      document.getElementById('sumBattery').textContent = (status.battery?.percent ?? '--') + '%';
      document.getElementById('sumStorage').textContent = status.storage?.sd_total_mb ? (status.storage.sd_total_mb / 1024).toFixed(1) + ' GB' : 'No SD';
      document.getElementById('sumMemory').textContent = Math.round((status.free_heap || 0) / 1024) + ' KB';
      document.getElementById('sumUptime').textContent = Math.floor((status.uptime || 0) / 60) + ' min';
      
      // WiFi info
      if (status.wifi?.connected) {
        document.getElementById('sumWifiSSID').textContent = status.wifi.ssid || 'Connected';
        document.getElementById('sumWifiIP').textContent = status.wifi.ip || '--';
      } else {
        document.getElementById('sumWifiSSID').textContent = 'Not connected';
        document.getElementById('sumWifiIP').textContent = '192.168.4.1 (Hotspot)';
      }
    }
    
    // Get settings
    const settings = await api('/api/settings');
    if (settings) {
      // Weather/Location
      document.getElementById('sumLocation').textContent = settings.weather?.zipCode || settings.weather?.location || 'Auto (IP-based)';
      document.getElementById('sumTimezone').textContent = settings.display?.timezone || 'Auto';
      
      // Display settings
      document.getElementById('sumBootBook').textContent = settings.display?.bootToLastBook ? '✓ On' : '✗ Off';
      document.getElementById('sumInvert').textContent = settings.display?.invertColors ? '✓ On' : '✗ Off';
      document.getElementById('sumSleepTimer').textContent = settings.display?.sleepMinutes ? settings.display.sleepMinutes + ' min' : 'Off';
      
      // Sync
      document.getElementById('sumKosync').textContent = settings.sync?.kosyncEnabled ? '✓ Enabled' : '✗ Off';
      
      // Bluetooth
      document.getElementById('sumBluetooth').textContent = settings.bluetooth?.btEnabled ? '✓ Enabled' : '✗ Off';
      
      // Theme & Layout from builder config
      const themes = ['Classic', 'Modern', 'Minimal', 'Retro'];
      document.getElementById('sumTheme').textContent = themes[builderConfig.theme] || 'Classic';
      document.getElementById('sumIconStyle').textContent = builderConfig.iconStyle === 'rounded' ? 'Rounded' : 'Flat';
      document.getElementById('sumOrientation').textContent = builderConfig.orientation === 'portrait' ? 'Portrait' : 'Landscape';
      
      const statusBarParts = [];
      if (builderConfig.showBattery) statusBarParts.push('Battery');
      if (builderConfig.showClock) statusBarParts.push('Clock');
      if (builderConfig.showWifi) statusBarParts.push('WiFi');
      document.getElementById('sumStatusBar').textContent = statusBarParts.length > 0 ? statusBarParts.join(', ') : 'Hidden';
    }
    
    // Get enabled apps
    const appsDiv = document.getElementById('sumApps');
    appsDiv.innerHTML = '';
    const allPlugins = [...PLUGINS.core, ...PLUGINS.games, ...PLUGINS.tools, ...PLUGINS.widgets, ...PLUGINS.system];
    for (const id of selected) {
      const plugin = allPlugins.find(p => p.id === id);
      if (plugin) {
        const badge = document.createElement('span');
        badge.style.cssText = 'background: #e9ecef; padding: 4px 8px; border-radius: 12px; font-size: 11px; display: inline-flex; align-items: center; gap: 4px;';
        badge.innerHTML = `${plugin.icon} ${plugin.name}`;
        appsDiv.appendChild(badge);
      }
    }
    
    // Get file counts
    const bookData = await api('/api/files?path=/books');
    const imageData = await api('/api/files?path=/images');
    const mapData = await api('/api/files?path=/maps');
    const cardData = await api('/api/files?path=/flashcards');
    const noteData = await api('/api/files?path=/notes');
    
    const bookCount = bookData?.files?.filter(f => !f.dir && /\.(epub|txt|pdf)$/i.test(f.name)).length || 0;
    const imageCount = imageData?.files?.filter(f => !f.dir && /\.(bmp|raw)$/i.test(f.name)).length || 0;
    const mapCount = mapData?.files?.filter(f => f.dir || /\.(jpg|jpeg|png|bmp)$/i.test(f.name)).length || 0;
    const cardCount = cardData?.files?.filter(f => !f.dir && /\.(json|csv|tsv|txt)$/i.test(f.name)).length || 0;
    const noteCount = noteData?.files?.filter(f => !f.dir && /\.(txt|md)$/i.test(f.name)).length || 0;
    
    document.getElementById('sumBooks').textContent = bookCount;
    document.getElementById('sumImages').textContent = imageCount;
    document.getElementById('sumMaps').textContent = mapCount;
    document.getElementById('sumCards').textContent = cardCount;
    document.getElementById('sumNotes').textContent = noteCount;
    
    // Check book processing status
    try {
      const bookStatus = await fetch('/api/books/status');
      const bookStatusData = await bookStatus.json();
      const statusDiv = document.getElementById('sumBooksStatus');
      
      if (bookStatusData.unprocessed?.length > 0) {
        statusDiv.style.display = 'block';
        statusDiv.style.background = '#fff3cd';
        statusDiv.innerHTML = `<span style="color: #856404;">⚠️ ${bookStatusData.unprocessed.length} book${bookStatusData.unprocessed.length > 1 ? 's' : ''} need processing</span> <a href="#" onclick="showPage('files'); setTimeout(() => showFileTab('books'), 100); return false;" style="color: #856404; font-weight: 600; margin-left: 8px;">Process →</a>`;
      } else if (bookCount > 0) {
        statusDiv.style.display = 'block';
        statusDiv.style.background = '#d4edda';
        statusDiv.innerHTML = `<span style="color: #155724;">✓ All ${bookCount} book${bookCount > 1 ? 's' : ''} processed and ready</span>`;
      } else {
        statusDiv.style.display = 'none';
      }
    } catch (e) {
      document.getElementById('sumBooksStatus').style.display = 'none';
    }
    
    // Current time
    document.getElementById('sumTime').textContent = new Date().toLocaleTimeString();
    
    // Last updated
    document.getElementById('sumLastUpdated').textContent = 'Last updated: ' + new Date().toLocaleTimeString();
    
  } catch (e) {
    console.error('Failed to refresh summary:', e);
  }
}

// Update the connection banner based on actual WiFi state from API
function updateConnectionBanner(sumiConnected, ssid, sumiIP) {
  const banner = document.getElementById('connectionBanner');
  if (!banner) return;
  
  banner.style.display = 'block';
  
  if (!sumiConnected) {
    // State 1: SUMI is in hotspot-only mode (no WiFi credentials saved)
    banner.innerHTML = `
      <div style="background: linear-gradient(90deg, #f8d7da 0%, #f5c6cb 100%); border-bottom: 2px solid #dc3545; padding: 12px 16px; display: flex; align-items: center; gap: 12px;">
        <span style="font-size: 24px;">⚠️</span>
        <div style="flex: 1;">
          <div style="font-weight: 700; color: #721c24; font-size: 14px;">Home WiFi Required for Books</div>
          <div style="color: #721c24; font-size: 12px; margin-top: 4px;">SUMI needs your home network to process EPUBs, get weather, and sync progress. Games work offline.</div>
        </div>
        <a href="#" onclick="showPage('wifi'); return false;" style="background: #dc3545; color: white; padding: 8px 14px; border-radius: 6px; text-decoration: none; font-size: 12px; font-weight: 600;">Add WiFi →</a>
      </div>
    `;
    return;
  }
  
  // SUMI is connected to home WiFi - check if browser is also on home network
  // Use the global isOnHotspot flag set during page load
  const sumiHasHomeIP = sumiIP && sumiIP !== '192.168.4.1';
  const browserOnHotspot = window.isOnHotspot === true;
  
  if (browserOnHotspot && sumiHasHomeIP) {
    // State 2: Browser on hotspot but SUMI connected to home WiFi - prompt to switch
    banner.innerHTML = `
      <div style="background: linear-gradient(90deg, #cce5ff 0%, #b8daff 100%); border-bottom: 2px solid #007bff; padding: 12px 16px; display: flex; align-items: center; gap: 12px;">
        <span style="font-size: 24px;">🔄</span>
        <div style="flex: 1;">
          <div style="font-weight: 700; color: #004085; font-size: 14px;">SUMI Connected! Now switch your device</div>
          <div style="color: #004085; font-size: 12px; margin-top: 4px;">
            Connect your phone/computer to <strong>${ssid}</strong>, then visit <a href="http://sumi.local" style="color: #004085; font-weight: 600;">sumi.local</a> or <a href="http://${sumiIP}" style="color: #004085; font-weight: 600;">${sumiIP}</a>
          </div>
        </div>
      </div>
    `;
  } else if (!browserOnHotspot && sumiHasHomeIP) {
    // State 3: Both SUMI and browser on home network - full features!
    banner.innerHTML = `
      <div style="background: linear-gradient(90deg, #d4edda 0%, #c3e6cb 100%); border-bottom: 1px solid #28a745; padding: 8px 16px; display: flex; align-items: center; gap: 10px;">
        <span style="font-size: 18px;">✅</span>
        <div style="flex: 1;">
          <span style="font-weight: 600; color: #155724;">Connected via Home Network</span>
          <span style="color: #155724; font-size: 12px;"> — Full features available: weather, sync, book processing.</span>
        </div>
        <span style="color: #155724; font-size: 11px; opacity: 0.8;">${ssid} • ${sumiIP}</span>
      </div>
    `;
  } else {
    // Fallback: unclear state
    banner.innerHTML = `
      <div style="background: linear-gradient(90deg, #fff3cd 0%, #ffeeba 100%); border-bottom: 2px solid #ffc107; padding: 12px 16px; display: flex; align-items: center; gap: 12px;">
        <span style="font-size: 24px;">📶</span>
        <div style="flex: 1;">
          <div style="font-weight: 700; color: #856404; font-size: 14px;">SUMI is on ${ssid}</div>
          <div style="color: #856404; font-size: 12px; margin-top: 4px;">
            Switch to the same network to access all features
          </div>
        </div>
      </div>
    `;
  }
  
  // Update Files page hotspot warning visibility
  const hotspotWarning = document.getElementById('hotspotBookWarning');
  const bookProcessingInfo = document.getElementById('bookProcessingInfo');
  if (hotspotWarning && bookProcessingInfo) {
    hotspotWarning.style.display = browserOnHotspot ? 'block' : 'none';
    bookProcessingInfo.style.display = browserOnHotspot ? 'none' : 'block';
  }
}

// =============================================================================
// BUILDER
// =============================================================================
function showBuilderTab(tab) {
  document.querySelectorAll('.builder-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.builder-panel').forEach(p => p.classList.remove('active'));
  document.querySelector(`.builder-tab[onclick*="${tab}"]`)?.classList.add('active');
  document.getElementById('panel-' + tab)?.classList.add('active');
}

function renderPlugins() {
  // Ensure settings is always in the selected set
  selected.add('settings');
  
  for (const [cat, plugins] of Object.entries(PLUGINS)) {
    const grid = document.getElementById('plugins-' + cat);
    if (!grid) continue;
    grid.innerHTML = plugins.map(p => {
      // Settings is always enabled and locked (can't be removed)
      if (p.id === 'settings' || p.locked) {
        return `
          <div class="plugin-card selected locked" title="${p.desc} (always enabled)">
            <div class="plugin-icon">${p.icon}</div>
            <div class="plugin-name">${p.name}</div>
          </div>`;
      }
      return `
        <div class="plugin-card ${selected.has(p.id)?'selected':''}" onclick="togglePlugin('${p.id}')" title="${p.desc}">
          <div class="plugin-icon">${p.icon}</div>
          <div class="plugin-name">${p.name}</div>
        </div>`;
    }).join('');
  }
  updatePreview();
}

function togglePlugin(id) {
  // Settings toggle disabled
  if (id === 'settings') return;
  if (selected.has(id)) selected.delete(id);
  else selected.add(id);
  renderPlugins();
  checkWifiWarnings();
}

// Check if WiFi-dependent features are enabled without WiFi
function checkWifiWarnings() {
  const warningEl = document.getElementById('wifiWarning');
  if (!warningEl) return;
  
  const needsWifi = selected.has('weather');
  
  if (needsWifi && !wifiConnected) {
    warningEl.style.display = 'block';
    warningEl.innerHTML = `
      <div style="background:#fff3cd;border:1px solid #ffc107;border-radius:6px;padding:8px 12px;margin:8px 0;font-size:11px;">
        <strong>⚠️ WiFi Required</strong><br>
        Weather widget needs WiFi to fetch data. 
        <a href="#" onclick="navigateTo('page-wifi');return false;" style="color:#856404;">Connect to WiFi →</a>
      </div>
    `;
  } else {
    warningEl.style.display = 'none';
    warningEl.innerHTML = '';
  }
}

function selectTheme(idx) {
  builderConfig.theme = idx;
  document.querySelectorAll('#panel-theme .style-option').forEach((o,i) => {
    if (i < 3) o.classList.toggle('selected', i === idx);
  });
  updatePreview();
}

function selectIconStyle(style) {
  builderConfig.iconStyle = style;
  document.querySelectorAll('#panel-theme .style-option').forEach(o => {
    if (o.querySelector('.icon-preview')) {
      o.classList.toggle('selected', o.onclick.toString().includes(style));
    }
  });
  updatePreview();
}

function updateFontSize(val) {
  builderConfig.fontSize = parseInt(val);
  document.getElementById('fontSizeVal').textContent = val + 'px';
}

function selectOrientation(orient) {
  builderConfig.orientation = orient;
  document.querySelectorAll('#panel-layout .style-option').forEach(o => {
    if (o.querySelector('.orient-preview')) {
      o.classList.toggle('selected', o.onclick.toString().includes(orient));
    }
  });
  updatePreview();
}

function togglePreviewOrientation() {
  const newOrient = builderConfig.orientation === 'portrait' ? 'landscape' : 'portrait';
  selectOrientation(newOrient);
}

// Grid is now auto-calculated based on app count - no manual selection needed

function selectLockStyle(style) {
  builderConfig.lockStyle = style;
  document.querySelectorAll('#panel-lockscreen .style-option').forEach(o => {
    const onclick = o.getAttribute('onclick') || '';
    if (onclick.includes('selectLockStyle')) {
      o.classList.toggle('selected', onclick.includes("'" + style + "'"));
    }
  });
  // Show/hide photo options
  const photoOpts = document.getElementById('lockPhotoOptions');
  if (photoOpts) photoOpts.style.display = (style === 'photo') ? 'block' : 'none';
  updatePreview();
}

function selectPhotoSource(src) {
  builderConfig.lockPhotoSource = src;
  document.querySelectorAll('#lockPhotoOptions .style-option').forEach(o => {
    const onclick = o.getAttribute('onclick') || '';
    o.classList.toggle('selected', onclick.includes("'" + src + "'"));
  });
}

function selectClockStyle(style) {
  builderConfig.clockStyle = style;
  document.querySelectorAll('#panel-lockscreen .style-option').forEach(o => {
    const onclick = o.getAttribute('onclick') || '';
    if (onclick.includes('selectClockStyle')) {
      o.classList.toggle('selected', onclick.includes("'" + style + "'"));
    }
  });
}

function selectSleepStyle(style) {
  builderConfig.sleepStyle = style;
  document.querySelectorAll('#panel-sleep .style-option').forEach(o => {
    const onclick = o.getAttribute('onclick') || '';
    if (onclick.includes('selectSleepStyle')) {
      o.classList.toggle('selected', onclick.includes("'" + style + "'"));
    }
  });
  updatePreview();
}

function toggleBuilder(el, key) {
  el.classList.toggle('on');
  builderConfig[key] = el.classList.contains('on');
  updatePreview();
}

function setPreviewMode(mode) {
  previewMode = mode;
  document.querySelectorAll('.preview-tab').forEach(t => t.classList.toggle('active', t.onclick.toString().includes(mode)));
  document.getElementById('previewBadge').textContent = {home:'Home',sleep:'Sleep'}[mode];
  updatePreview();
}

function updatePreview() {
  const frame = document.getElementById('deviceFrame');
  const screen = document.getElementById('einkScreen');
  const stats = document.getElementById('previewStats');
  
  const isPortrait = builderConfig.orientation === 'portrait';
  const themeClass = ['light','dark','contrast'][builderConfig.theme] || 'light';
  
  frame.className = 'device-outer ' + builderConfig.orientation;
  screen.className = 'eink-screen ' + themeClass;
  
  if (previewMode === 'home') {
    renderHomePreview(screen, stats);
  } else {
    renderSleepPreview(screen, stats);
  }
}

function renderHomePreview(screen, stats) {
  const isPortrait = builderConfig.orientation === 'portrait';
  
  // Check which widgets are enabled
  const hasBook = selected.has('book');
  const hasWeather = selected.has('weather');
  const hasOrient = selected.has('orient');
  const widgetCount = (hasBook ? 1 : 0) + (hasWeather ? 1 : 0) + (hasOrient ? 1 : 0);
  
  // Get enabled apps (non-widget plugins)
  const widgetIds = ['weather', 'book', 'orient'];
  const all = Object.values(PLUGINS).flat();
  let apps = all.filter(p => selected.has(p.id) && !widgetIds.includes(p.id) && p.id !== 'settings');
  
  // Settings always at end
  const settingsPlugin = all.find(p => p.id === 'settings');
  if (settingsPlugin) apps.push(settingsPlugin);
  
  const appCount = apps.length;
  
  // Build status bar
  let statusBar = '';
  if (builderConfig.showStatusBar) {
    statusBar = `<div class="eink-statusbar">
      <div class="statusbar-section">${builderConfig.showClock ? '12:34 PM' : ''}</div>
      <div class="statusbar-section">
        ${builderConfig.showWifi ? '<span style="font-size:7px">WiFi</span>' : ''}
        ${builderConfig.showBattery ? '100%' : ''}
      </div>
    </div>`;
  }
  
  // Build widgets HTML matching actual device layout
  let widgetsHtml = '';
  
  if (widgetCount > 0) {
    if (isPortrait) {
      // PORTRAIT MODE - matches HomeScreen.cpp layout
      if (hasBook && hasWeather && hasOrient) {
        // All 3: Book left (55%) | Weather + Orient stacked right (45%)
        widgetsHtml = `<div style="display:flex;gap:3px;height:45%;padding:3px;">
          <div style="width:60%;display:flex;align-items:stretch;">
            ${renderBookWidget(true)}
          </div>
          <div style="flex:1;display:flex;flex-direction:column;gap:3px;">
            <div style="flex:7;">${renderWeatherWidget(true)}</div>
            <div style="flex:3;">${renderOrientWidget()}</div>
          </div>
        </div>`;
      } else if (hasBook && hasWeather) {
        // Book + Weather: side by side
        widgetsHtml = `<div style="display:flex;gap:3px;height:45%;padding:3px;">
          <div style="width:60%;">${renderBookWidget(true)}</div>
          <div style="flex:1;">${renderWeatherWidget(true)}</div>
        </div>`;
      } else if (hasBook && hasOrient) {
        // Book + Orient: Book left, Orient right (slim)
        widgetsHtml = `<div style="display:flex;gap:3px;height:45%;padding:3px;">
          <div style="width:60%;">${renderBookWidget(true)}</div>
          <div style="flex:1;display:flex;align-items:center;justify-content:center;">${renderOrientWidget()}</div>
        </div>`;
      } else if (hasWeather && hasOrient) {
        // Weather + Orient: Both stacked on right side style
        widgetsHtml = `<div style="display:flex;gap:3px;height:25%;padding:3px;">
          <div style="flex:1;display:flex;flex-direction:column;gap:3px;">
            <div style="flex:7;">${renderWeatherWidget(true)}</div>
            <div style="flex:3;">${renderOrientWidget()}</div>
          </div>
        </div>`;
      } else if (hasBook) {
        // Just Book: centered, large
        widgetsHtml = `<div style="display:flex;justify-content:center;height:45%;padding:3px;">
          <div style="width:60%;">${renderBookWidget(true)}</div>
        </div>`;
      } else if (hasWeather) {
        // Just Weather: centered
        widgetsHtml = `<div style="display:flex;justify-content:center;height:20%;padding:3px;">
          <div style="width:60%;">${renderWeatherWidget(true)}</div>
        </div>`;
      } else if (hasOrient) {
        // Just Orient: slim bar centered (like in the photo)
        widgetsHtml = `<div style="display:flex;justify-content:center;align-items:center;height:10%;padding:3px;">
          ${renderOrientWidget()}
        </div>`;
      }
    } else {
      // LANDSCAPE MODE - widgets stacked vertically on left
      let widgetItems = [];
      if (hasBook) widgetItems.push(renderBookWidget(false));
      if (hasWeather) widgetItems.push(renderWeatherWidget(false));
      if (hasOrient) widgetItems.push(renderOrientWidget());
      
      widgetsHtml = `<div style="width:30%;display:flex;flex-direction:column;gap:2px;padding:2px;">
        ${widgetItems.map(w => `<div style="flex:1;">${w}</div>`).join('')}
      </div>`;
    }
  }
  
  // Calculate app grid
  let cols, rows;
  if (isPortrait) {
    cols = 2;
    rows = Math.ceil(appCount / cols);
  } else {
    if (appCount <= 4) { cols = 2; rows = 2; }
    else if (appCount <= 6) { cols = 3; rows = 2; }
    else { cols = 4; rows = 2; }
  }
  
  const labelSize = isPortrait ? 9 : 8;
  
  // Build app grid
  let appsHtml = '';
  const maxItems = cols * rows;
  for (let i = 0; i < maxItems; i++) {
    if (i < apps.length) {
      const p = apps[i];
      const isSelected = i === 0; // First app selected
      appsHtml += `<div class="eink-app ${isSelected ? 'selected' : ''}">
        <div class="eink-app-box style-${builderConfig.iconStyle}" style="font-size:${labelSize}px">${p.name}</div>
      </div>`;
    }
  }
  
  // Combine layout
  if (isPortrait) {
    screen.innerHTML = `
      ${statusBar}
      <div style="display:flex;flex-direction:column;height:${builderConfig.showStatusBar ? 'calc(100% - 14px)' : '100%'};">
        ${widgetsHtml}
        <div style="display:grid;grid-template-columns:repeat(${cols},1fr);gap:3px;flex:1;padding:3px;">${appsHtml}</div>
      </div>
    `;
  } else {
    // Landscape: widgets on left, apps on right
    screen.innerHTML = `
      ${statusBar}
      <div style="display:flex;height:${builderConfig.showStatusBar ? 'calc(100% - 14px)' : '100%'};">
        ${widgetsHtml}
        <div style="display:grid;grid-template-columns:repeat(${cols},1fr);gap:2px;flex:1;padding:2px;">${appsHtml}</div>
      </div>
    `;
  }
  
  const themeLabel = ['Light', 'Dark', 'High Contrast'][builderConfig.theme];
  stats.innerHTML = `${widgetCount} widgets · ${appCount} apps · ${themeLabel}`;
}

// Helper functions to render individual widgets
function renderBookWidget(tall) {
  const h = tall ? '100%' : 'auto';
  return `<div class="eink-app-box style-${builderConfig.iconStyle}" style="height:${h};display:flex;flex-direction:column;align-items:center;justify-content:center;gap:1px;padding:4px;">
    <div style="width:70%;flex:1;min-height:20px;background:linear-gradient(135deg,#8B4513,#654321);border:1px solid #333;border-radius:2px;"></div>
  </div>`;
}

function renderWeatherWidget(showDetails) {
  if (showDetails) {
    return `<div class="eink-app-box style-${builderConfig.iconStyle}" style="height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:1px;padding:3px;">
      <div style="font-size:5px;opacity:0.7;">Location</div>
      <div style="font-size:8px;">🌤️</div>
      <div style="font-size:9px;font-weight:bold;">Temp</div>
      <div style="font-size:5px;opacity:0.7;">Humidity</div>
      <div style="font-size:5px;opacity:0.7;">Day</div>
      <div style="font-size:5px;opacity:0.7;">Date</div>
    </div>`;
  }
  return `<div class="eink-app-box style-${builderConfig.iconStyle}" style="height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:2px;">
    <div style="font-size:7px;">🌤️ Temp</div>
  </div>`;
}

function renderOrientWidget() {
  const isPortrait = builderConfig.orientation === 'portrait';
  return `<div class="eink-app-box style-${builderConfig.iconStyle}" style="display:flex;align-items:center;justify-content:center;padding:4px;height:100%;">
    <div style="width:28px;height:12px;border:1px solid currentColor;border-radius:6px;position:relative;">
      <div style="width:10px;height:10px;background:currentColor;border-radius:50%;position:absolute;top:0px;${isPortrait ? 'right:0px' : 'left:0px'};"></div>
    </div>
  </div>`;
}

function renderLockPreview(screen, stats) {
  const themeLabel = ['Light', 'Dark', 'High Contrast'][builderConfig.theme];
  let html = '';
  
  html += `<div class="eink-lock">`;
  
  if (builderConfig.lockStyle === 'minimal') {
    html += `<div class="lock-minimal-display"></div>`;
  } else {
    // Shuffle images - show placeholder image grid
    html += `<div style="display:grid;grid-template-columns:repeat(2,1fr);gap:4px;padding:8px;width:100%;height:100%;">
      <div style="background:linear-gradient(135deg,#ddd,#aaa);border-radius:2px;"></div>
      <div style="background:linear-gradient(45deg,#ccc,#999);border-radius:2px;"></div>
      <div style="background:linear-gradient(180deg,#bbb,#888);border-radius:2px;"></div>
      <div style="background:linear-gradient(90deg,#ddd,#aaa);border-radius:2px;"></div>
    </div>`;
  }
  
  html += `</div>`;
  
  screen.innerHTML = html;
  
  const styleLabel = builderConfig.lockStyle === 'minimal' ? 'Minimal' : 'Shuffle Images';
  stats.innerHTML = `${styleLabel} · ${themeLabel}`;
}

function renderSleepPreview(screen, stats) {
  const themeLabel = ['Light', 'Dark', 'High Contrast'][builderConfig.theme];
  let html = '<div class="eink-sleep">';
  
  if (builderConfig.sleepStyle === 'off') {
    html += `<div class="sleep-display clock-mode" style="flex-direction:column;gap:10px;">
      <div style="font-size:14px;font-weight:bold;">wake me up...</div>
    </div>`;
  } else if (builderConfig.sleepStyle === 'book') {
    html += `<div class="sleep-display clock-mode" style="flex-direction:column;gap:10px;">
      <div style="font-size:16px;font-weight:bold;">📕</div>
      <div style="font-size:11px;opacity:0.6;">Book Cover</div>
    </div>`;
  } else if (builderConfig.sleepStyle === 'shuffle') {
    html += `<div class="sleep-display clock-mode" style="flex-direction:column;gap:10px;">
      <div style="font-size:16px;font-weight:bold;">🔀</div>
      <div style="font-size:11px;opacity:0.6;">Random Image</div>
    </div>`;
  }
  
  html += '</div>';
  
  // Battery warning
  if (builderConfig.sleepShowBattery && builderConfig.sleepStyle !== 'off') {
    html += `<div class="sleep-battery-warn">Low battery warning enabled</div>`;
  }
  
  screen.innerHTML = html;
  
  const styleLabel = {off: 'Wake Me Up', book: 'Book Cover', shuffle: 'Shuffle Images'}[builderConfig.sleepStyle];
  stats.innerHTML = styleLabel;
}

// =============================================================================
// SAVE & DEPLOY
// =============================================================================
async function saveAndDeploy() {
  const btn = document.getElementById('deployBtn');
  btn.innerHTML = '⏳ Deploying...';
  btn.disabled = true;
  
  try {
    // Auto-calculate grid based on app count for BOTH orientations
    const appCount = selected.size + 1; // +1 for settings
    const isPortrait = builderConfig.orientation === 'portrait';
    
    // Calculate columns for portrait mode
    let vCols;
    if (appCount <= 4) { vCols = 2; }
    else if (appCount <= 6) { vCols = 2; }
    else if (appCount <= 8) { vCols = 2; }
    else { vCols = 3; }
    
    // Calculate columns for landscape mode
    let hCols;
    if (appCount <= 4) { hCols = 2; }
    else if (appCount <= 6) { hCols = 3; }
    else if (appCount <= 8) { hCols = 4; }
    else { hCols = 5; }
    
    // Save settings - homeItems handles plugin selection
    await api('/api/settings', 'POST', {
      homeItems: [...selected],
      themeIndex: builderConfig.theme,
      display: {
        // Layout - auto-calculated (hItemsPerRow/vItemsPerRow are COLUMNS)
        orientation: builderConfig.orientation === 'portrait' ? 1 : 0,
        hItemsPerRow: hCols,
        vItemsPerRow: vCols,
        
        // Widget visibility - derived from selected set
        showBookWidget: selected.has('book'),
        showWeatherWidget: selected.has('weather'),
        showOrientWidget: selected.has('orient'),
        
        // Appearance
        buttonShape: {rounded:0, square:2, circle:1}[builderConfig.iconStyle] || 0,
        bgTheme: builderConfig.theme,
        invertColors: builderConfig.theme === 1,
        fontSize: builderConfig.fontSize || 12,
        
        // Home screen indicators
        showStatusBar: builderConfig.showStatusBar ?? true,
        showBatteryHome: builderConfig.showBattery,
        showClockHome: builderConfig.showClock,
        showWifi: builderConfig.showWifi,
        
        // Lock screen settings
        lockStyle: {clock:0, photo:1, quote:2, minimal:3}[builderConfig.lockStyle] || 0,
        lockPhotoSource: {shuffle:0, select:1, folder:2}[builderConfig.lockPhotoSource] || 0,
        clockStyle: {digital:0, analog:1, minimal:2}[builderConfig.clockStyle] || 0,
        showDate: builderConfig.showDate,
        showBatteryLock: builderConfig.showBatteryLock,
        showWeatherLock: builderConfig.showWeatherLock,
        
        // Sleep screen settings - NEW: book=0, shuffle=1, photo=2, off=3
        sleepStyle: {book:0, shuffle:1, off:2}[builderConfig.sleepStyle] || 2,
        sleepSelectedImage: builderConfig.sleepSelectedImage || '',
        showBatterySleep: builderConfig.showBatterySleep,
        sleepMinutes: builderConfig.sleepMinutes || 15,
        wakeButton: {any:0, select:1, power:2}[builderConfig.wakeButton] || 0
      }
      // Reader settings now managed on-device only
    });
    
    originalSelected = new Set(selected);
    originalConfig = {...builderConfig};
    
    btn.innerHTML = '✓ Deployed!';
    toast('Settings saved! Your Sumi is updating...', 'success');
    
  } catch(e) {
    console.error(e);
    btn.innerHTML = '❌ Error';
    toast('Failed to save settings', 'error');
  }
  
  setTimeout(() => {
    btn.innerHTML = '💾 Save & Deploy';
    btn.disabled = false;
  }, 2000);
}

function resetToDefaults() {
  if (!confirm('Reset all settings to defaults?')) return;
  selected = new Set(DEFAULTS);
  builderConfig = {
    theme: 0, iconStyle: 'rounded', orientation: 'portrait', fontSize: 12,
    showStatusBar: true, showBattery: true, showClock: true, showWifi: false,
    lockStyle: 'shuffle', showBatteryLock: true,
    sleepStyle: 'shuffle', showBatterySleep: true, sleepMinutes: 15, wakeButton: 'any'
  };
  syncUIToConfig();
  renderPlugins();
  updatePreview();
  toast('Reset to defaults', 'success');
}

// =============================================================================
// WIFI
// =============================================================================
async function loadWifiStatus() {
  const d = await api('/api/status');
  if (d?.wifi) {
    wifiConnected = d.wifi.connected;
    
    if (d.wifi.connected) {
      // SUMI is connected to a network - check if browser is on hotspot
      // Only 192.168.4.1 definitively means hotspot (sumi.local works via mDNS from home network too)
      const browserHost = window.location.hostname;
      const isOnHotspot = (browserHost === '192.168.4.1');
      
      if (isOnHotspot) {
        // User is on hotspot but SUMI connected to WiFi - prompt to switch for full features
        document.getElementById('wifiStatus').innerHTML = `
          <div style="color:var(--success);font-weight:600;margin-bottom:8px;">✓ SUMI Connected to ${d.wifi.ssid}</div>
          <div style="background: #fff3cd; border: 1px solid #ffc107; border-radius: 8px; padding: 12px; margin: 10px 0;">
            <div style="font-size: 12px; color: #856404; line-height: 1.5;">
              <strong>Switch to ${d.wifi.ssid}</strong> for book processing, then visit:<br>
              <a href="http://sumi.local" style="color: #856404; font-weight: 600;">sumi.local</a> or 
              <a href="http://${d.wifi.ip}" style="color: #856404; font-weight: 600;">${d.wifi.ip}</a>
            </div>
          </div>
          <button class="btn btn-sm btn-secondary" style="margin-top:8px;" onclick="disconnectWifi()">📴 Disconnect</button>
        `;
      } else {
        // User is already on home network - all good!
        document.getElementById('wifiStatus').innerHTML = `
          <div style="color:var(--success);font-weight:600;margin-bottom:8px;">✓ Connected to ${d.wifi.ssid}</div>
          <div style="color:var(--text-muted);font-size:12px;">IP: ${d.wifi.ip}</div>
          <div style="color:var(--text-muted);margin-top:8px;font-size:12px;">📍 Weather location auto-detected from IP</div>
          <button class="btn btn-sm btn-secondary" style="margin-top:12px;" onclick="disconnectWifi()">📴 Disconnect</button>
        `;
      }
    } else {
      document.getElementById('wifiStatus').innerHTML = `
        <div style="color:var(--warning);font-weight:600;margin-bottom:8px;">📡 Hotspot Mode</div>
        <div style="color:var(--text-muted);">Connect to a WiFi network below to enable weather, sync, and book processing features</div>
      `;
      
      // Auto-scan for networks if not connected yet
      scanWifi();
    }
    // Update warning after status is known
    checkWifiWarnings();
  }
}

async function disconnectWifi() {
  toast('Disconnecting...', 'info');
  await api('/api/wifi/disconnect', 'POST');
  toast('Disconnected. Device returning to hotspot mode.', 'success');
  setTimeout(loadWifiStatus, 2000);
}

async function scanWifi() {
  const btn = document.getElementById('wifiScanBtn');
  btn.disabled = true;
  btn.textContent = '⏳ Scanning...';
  document.getElementById('wifiList').innerHTML = '<p style="text-align:center;padding:20px;color:var(--text-muted);">Scanning...</p>';
  
  const d = await api('/api/wifi/scan');
  
  if (d?.networks?.length) {
    document.getElementById('wifiList').innerHTML = d.networks.map(n => {
      const safeSSID = n.ssid.replace(/'/g, "&#39;").replace(/"/g, "&quot;");
      return `
      <div class="wifi-item" onclick="promptWifi('${safeSSID}')">
        <span class="wifi-icon">📶</span>
        <div class="wifi-info">
          <div class="wifi-name">${n.ssid}</div>
          <div class="wifi-meta">${n.rssi} dBm ${n.secure ? '🔒' : 'Open'}</div>
        </div>
      </div>
    `}).join('');
  } else {
    document.getElementById('wifiList').innerHTML = '<p style="text-align:center;padding:20px;color:var(--text-muted);">No networks found. Try again.</p>';
  }
  
  btn.disabled = false;
  btn.textContent = '🔍 Scan';
}

function promptWifi(ssid) {
  // Decode HTML entities back to original characters
  const decoded = ssid.replace(/&#39;/g, "'").replace(/&quot;/g, '"');
  pendingWifiSSID = decoded;
  document.getElementById('wifiModalSSID').textContent = decoded;
  document.getElementById('wifiPassword').value = '';
  document.getElementById('wifiModal').classList.add('show');
  document.getElementById('wifiPassword').focus();
}

function closeModal(id) {
  document.getElementById(id).classList.remove('show');
}

async function connectWifi() {
  const pw = document.getElementById('wifiPassword').value;
  closeModal('wifiModal');
  toast('Saving WiFi credentials...', 'info');
  
  // Show connecting status immediately
  document.getElementById('wifiStatus').innerHTML = `
    <div style="color:var(--primary);font-weight:600;margin-bottom:8px;">⏳ Connecting to ${pendingWifiSSID}...</div>
    <div style="color:var(--text-muted);font-size:12px;">Please wait while SUMI connects to your network</div>
  `;
  
  const result = await api('/api/wifi/connect', 'POST', {ssid: pendingWifiSSID, password: pw});
  
  if (result?.status === 'connected' || result?.status === 'credentials_saved') {
    toast('✓ WiFi credentials saved! Connecting...', 'success');
    // Poll for connection status
    pollWifiConnection(pendingWifiSSID);
  } else {
    toast('Failed to save credentials.', 'error');
    loadWifiStatus();
  }
}

// Poll for WiFi connection after saving credentials
async function pollWifiConnection(ssid, attempts = 0) {
  if (attempts >= 10) {
    // Give up after 10 attempts (20 seconds)
    document.getElementById('wifiStatus').innerHTML = `
      <div style="color:var(--warning);font-weight:600;margin-bottom:8px;">⚠️ Connection taking longer than expected</div>
      <div style="color:var(--text-muted);font-size:12px;">SUMI may still connect. Check the device screen or try again.</div>
    `;
    return;
  }
  
  const d = await api('/api/status');
  if (d?.wifi?.connected) {
    // Connected! Show success with switch instructions
    document.getElementById('wifiStatus').innerHTML = `
      <div style="color:var(--success);font-weight:600;margin-bottom:8px;font-size:16px;">✓ SUMI Connected to ${d.wifi.ssid}!</div>
      <div style="background: linear-gradient(135deg, #d4edda 0%, #c3e6cb 100%); border: 2px solid #28a745; border-radius: 8px; padding: 14px; margin: 12px 0;">
        <div style="font-size: 13px; color: #155724; line-height: 1.6;">
          <strong>✓ Safe to switch networks now!</strong><br><br>
          1. Go to your WiFi settings<br>
          2. Connect to <strong>${d.wifi.ssid}</strong><br>
          3. Return here or visit: <a href="http://${d.wifi.ip}" style="color: #155724; font-weight: 600;">${d.wifi.ip}</a>
        </div>
      </div>
      <button class="btn btn-sm btn-secondary" style="margin-top:8px;" onclick="disconnectWifi()">📴 Disconnect</button>
    `;
    checkWifiWarnings();
    
    // Auto-detect timezone from browser on first WiFi connect
    autoSetupTimezoneAndLocation();
  } else {
    // Still connecting, update status and try again
    document.getElementById('wifiStatus').innerHTML = `
      <div style="color:var(--primary);font-weight:600;margin-bottom:8px;">⏳ Connecting to ${ssid}...</div>
      <div style="color:var(--text-muted);font-size:12px;">Attempt ${attempts + 1}/10 - Please wait...</div>
    `;
    setTimeout(() => pollWifiConnection(ssid, attempts + 1), 2000);
  }
}

// =============================================================================
// FILES
// =============================================================================
function showFileTab(tab) {
  document.querySelectorAll('.file-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.file-panel').forEach(p => p.classList.remove('active'));
  document.querySelector(`.file-tab[onclick*="${tab}"]`)?.classList.add('active');
  document.getElementById('panel-' + tab)?.classList.add('active');
  
  // Don't refresh books tab while processing - it would wipe out the progress UI
  if (tab === 'books' && processingState.active) {
    return;
  }
  
  refreshFiles(tab);
  
  // Load flashcard settings when tab is shown
  if (tab === 'flashcards') {
    loadFlashcardSettings();
  }
}

async function refreshFiles(type) {
  const paths = {books:'/books', images:'/images', maps:'/maps', flashcards:'/flashcards', notes:'/notes'};
  const d = await api('/api/files?path=' + paths[type]);
  if (!d?.files) return;
  
  // Filter by type
  let files = d.files;
  
  if (type === 'books') {
    // Include epub, txt, pdf files (no directories)
    files = files.filter(f => !f.dir && /\.(epub|txt|pdf)$/i.test(f.name));
    
    // Get processing status and metadata for books
    try {
      const statusResp = await fetch('/api/books/status');
      const statusData = await statusResp.json();
      
      // Build lookup map for processed books with metadata
      const processedMap = {};
      (statusData.processed || []).forEach(b => {
        processedMap[b.name] = b;
      });
      
      files = files.map(f => {
        const meta = processedMap[f.name];
        if (meta) {
          return {
            ...f,
            isProcessed: true,
            hash: meta.hash || simpleHash(f.name),
            title: meta.title,
            author: meta.author,
            totalChapters: meta.totalChapters,
            totalWords: meta.totalWords,
            totalImages: meta.totalImages || 0,
            estimatedPages: meta.estimatedPages,
            estimatedReadingMins: meta.estimatedReadingMins,
            currentChapter: meta.currentChapter,
            currentPage: meta.currentPage,
            lastRead: meta.lastRead
          };
        } else {
          return { ...f, isProcessed: false, hash: simpleHash(f.name) };
        }
      });
    } catch (e) {
      // If status API fails, just show books without processed indicator
      files = files.map(f => ({ ...f, isProcessed: false, hash: simpleHash(f.name) }));
    }
  } else if (type === 'images') {
    // Only BMP and RAW formats are supported on e-ink display
    files = files.filter(f => !f.dir && /\.(bmp|raw)$/i.test(f.name));
  } else if (type === 'maps') {
    // Maps can be images OR tile folders (directories)
    files = files.filter(f => f.dir || /\.(jpg|jpeg|png|bmp)$/i.test(f.name));
  } else if (type === 'flashcards') {
    files = files.filter(f => !f.dir && /\.(json|csv|tsv|txt)$/i.test(f.name));
  } else if (type === 'notes') {
    files = files.filter(f => !f.dir && /\.(txt|md)$/i.test(f.name));
  }
  
  fileCache[type] = files;
  renderFileGrid(type);
  updateFileCounts();
  
  // Check if any books need processing (only for books tab)
  if (type === 'books') {
    checkBooksNeedProcessing();
  }
}

function renderFileGrid(type) {
  const gridId = type === 'flashcards' ? 'deckList' : type.slice(0,-1) + 'Grid';
  const grid = document.getElementById(gridId);
  if (!grid) return;
  
  const files = fileCache[type];
  
  if (!files.length) {
    const icons = {books:'📚', images:'🖼️', maps:'🗺️', flashcards:'🎴', notes:'📝'};
    grid.innerHTML = `<div class="file-empty"><div class="icon">${icons[type]}</div><div>No ${type} yet</div></div>`;
    return;
  }
  
  if (type === 'flashcards') {
    grid.innerHTML = files.map(f => `
      <div class="deck-card" onclick="openDeck('${f.name}')">
        <h4 style="margin:0 0 10px;font-size:16px;">🎴 ${f.name.replace('.json','')}</h4>
        <div style="font-size:12px;color:var(--text-muted);margin-bottom:12px;">
          <span>📄 ${Math.floor(f.size/100)} cards</span>
        </div>
        <div style="display:flex;gap:6px;">
          <button class="btn btn-sm btn-secondary" onclick="event.stopPropagation();editDeck('${f.name}')">✏️</button>
          <button class="btn btn-sm btn-secondary" onclick="event.stopPropagation();exportDeck('${f.name}')">📤</button>
          <button class="btn btn-sm btn-secondary" onclick="event.stopPropagation();deleteFile('flashcards','${f.name}')" style="color:var(--error)">🗑️</button>
        </div>
      </div>
    `).join('');
  } else if (type === 'maps') {
    grid.innerHTML = files.map(f => `
      <div class="file-item">
        <div class="thumb">${f.dir ? '📂' : '🗺️'}</div>
        <div class="name" title="${f.name}">${f.name}</div>
        <div class="meta">${f.dir ? 'Tile Map' : formatSize(f.size)}</div>
        <button class="delete-btn" onclick="event.stopPropagation();deleteFile('maps','${f.name}')" style="position:absolute;top:4px;right:4px;width:20px;height:20px;border:none;background:var(--error);color:white;border-radius:50%;cursor:pointer;font-size:12px;line-height:1;">×</button>
      </div>
    `).join('');
  } else if (type === 'books') {
    // Special rendering for books with cover images and metadata
    grid.innerHTML = files.map(f => {
      // Use download API to fetch covers from SD card
      const coverUrl = f.isProcessed && f.hash ? `/api/download?path=/.sumi/books/${f.hash}/cover_thumb.jpg` : null;
      
      // Format reading time
      const formatReadTime = (mins) => {
        if (!mins) return '';
        const hrs = Math.floor(mins / 60);
        const m = mins % 60;
        return hrs > 0 ? `${hrs}h ${m}m` : `${m}m`;
      };
      
      // Build metadata line
      let metaInfo = [];
      if (f.isProcessed) {
        if (f.totalChapters) metaInfo.push(`${f.totalChapters} ch`);
        if (f.estimatedPages) metaInfo.push(`~${f.estimatedPages} pg`);
        // Always show image count for processed books
        metaInfo.push(`${f.totalImages || 0} img`);
        if (f.estimatedReadingMins) metaInfo.push(formatReadTime(f.estimatedReadingMins));
      } else {
        metaInfo.push(formatSize(f.size));
      }
      
      // Progress indicator
      let progressBar = '';
      if (f.isProcessed && f.currentChapter !== undefined && f.totalChapters > 0) {
        const progressPct = Math.round((f.currentChapter / f.totalChapters) * 100);
        progressBar = `<div style="height:3px;background:#e9ecef;border-radius:2px;margin-top:4px;overflow:hidden;">
          <div style="height:100%;width:${progressPct}%;background:#007bff;"></div>
        </div>
        <div style="font-size:9px;color:#007bff;margin-top:2px;">${progressPct}% complete</div>`;
      }
      
      const processedBadge = f.isProcessed 
        ? `<div style="position:absolute;top:6px;left:6px;background:#28a745;color:white;border-radius:50%;width:20px;height:20px;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:bold;box-shadow:0 2px 4px rgba(0,0,0,0.3);">✓</div>`
        : `<div style="position:absolute;top:6px;left:6px;background:#ffc107;color:#333;border-radius:4px;padding:2px 6px;font-size:8px;font-weight:600;box-shadow:0 1px 3px rgba(0,0,0,0.2);">PROCESS</div>`;
      
      const thumbContent = coverUrl
        ? `<img src="${coverUrl}" onerror="this.parentElement.innerHTML='${getBookIcon(f.name)}'" style="width:100%;height:100%;object-fit:cover;">`
        : `<div style="display:flex;align-items:center;justify-content:center;width:100%;height:100%;font-size:32px;background:#f0f0f0;">${getBookIcon(f.name)}</div>`;
      
      // Title: use metadata title or filename
      const displayTitle = f.title || f.name.replace(/\.epub$/i, '');
      const displayAuthor = f.author && f.author !== 'Unknown' ? f.author : '';
      
      return `
        <div class="file-item" style="position:relative;">
          <div class="thumb" style="position:relative;background:#e9ecef;border-radius:6px;overflow:hidden;box-shadow:0 2px 8px rgba(0,0,0,0.1);aspect-ratio:2/3;min-height:100px;">
            ${thumbContent}
            ${processedBadge}
          </div>
          <div class="name" title="${f.name}" style="font-weight:600;font-size:11px;margin-top:6px;line-height:1.3;">${displayTitle.substring(0, 30)}${displayTitle.length > 30 ? '...' : ''}</div>
          ${displayAuthor ? `<div style="font-size:10px;color:#666;margin-top:2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">${displayAuthor}</div>` : ''}
          <div class="meta" style="font-size:10px;color:#888;margin-top:3px;">${metaInfo.join(' • ')}</div>
          ${progressBar}
          <button class="delete-btn" onclick="event.stopPropagation();deleteFile('books','${f.name}')" style="position:absolute;top:4px;right:4px;width:20px;height:20px;border:none;background:var(--error);color:white;border-radius:50%;cursor:pointer;font-size:12px;line-height:1;">×</button>
        </div>
      `;
    }).join('');
  } else {
    grid.innerHTML = files.map(f => `
      <div class="file-item" onclick="openFile('${type}','${f.name}')">
        <div class="thumb">${type === 'images' ? `<img src="/images/${f.name}" onerror="this.parentElement.innerHTML='🖼️'" style="width:100%;height:100%;object-fit:cover;">` : '📝'}</div>
        <div class="name" title="${f.name}">${f.name}</div>
        <div class="meta">${formatSize(f.size)}</div>
        <button class="delete-btn" onclick="event.stopPropagation();deleteFile('${type}','${f.name}')" style="position:absolute;top:4px;right:4px;width:20px;height:20px;border:none;background:var(--error);color:white;border-radius:50%;cursor:pointer;font-size:12px;line-height:1;">×</button>
      </div>
    `).join('');
  }
}

function getBookIcon(name) {
  const ext = name.split('.').pop().toLowerCase();
  const icons = {epub:'📕', pdf:'📄', txt:'📃', mobi:'📗'};
  return icons[ext] || '📖';
}

function openFile(type, name) {
  // Open file in reader/viewer
  if (type === 'books') {
    window.location.href = `/reader?file=${encodeURIComponent(name)}`;
  } else if (type === 'images') {
    window.open(`/images/${name}`, '_blank');
  } else if (type === 'notes') {
    window.location.href = `/notes?file=${encodeURIComponent(name)}`;
  }
}

function openDeck(name) {
  window.location.href = `/flashcards?deck=${encodeURIComponent(name)}`;
}

function editDeck(name) {
  window.location.href = `/flashcards/edit?deck=${encodeURIComponent(name)}`;
}

async function exportDeck(name) {
  toast('Exporting deck...', 'info');
  try {
    const response = await fetch(`/api/flashcards/export?deck=${encodeURIComponent(name)}`);
    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = name.replace('.json', '.txt');
    a.click();
    URL.revokeObjectURL(url);
    toast('Deck exported!', 'success');
  } catch(e) {
    toast('Export failed', 'error');
  }
}

async function deleteFile(type, name) {
  if (!confirm(`Delete ${name}?`)) return;
  const paths = {books:'/books', images:'/images', flashcards:'/flashcards', notes:'/notes'};
  try {
    await api('/api/files/delete', 'POST', {path: paths[type] + '/' + name});
    toast('Deleted: ' + name, 'success');
    refreshFiles(type);
  } catch(e) {
    toast('Delete failed', 'error');
  }
}

function updateFileCounts() {
  for (const type of ['books','images','maps','flashcards','notes']) {
    const el = document.getElementById('count' + type.charAt(0).toUpperCase() + type.slice(1));
    if (el) el.textContent = fileCache[type].length;
  }
}

function formatSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024*1024) return Math.round(bytes/1024) + ' KB';
  return (bytes/(1024*1024)).toFixed(1) + ' MB';
}

async function uploadFiles(files, type) {
  const paths = {books:'/books', images:'/images', maps:'/maps', flashcards:'/flashcards', notes:'/notes'};
  for (const file of files) {
    const formData = new FormData();
    // Path must come BEFORE file for multipart parsing
    formData.append('path', paths[type]);
    formData.append('file', file);
    try {
      await fetch('/api/files/upload', {method:'POST', body:formData});
      toast('Uploaded: ' + file.name, 'success');
    } catch(e) {
      toast('Upload failed: ' + file.name, 'error');
    }
  }
  refreshFiles(type);
}

// =============================================================================
// FULL EPUB PRE-PROCESSING SYSTEM
// =============================================================================
// This processes EPUBs entirely in the browser:
// - Extracts metadata (title, author)
// - Extracts and optimizes cover images
// - Converts all chapters from HTML to clean plain text
// - Uploads everything as a ready-to-read cache
// The ESP32 never has to decompress ZIPs or parse HTML!

let JSZip = null;

async function loadJSZip() {
  if (JSZip) return true;
  
  try {
    const script = document.createElement('script');
    script.src = 'https://cdnjs.cloudflare.com/ajax/libs/jszip/3.10.1/jszip.min.js';
    
    await new Promise((resolve, reject) => {
      script.onload = resolve;
      script.onerror = () => reject(new Error('CDN unreachable'));
      document.head.appendChild(script);
      // Timeout after 10 seconds
      setTimeout(() => reject(new Error('Timeout')), 10000);
    });
    
    JSZip = window.JSZip;
    console.log('[PORTAL] JSZip loaded');
    return true;
  } catch (e) {
    console.error('[PORTAL] Failed to load JSZip:', e);
    // Show helpful error
    toast('No internet access - connect your device to WiFi with internet', 'error');
    return false;
  }
}

function handleBookDrop(e) {
  e.preventDefault();
  e.currentTarget.classList.remove('dragover');
  uploadBooks(e.dataTransfer.files);
}

async function uploadBooks(files) {
  if (!files || files.length === 0) return;
  
  const progressDiv = document.getElementById('uploadProgress');
  const statusText = document.getElementById('uploadStatusText');
  const detailText = document.getElementById('uploadDetailText');
  const progressBar = document.getElementById('uploadProgressBar');
  
  progressDiv.style.display = 'block';
  
  const epubFiles = [];
  const otherFiles = [];
  
  for (const file of files) {
    if (file.name.toLowerCase().endsWith('.epub')) {
      epubFiles.push(file);
    } else {
      otherFiles.push(file);
    }
  }
  
  // Upload non-EPUB files normally
  for (const file of otherFiles) {
    statusText.textContent = `Uploading ${file.name}...`;
    detailText.textContent = '';
    await uploadFileSimple(file, '/books');
    toast('Uploaded: ' + file.name, 'success');
  }
  
  // Full pre-processing for EPUBs
  if (epubFiles.length > 0) {
    statusText.textContent = 'Loading processor...';
    detailText.textContent = 'Preparing to extract and optimize your books';
    
    const hasJSZip = await loadJSZip();
    
    if (!hasJSZip) {
      toast('No internet - uploading without pre-processing (slower reading)', 'warning');
      for (const file of epubFiles) {
        await uploadFileSimple(file, '/books');
      }
    } else {
      for (let i = 0; i < epubFiles.length; i++) {
        const file = epubFiles[i];
        progressBar.style.width = Math.round((i / epubFiles.length) * 100) + '%';
        statusText.textContent = `Processing ${file.name} (${i+1}/${epubFiles.length})`;
        
        try {
          await fullProcessEpub(file, (step, subProgress) => {
            detailText.textContent = step;
            if (subProgress !== undefined) {
              const base = (i / epubFiles.length) * 100;
              const chunk = (1 / epubFiles.length) * 100;
              progressBar.style.width = (base + chunk * subProgress) + '%';
            }
          });
          toast('✓ ' + file.name, 'success');
        } catch (e) {
          console.error('EPUB processing failed:', e);
          toast('Error: ' + file.name, 'error');
          // Fallback: upload raw
          await uploadFileSimple(file, '/books');
        }
      }
    }
  }
  
  progressBar.style.width = '100%';
  statusText.textContent = 'Complete!';
  detailText.textContent = '';
  
  setTimeout(() => {
    progressDiv.style.display = 'none';
    progressBar.style.width = '0%';
  }, 1500);
  
  refreshFiles('books');
  checkBooksNeedProcessing();
}

async function uploadFileSimple(file, path) {
  const formData = new FormData();
  // Path must come BEFORE file for multipart parsing
  formData.append('path', path);
  formData.append('file', file);
  await fetch('/api/files/upload', {method:'POST', body:formData});
}

// =============================================================================
// FULL EPUB PROCESSOR
// =============================================================================
async function fullProcessEpub(file, onProgress, onCoverReady) {
  const hash = simpleHash(file.name);
  const cacheDir = `/.sumi/books/${hash}`;
  
  onProgress('Reading EPUB...', 0);
  const arrayBuffer = await file.arrayBuffer();
  const zip = await JSZip.loadAsync(arrayBuffer);
  
  // Step 1: Parse OPF for metadata and spine
  onProgress('Parsing metadata...', 0.05);
  const { opfPath, opfContent, opfDir } = await findOPF(zip);
  if (!opfContent) throw new Error('Invalid EPUB: No OPF found');
  
  const metadata = parseMetadata(opfContent);
  const manifest = parseManifest(opfContent);
  const spine = parseSpine(opfContent);
  const language = metadata.language || 'en';
  
  console.log('[PORTAL] Metadata:', metadata);
  console.log('[PORTAL] Spine items:', spine.length);
  console.log('[PORTAL] Language:', language);
  
  // Step 2: Extract TOC
  onProgress('Extracting TOC...', 0.08);
  const toc = await extractToc(zip, opfContent, opfDir, manifest);
  console.log('[PORTAL] TOC entries:', toc.length);
  
  // Step 3: Extract cover
  onProgress('Extracting cover...', 0.1);
  const coverPath = await findCoverInEpub(zip, opfContent, opfDir);
  let thumbBlob = null, fullBlob = null;
  
  if (coverPath) {
    const coverFile = zip.file(coverPath);
    if (coverFile) {
      const coverData = await coverFile.async('blob');
      thumbBlob = await processImage(coverData, 120, 180, 0.75);
      fullBlob = await processImage(coverData, 300, 450, 0.8);
      
      // Immediately show cover preview if callback provided
      if (onCoverReady && thumbBlob) {
        const coverUrl = URL.createObjectURL(thumbBlob);
        onCoverReady(coverUrl);
      }
    }
  }
  
  // Step 4: Extract and convert all chapters with enhanced processing
  onProgress('Extracting chapters...', 0.15);
  const chapters = [];
  let totalChars = 0;
  let totalWords = 0;
  let imageIndex = { count: 0 };  // Shared counter for all images in book
  
  // Ensure images directory exists
  await ensureDirectory(cacheDir + '/images');
  
  for (let i = 0; i < spine.length; i++) {
    const idref = spine[i];
    const item = manifest[idref];
    if (!item) continue;
    
    const progress = 0.15 + (i / spine.length) * 0.65;
    onProgress(`Chapter ${i+1}/${spine.length}...`, progress);
    
    const chapterPath = resolveHref(item.href, opfDir);
    const chapterFile = zip.file(chapterPath);
    
    // Get directory of chapter file for resolving relative image paths
    const chapterDir = chapterPath.includes('/') 
      ? chapterPath.substring(0, chapterPath.lastIndexOf('/') + 1)
      : '';
    
    if (chapterFile) {
      try {
        let html = await chapterFile.async('string');
        
        console.log(`[PORTAL] Processing chapter ${i}: ${chapterPath}, html length: ${html.length}, chapterDir: ${chapterDir}`);
        
        // Extract inline images BEFORE converting to plain text
        // Pass chapterDir so image paths resolve correctly (images are relative to chapter, not OPF)
        const images = await extractInlineImages(zip, html, chapterDir, cacheDir, imageIndex);
        console.log(`[PORTAL] extractInlineImages returned ${images.length} images`);
        
        // Replace <img> tags with markers
        for (const img of images) {
          html = html.replace(img.original, img.marker);
        }
        
        // Convert HTML to plain text with rich markers
        let text = htmlToPlainText(html);
        
        // PROTECT image markers from text processing
        // Extract [!IMG:...] markers before typography/hyphenation mangles them
        const imgMarkers = [];
        text = text.replace(/\[!IMG:[^\]]+\]/g, (match) => {
          imgMarkers.push(match);
          return `\x00IMGMARKER${imgMarkers.length - 1}\x00`;
        });
        
        // Apply smart typography (quotes, em-dashes, ellipsis)
        text = applySmartTypography(text);
        
        // Add soft hyphens for better line breaking
        text = addSoftHyphens(text, language);
        
        // RESTORE image markers (unchanged)
        text = text.replace(/\x00IMGMARKER(\d+)\x00/g, (m, idx) => {
          return imgMarkers[parseInt(idx)];
        });
        
        if (text.trim().length > 0) {
          const charCount = text.length;
          const wordCount = text.split(/\s+/).filter(w => w.length > 0).length;
          totalChars += charCount;
          totalWords += wordCount;
          
          chapters.push({
            index: chapters.length,
            idref: idref,
            href: item.href,
            text: text,
            chars: charCount,
            words: wordCount
          });
        }
      } catch (e) {
        console.warn('[PORTAL] Failed to process chapter:', chapterPath, e);
      }
    }
    
    // Yield to UI
    if (i % 5 === 0) await new Promise(r => setTimeout(r, 0));
  }
  
  console.log('[PORTAL] Extracted', chapters.length, 'chapters,', totalWords, 'words,', imageIndex.count, 'images');
  
  // Estimate pages: ~1800 chars per e-ink page (conservative estimate for 800x480)
  const CHARS_PER_PAGE = 1800;
  const estimatedPages = Math.ceil(totalChars / CHARS_PER_PAGE);
  
  // Step 5: Build meta.json with enhanced metadata
  const meta = {
    version: 5,  // Bumped version for enhanced metadata extraction
    hash: hash,
    filename: file.name,
    fileSize: file.size,
    
    // Core metadata
    title: metadata.title || file.name.replace('.epub', ''),
    author: metadata.author || 'Unknown',
    language: language,
    
    // Extended metadata (if available)
    publisher: metadata.publisher || null,
    description: metadata.description || null,
    pubYear: metadata.pubYear || null,
    pubDate: metadata.pubDate || null,
    subjects: metadata.subjects || null,
    isbn: metadata.isbn || null,
    series: metadata.series || null,
    seriesPosition: metadata.seriesPosition || null,
    epubVersion: metadata.epubVersion || null,
    
    // Content stats
    totalChapters: chapters.length,
    totalChars: totalChars,
    totalWords: totalWords,
    totalImages: imageIndex.count,
    estimatedPages: estimatedPages,
    estimatedReadingMins: Math.ceil(totalWords / 250),
    
    // Processing info
    hasToc: toc.length > 0,
    hasHyphenation: true,
    hasSmartTypography: true,
    processedAt: Date.now(),
    
    // Chapter details
    chapters: chapters.map((c, i) => ({
      index: i,
      file: `ch_${String(i).padStart(3, '0')}.txt`,
      chars: c.chars,
      words: c.words
    }))
  };
  
  // Map TOC entries to chapter indices
  const tocMapped = toc.map((entry, i) => {
    // Try to find matching chapter by href
    let chapterIndex = i;
    for (let j = 0; j < chapters.length; j++) {
      if (chapters[j].href && entry.href && chapters[j].href.includes(entry.href)) {
        chapterIndex = j;
        break;
      }
    }
    return {
      title: entry.title,
      chapter: Math.min(chapterIndex, chapters.length - 1)
    };
  });
  
  // Step 6: Upload everything
  onProgress('Uploading...', 0.82);
  
  // Create cache directory
  await ensureDirectory(cacheDir);
  
  // Upload meta.json
  await uploadTextFile(cacheDir + '/meta.json', JSON.stringify(meta, null, 2));
  
  // Upload toc.json if we have TOC entries
  if (tocMapped.length > 0) {
    await uploadTextFile(cacheDir + '/toc.json', JSON.stringify(tocMapped, null, 2));
    console.log('[PORTAL] Saved TOC with', tocMapped.length, 'entries');
  }
  
  // Upload covers
  if (thumbBlob) {
    await uploadBlobFile(cacheDir + '/cover_thumb.jpg', thumbBlob);
  }
  if (fullBlob) {
    await uploadBlobFile(cacheDir + '/cover_full.jpg', fullBlob);
  }
  
  // Upload chapters
  for (let i = 0; i < chapters.length; i++) {
    const progress = 0.88 + (i / chapters.length) * 0.08;
    onProgress(`Saving ${i+1}/${chapters.length}...`, progress);
    
    const filename = `ch_${String(i).padStart(3, '0')}.txt`;
    await uploadTextFile(cacheDir + '/' + filename, chapters[i].text);
    
    // Give ESP32 time to process files
    await new Promise(r => setTimeout(r, 50));
  }
  
  // Upload original EPUB too (for backup/re-processing)
  onProgress('Saving EPUB...', 0.98);
  await uploadFileSimple(file, '/books');
  
  onProgress('Done!', 1);
  
  // Return the metadata for display
  return meta;
}

// =============================================================================
// EPUB PARSING HELPERS
// =============================================================================
async function findOPF(zip) {
  let opfPath = null;
  
  // Try container.xml first
  try {
    const container = await zip.file('META-INF/container.xml')?.async('string');
    if (container) {
      const match = container.match(/full-path="([^"]+\.opf)"/i);
      if (match) opfPath = match[1];
    }
  } catch (e) {}
  
  // Fallback: find any .opf
  if (!opfPath) {
    for (const path of Object.keys(zip.files)) {
      if (path.toLowerCase().endsWith('.opf')) {
        opfPath = path;
        break;
      }
    }
  }
  
  if (!opfPath) return { opfPath: null, opfContent: null, opfDir: '' };
  
  const opfContent = await zip.file(opfPath)?.async('string');
  const opfDir = opfPath.includes('/') ? opfPath.substring(0, opfPath.lastIndexOf('/') + 1) : '';
  
  return { opfPath, opfContent, opfDir };
}

function parseMetadata(opf) {
  const meta = {};
  
  // Title
  const titleMatch = opf.match(/<dc:title[^>]*>([^<]+)<\/dc:title>/i);
  if (titleMatch) meta.title = decodeEntities(titleMatch[1].trim());
  
  // Author (may have multiple)
  const authorMatches = opf.matchAll(/<dc:creator[^>]*>([^<]+)<\/dc:creator>/gi);
  const authors = [];
  for (const m of authorMatches) {
    authors.push(decodeEntities(m[1].trim()));
  }
  if (authors.length > 0) meta.author = authors.join(', ');
  
  // Language
  const langMatch = opf.match(/<dc:language[^>]*>([^<]+)<\/dc:language>/i);
  if (langMatch) meta.language = langMatch[1].trim().substring(0, 5); // "en-US" -> "en-US"
  
  // Publisher
  const pubMatch = opf.match(/<dc:publisher[^>]*>([^<]+)<\/dc:publisher>/i);
  if (pubMatch) meta.publisher = decodeEntities(pubMatch[1].trim());
  
  // Description (may contain CDATA or HTML)
  const descMatch = opf.match(/<dc:description[^>]*>([\s\S]*?)<\/dc:description>/i);
  if (descMatch) {
    let desc = descMatch[1].trim();
    // Remove CDATA wrapper if present
    desc = desc.replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1');
    // Strip HTML tags
    desc = desc.replace(/<[^>]+>/g, ' ');
    // Normalize whitespace
    desc = desc.replace(/\s+/g, ' ').trim();
    // Decode entities
    desc = decodeEntities(desc);
    // Truncate if very long
    if (desc.length > 500) desc = desc.substring(0, 497) + '...';
    meta.description = desc;
  }
  
  // Publication date
  const dateMatch = opf.match(/<dc:date[^>]*>([^<]+)<\/dc:date>/i);
  if (dateMatch) {
    const dateStr = dateMatch[1].trim();
    // Extract year from various formats: "2023", "2023-01-15", "January 15, 2023"
    const yearMatch = dateStr.match(/(\d{4})/);
    if (yearMatch) meta.pubYear = parseInt(yearMatch[1]);
    meta.pubDate = dateStr;
  }
  
  // Subjects/genres (may have multiple)
  const subjectMatches = opf.matchAll(/<dc:subject[^>]*>([^<]+)<\/dc:subject>/gi);
  const subjects = [];
  for (const m of subjectMatches) {
    const subj = decodeEntities(m[1].trim());
    if (subj && subj.length < 50) subjects.push(subj);
  }
  if (subjects.length > 0) meta.subjects = subjects.slice(0, 5); // Max 5
  
  // ISBN (identifier with scheme="ISBN" or type containing ISBN)
  const identMatches = opf.matchAll(/<dc:identifier[^>]*>([^<]+)<\/dc:identifier>/gi);
  for (const m of identMatches) {
    const id = m[1].trim();
    const tag = m[0];
    // Check for ISBN in tag attributes or value
    if (tag.toLowerCase().includes('isbn') || /^(97[89])?\d{9}[\dXx]$/.test(id.replace(/-/g, ''))) {
      meta.isbn = id.replace(/-/g, '');
      break;
    }
  }
  
  // Series info (EPUB3 meta properties)
  const seriesMatch = opf.match(/<meta[^>]*property=["']belongs-to-collection["'][^>]*>([^<]+)<\/meta>/i);
  if (seriesMatch) meta.series = decodeEntities(seriesMatch[1].trim());
  
  // Series position
  const seriesPosMatch = opf.match(/<meta[^>]*property=["']group-position["'][^>]*>([^<]+)<\/meta>/i);
  if (seriesPosMatch) meta.seriesPosition = parseInt(seriesPosMatch[1].trim());
  
  // EPUB version
  const versionMatch = opf.match(/<package[^>]*version=["']([^"']+)["']/i);
  if (versionMatch) meta.epubVersion = versionMatch[1];
  
  return meta;
}

function parseManifest(opf) {
  const manifest = {};
  const itemRegex = /<item\s+([^>]+)>/gi;
  let match;
  
  while ((match = itemRegex.exec(opf)) !== null) {
    const attrs = match[1];
    const id = attrs.match(/id=["']([^"']+)["']/i)?.[1];
    const href = attrs.match(/href=["']([^"']+)["']/i)?.[1];
    const mediaType = attrs.match(/media-type=["']([^"']+)["']/i)?.[1];
    
    if (id && href) {
      manifest[id] = { href: decodeURIComponent(href), mediaType };
    }
  }
  
  return manifest;
}

function parseSpine(opf) {
  const spine = [];
  const spineMatch = opf.match(/<spine[^>]*>([\s\S]*?)<\/spine>/i);
  
  if (spineMatch) {
    const itemrefRegex = /<itemref\s+([^>]+)>/gi;
    let match;
    
    while ((match = itemrefRegex.exec(spineMatch[1])) !== null) {
      const idref = match[1].match(/idref=["']([^"']+)["']/i)?.[1];
      if (idref) spine.push(idref);
    }
  }
  
  return spine;
}

// =============================================================================
// TOC EXTRACTION
// =============================================================================
async function extractToc(zip, opfContent, opfDir, manifest) {
  const toc = [];
  
  // Try NCX first (EPUB 2)
  const ncxId = opfContent.match(/<spine[^>]*toc=["']([^"']+)["']/i)?.[1];
  if (ncxId && manifest[ncxId]) {
    const ncxPath = resolveHref(manifest[ncxId].href, opfDir);
    const ncxFile = zip.file(ncxPath);
    if (ncxFile) {
      const ncx = await ncxFile.async('string');
      const navPoints = ncx.match(/<navPoint[^>]*>[\s\S]*?<\/navPoint>/gi) || [];
      
      navPoints.forEach((np, i) => {
        const labelMatch = np.match(/<text>([^<]+)<\/text>/i);
        const srcMatch = np.match(/<content[^>]*src=["']([^"'#]+)/i);
        if (labelMatch) {
          toc.push({
            title: decodeEntities(labelMatch[1].trim()),
            href: srcMatch ? srcMatch[1] : '',
            chapter: i
          });
        }
      });
    }
  }
  
  // Try nav.xhtml (EPUB 3)
  if (toc.length === 0) {
    for (const [id, item] of Object.entries(manifest)) {
      if (item.href.includes('nav') || item.mediaType === 'application/xhtml+xml') {
        const navPath = resolveHref(item.href, opfDir);
        const navFile = zip.file(navPath);
        if (navFile) {
          const nav = await navFile.async('string');
          const tocMatch = nav.match(/<nav[^>]*epub:type=["']toc["'][^>]*>([\s\S]*?)<\/nav>/i);
          if (tocMatch) {
            const links = tocMatch[1].match(/<a[^>]*>[\s\S]*?<\/a>/gi) || [];
            links.forEach((link, i) => {
              const textMatch = link.match(/>([^<]+)</);
              const hrefMatch = link.match(/href=["']([^"'#]+)/i);
              if (textMatch) {
                toc.push({
                  title: decodeEntities(textMatch[1].trim()),
                  href: hrefMatch ? hrefMatch[1] : '',
                  chapter: i
                });
              }
            });
            if (toc.length > 0) break;
          }
        }
      }
    }
  }
  
  return toc;
}

// =============================================================================
// SMART TYPOGRAPHY
// =============================================================================
function applySmartTypography(text) {
  // First, temporarily protect asterisk markers from quote processing
  // IMPORTANT: Capture bold (**) BEFORE italic (*) to avoid conflicts
  const boldMarkers = [];
  text = text.replace(/\*\*([^*\n]+)\*\*/g, (m, content) => {
    boldMarkers.push(content);
    return `\x00BOLD${boldMarkers.length - 1}\x00`;
  });
  const italicMarkers = [];
  text = text.replace(/\*([^*\n]+)\*/g, (m, content) => {
    italicMarkers.push(content);
    return `\x00ITALIC${italicMarkers.length - 1}\x00`;
  });
  
  // Preserve existing curly quotes (some EPUBs already have them)
  // Skip quote processing if text already has curly quotes
  const hasExistingCurlyQuotes = /[\u201C\u201D\u2018\u2019]/.test(text);
  
  if (!hasExistingCurlyQuotes) {
    // Smart quotes (straight to curly)
    // Opening double quotes: after space, newline, start, or opening bracket/paren
    text = text.replace(/(^|[\s\n\(\[\{])"/g, "$1\u201C");
    // Closing double quotes: before space, newline, punctuation, end, or after word
    text = text.replace(/"([\s\n.,;:!?\)\]\}]|$)/g, "\u201D$1");
    // Quote after word (likely closing)
    text = text.replace(/(\w)"/g, "$1\u201D");
    // Remaining double quotes (likely closing)
    text = text.replace(/"/g, "\u201D");
    
    // Single quotes / apostrophes
    // Opening: after space, newline, or opening bracket
    text = text.replace(/(^|[\s\n\(\[\{])'/g, "$1\u2018");
  }
  
  // Apostrophes in contractions ALWAYS need fixing (don't, won't, it's, they'll, etc)
  // This handles both straight quotes and ensures contractions work
  text = text.replace(/(\w)'(\w)/g, "$1\u2019$2");
  // Possessives ending in s (teachers', James')
  text = text.replace(/(\w)'([\s\n.,;:!?\)\]\}]|$)/g, "$1\u2019$2");
  
  if (!hasExistingCurlyQuotes) {
    // Remaining single quotes (likely closing)
    text = text.replace(/'/g, "\u2019");
  }
  
  // Em-dashes (but preserve existing proper ones)
  text = text.replace(/--/g, "\u2014");
  text = text.replace(/ - /g, "\u2014");  // Remove spaces around em-dash for tighter typography
  
  // Ellipsis
  text = text.replace(/\.\.\./g, "\u2026");
  
  // Restore bold markers first
  text = text.replace(/\x00BOLD(\d+)\x00/g, (m, idx) => {
    return '**' + boldMarkers[parseInt(idx)] + '**';
  });
  // Restore italic markers
  text = text.replace(/\x00ITALIC(\d+)\x00/g, (m, idx) => {
    return '*' + italicMarkers[parseInt(idx)] + '*';
  });
  
  // Fix common spacing issues (AFTER restoring markers)
  // Remove space before punctuation (including after italic/bold markers)
  text = text.replace(/\*\s+([.,;:!?\)\]\}])/g, "*$1");  // *word* , -> *word*,
  text = text.replace(/\*\*\s+([.,;:!?\)\]\}])/g, "**$1");  // **word** , -> **word**,
  text = text.replace(/ +([.,;:!?\)\]\}])/g, "$1");  // word , -> word,
  // Ensure space after punctuation (except at end of line or before closing markers)
  text = text.replace(/([.,;:!?])([A-Za-z])/g, "$1 $2");
  
  return text;
}

// =============================================================================
// SOFT HYPHENATION
// =============================================================================
// Simple algorithmic hyphenation - inserts soft hyphens (U+00AD) at likely break points
// Based on basic English syllable patterns
function addSoftHyphens(text, language = 'en') {
  // Only hyphenate words longer than 6 characters
  const MIN_WORD_LENGTH = 6;
  const MIN_PREFIX = 2;  // Minimum chars before hyphen
  const MIN_SUFFIX = 3;  // Minimum chars after hyphen
  
  // Common English suffixes (break before these)
  const suffixes = ['ing', 'tion', 'sion', 'ment', 'ness', 'able', 'ible', 'less', 'ful', 'ous', 'ive', 'ly'];
  
  // Common prefixes (break after these)
  const prefixes = ['pre', 'pro', 'con', 'com', 'dis', 'mis', 'sub', 'super', 'inter', 'over', 'under', 'out', 'anti', 'auto', 'semi', 'multi', 'trans', 'non', 'un', 're', 'de'];
  
  // Vowels for syllable detection
  const vowels = 'aeiouyAEIOUY';
  const isVowel = c => vowels.includes(c);
  
  // Process each word
  return text.replace(/[a-zA-Z]{7,}/g, word => {
    if (word.length < MIN_WORD_LENGTH) return word;
    
    const breakPoints = [];
    const lowerWord = word.toLowerCase();
    
    // Check prefixes
    for (const prefix of prefixes) {
      if (lowerWord.startsWith(prefix) && word.length > prefix.length + MIN_SUFFIX) {
        breakPoints.push(prefix.length);
        break;
      }
    }
    
    // Check suffixes
    for (const suffix of suffixes) {
      if (lowerWord.endsWith(suffix) && word.length > suffix.length + MIN_PREFIX) {
        breakPoints.push(word.length - suffix.length);
        break;
      }
    }
    
    // Simple syllable detection: break between consonant clusters
    // Pattern: vowel + consonant(s) + vowel = break before last consonant
    for (let i = MIN_PREFIX; i < word.length - MIN_SUFFIX; i++) {
      if (!isVowel(word[i]) && isVowel(word[i-1]) && i + 1 < word.length && isVowel(word[i+1])) {
        // consonant between vowels - potential break point
        breakPoints.push(i);
      } else if (!isVowel(word[i]) && !isVowel(word[i-1]) && i > MIN_PREFIX && isVowel(word[i-2])) {
        // two consonants after vowel - break between them
        breakPoints.push(i);
      }
    }
    
    // Remove duplicates and sort
    const uniqueBreaks = [...new Set(breakPoints)]
      .filter(p => p >= MIN_PREFIX && p <= word.length - MIN_SUFFIX)
      .sort((a, b) => a - b);
    
    // Insert soft hyphens at break points
    if (uniqueBreaks.length === 0) return word;
    
    let result = '';
    let lastPos = 0;
    for (const pos of uniqueBreaks) {
      result += word.substring(lastPos, pos) + '\u00AD';
      lastPos = pos;
    }
    result += word.substring(lastPos);
    
    return result;
  });
}

// =============================================================================
// INLINE IMAGE EXTRACTION
// =============================================================================
async function extractInlineImages(zip, html, chapterDir, cacheDir, imageIndex) {
  const images = [];
  
  console.log(`[PORTAL] extractInlineImages called - chapterDir: ${chapterDir}, html length: ${html.length}`);
  
  // Target size for e-ink display (480x800 with margins)
  const MAX_IMG_WIDTH = 420;
  const MAX_IMG_HEIGHT = 500;  // Leave room for text on e-ink (800px screen)
  const MIN_IMG_SIZE = 80;  // Skip icons/spacers smaller than this
  
  // Find all image references - multiple patterns for different EPUB formats
  const allMatches = [];
  
  // Pattern 1: Standard <img ... src="..."> - more flexible regex
  // Match img tags and extract src attribute regardless of position
  const imgTags = html.match(/<img[^>]+>/gi) || [];
  console.log(`[PORTAL] Found ${imgTags.length} <img> tags`);
  
  for (const tag of imgTags) {
    const srcMatch = tag.match(/src\s*=\s*["']([^"']+)["']/i);
    if (srcMatch) {
      allMatches.push({ fullMatch: tag, src: srcMatch[1], index: html.indexOf(tag) });
      console.log(`[PORTAL] Found img src: ${srcMatch[1]}`);
    }
  }
  
  // Pattern 2: SVG <image> with href or xlink:href
  const svgImageTags = html.match(/<image[^>]+>/gi) || [];
  for (const tag of svgImageTags) {
    const hrefMatch = tag.match(/(?:xlink:)?href\s*=\s*["']([^"']+)["']/i);
    if (hrefMatch) {
      allMatches.push({ fullMatch: tag, src: hrefMatch[1], index: html.indexOf(tag) });
      console.log(`[PORTAL] Found svg image href: ${hrefMatch[1]}`);
    }
  }
  
  // Pattern 3: <figure> containing images (Standard Ebooks wraps in figure)
  const figureTags = html.match(/<figure[^>]*>[\s\S]*?<\/figure>/gi) || [];
  console.log(`[PORTAL] Found ${figureTags.length} <figure> tags`);
  
  for (const figure of figureTags) {
    // Look for img inside figure
    const imgMatch = figure.match(/<img[^>]+>/i);
    if (imgMatch) {
      const srcMatch = imgMatch[0].match(/src\s*=\s*["']([^"']+)["']/i);
      if (srcMatch && !allMatches.some(m => m.src === srcMatch[1])) {
        allMatches.push({ fullMatch: imgMatch[0], src: srcMatch[1], index: html.indexOf(imgMatch[0]) });
        console.log(`[PORTAL] Found figure img src: ${srcMatch[1]}`);
      }
    }
  }
  
  // Sort by position and remove duplicates
  allMatches.sort((a, b) => a.index - b.index);
  const seen = new Set();
  const uniqueMatches = allMatches.filter(m => {
    if (seen.has(m.src)) return false;
    seen.add(m.src);
    return true;
  });
  
  console.log(`[PORTAL] Total unique images to extract: ${uniqueMatches.length}`);
  
  for (const match of uniqueMatches) {
    const src = match.src;
    
    // Skip data: URLs
    if (src.startsWith('data:')) continue;
    
    // Resolve the path relative to chapter directory
    const imgPath = resolveHref(decodeURIComponent(src), chapterDir);
    console.log(`[PORTAL] Resolving image: ${src} -> ${imgPath}`);
    
    // Try to find the file with various path formats
    let imgFile = zip.file(imgPath);
    if (!imgFile) imgFile = zip.file(imgPath.replace(/^\//, ''));
    if (!imgFile) {
      // Case-insensitive search
      const files = Object.keys(zip.files);
      const lowerPath = imgPath.toLowerCase().replace(/^\//, '');
      const found = files.find(f => f.toLowerCase() === lowerPath);
      if (found) imgFile = zip.file(found);
    }
    
    if (!imgFile) {
      console.warn('[PORTAL] Image not found in zip:', imgPath);
      continue;
    }
    
    console.log(`[PORTAL] Found image file: ${imgPath}`);
    
    try {
      const isSvg = imgPath.toLowerCase().endsWith('.svg');
      let imgBlob;
      
      if (isSvg) {
        // Convert SVG to PNG
        const svgText = await imgFile.async('string');
        imgBlob = await svgToPng(svgText, MAX_IMG_WIDTH, MAX_IMG_HEIGHT);
        if (!imgBlob) {
          console.warn('[PORTAL] Failed to convert SVG:', imgPath);
          continue;
        }
      } else {
        imgBlob = await imgFile.async('blob');
      }
      
      // Check original dimensions
      const origDims = await getImageDimensions(imgBlob);
      
      // Skip tiny images
      if (origDims.width < MIN_IMG_SIZE && origDims.height < MIN_IMG_SIZE) {
        console.log('[PORTAL] Skipping small image:', imgPath, `${origDims.width}x${origDims.height}`);
        continue;
      }
      
      // Resize/convert for e-ink (grayscale JPG)
      const resized = await processImage(imgBlob, MAX_IMG_WIDTH, MAX_IMG_HEIGHT, 0.85);
      
      if (resized && resized.size > 100) {
        const imgFilename = `img_${String(imageIndex.count).padStart(3, '0')}.jpg`;
        imageIndex.count++;
        
        // Upload image with delay for ESP32 recovery
        await uploadBlobFile(`${cacheDir}/images/${imgFilename}`, resized);
        
        // Extra delay between images to prevent overwhelming ESP32
        await new Promise(r => setTimeout(r, 100));
        
        const dims = await getImageDimensions(resized);
        
        images.push({
          original: match.fullMatch,
          marker: `[!IMG:${imgFilename},w=${dims.width},h=${dims.height}]`,
          filename: imgFilename
        });
        
        console.log(`[PORTAL] Extracted: ${imgFilename} (${dims.width}x${dims.height}) from ${imgPath}`);
      }
    } catch (e) {
      console.warn('[PORTAL] Failed to extract:', imgPath, e.message || e);
    }
  }
  
  return images;
}

// Convert SVG to PNG using canvas
async function svgToPng(svgText, maxWidth, maxHeight) {
  return new Promise((resolve) => {
    const img = new Image();
    
    img.onload = () => {
      let w = img.width || 400;
      let h = img.height || 400;
      
      // Scale to fit
      if (w > maxWidth) { h = h * maxWidth / w; w = maxWidth; }
      if (h > maxHeight) { w = w * maxHeight / h; h = maxHeight; }
      
      const canvas = document.createElement('canvas');
      canvas.width = Math.round(w);
      canvas.height = Math.round(h);
      
      const ctx = canvas.getContext('2d');
      ctx.fillStyle = 'white';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
      
      canvas.toBlob(blob => {
        URL.revokeObjectURL(img.src);
        resolve(blob);
      }, 'image/png', 0.9);
    };
    
    img.onerror = () => {
      URL.revokeObjectURL(img.src);
      resolve(null);
    };
    
    // Create blob URL from SVG
    const svgBlob = new Blob([svgText], { type: 'image/svg+xml' });
    img.src = URL.createObjectURL(svgBlob);
  });
}

async function getImageDimensions(blob) {
  return new Promise((resolve) => {
    const img = new Image();
    img.onload = () => {
      URL.revokeObjectURL(img.src);
      resolve({ width: img.width, height: img.height });
    };
    img.onerror = () => {
      URL.revokeObjectURL(img.src);
      resolve({ width: 200, height: 150 }); // Default
    };
    img.src = URL.createObjectURL(blob);
  });
}

function resolveHref(href, baseDir) {
  if (href.startsWith('/')) return href.substring(1);
  
  let dir = baseDir;
  let path = href;
  
  while (path.startsWith('../')) {
    path = path.substring(3);
    const lastSlash = dir.lastIndexOf('/', dir.length - 2);
    dir = lastSlash >= 0 ? dir.substring(0, lastSlash + 1) : '';
  }
  
  return dir + path;
}

function decodeEntities(str) {
  const entities = {
    '&amp;': '&', '&lt;': '<', '&gt;': '>', '&quot;': '"', '&apos;': "'",
    '&#39;': "'", '&#x27;': "'", '&nbsp;': ' '
  };
  return str.replace(/&[^;]+;/g, m => entities[m] || m);
}

// =============================================================================
// HTML TO PLAIN TEXT CONVERTER
// =============================================================================
function htmlToPlainText(html) {
  // Remove scripts, styles, head
  let text = html
    .replace(/<script[\s\S]*?<\/script>/gi, '')
    .replace(/<style[\s\S]*?<\/style>/gi, '')
    .replace(/<head[\s\S]*?<\/head>/gi, '');
  
  // Extract body content if present
  const bodyMatch = text.match(/<body[^>]*>([\s\S]*)<\/body>/i);
  if (bodyMatch) text = bodyMatch[1];
  
  // === RICH TEXT MARKERS ===
  
  // Headers: convert to # prefix (before removing tags)
  // Process headers in order: h1, h2, h3, etc.
  text = text.replace(/<h([1-6])[^>]*>([\s\S]*?)<\/h\1>/gi, (m, level, content) => {
    // Strip nested tags but preserve word boundaries
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    if (!clean) return '';
    
    // Check if this is a combined "CHAPTER X. Title" format
    // Split into separate lines for better e-ink display
    const chapterMatch = clean.match(/^(CHAPTER\s+[IVXLCDM\d]+\.?)\s*(.*)$/i);
    if (chapterMatch && chapterMatch[2]) {
      // Return chapter number and title on separate lines
      return '\n\n# ' + chapterMatch[1].trim() + '\n# ' + chapterMatch[2].trim() + '\n\n';
    }
    
    return '\n\n# ' + clean + '\n\n';
  });
  
  // Bold: **text** - handle <b>, <strong>, and styled spans
  text = text.replace(/<(b|strong)[^>]*>([\s\S]*?)<\/\1>/gi, (m, tag, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    if (!clean) return '';
    return '**' + clean + '**';
  });
  // Bold spans with style or class
  text = text.replace(/<span[^>]*(?:style=["'][^"']*font-weight:\s*bold[^"']*["']|class=["'][^"']*bold[^"']*["'])[^>]*>([\s\S]*?)<\/span>/gi, (m, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    if (!clean) return '';
    return '**' + clean + '**';
  });
  
  // Italic: *text* - handle <i>, <em>, <cite>, and styled spans
  text = text.replace(/<(i|em|cite)[^>]*>([\s\S]*?)<\/\1>/gi, (m, tag, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    if (!clean) return '';
    return '*' + clean + '*';
  });
  // Also handle mismatched tags like <i>...</em> (some EPUBs have this)
  text = text.replace(/<(i|em|cite)[^>]*>([\s\S]*?)<\/(i|em|cite)>/gi, (m, openTag, content, closeTag) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    if (!clean) return '';
    return '*' + clean + '*';
  });
  // Italic spans with style or class
  text = text.replace(/<span[^>]*(?:style=["'][^"']*font-style:\s*italic[^"']*["']|class=["'][^"']*italic[^"']*["'])[^>]*>([\s\S]*?)<\/span>/gi, (m, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    if (!clean) return '';
    return '*' + clean + '*';
  });
  
  // List items: • prefix
  text = text.replace(/<li[^>]*>([\s\S]*?)<\/li>/gi, (m, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    return '\n• ' + clean;
  });
  
  // Images: [Image] or [Image: alt text]
  text = text.replace(/<img[^>]*alt=["']([^"']+)["'][^>]*>/gi, '[Image: $1]');
  text = text.replace(/<img[^>]*>/gi, '[Image]');
  
  // Tables: placeholder
  text = text.replace(/<table[\s\S]*?<\/table>/gi, '\n[Table]\n');
  
  // === BLOCK ELEMENTS ===
  
  // Convert block elements to newlines
  text = text
    .replace(/<\/p>/gi, '\n\n')
    .replace(/<br\s*\/?>/gi, '\n')
    .replace(/<\/div>/gi, '\n')
    .replace(/<\/tr>/gi, '\n')
    .replace(/<hr[^>]*>/gi, '\n---\n');
  
  // Add paragraph breaks before block elements
  text = text
    .replace(/<p[^>]*>/gi, '\n\n')
    .replace(/<div[^>]*>/gi, '\n');
  
  // Remove list containers (ul, ol) but keep content
  text = text.replace(/<\/?[uo]l[^>]*>/gi, '\n');
  
  // CRITICAL: Add space after ALL closing inline tags to prevent word concatenation
  text = text
    .replace(/<\/span>/gi, ' ')
    .replace(/<\/a>/gi, ' ')
    .replace(/<\/u>/gi, ' ')
    .replace(/<\/small>/gi, ' ')
    .replace(/<\/sup>/gi, ' ')
    .replace(/<\/sub>/gi, ' ');
  
  // Remove all remaining HTML tags
  text = text.replace(/<[^>]+>/g, ' ');
  
  // Decode HTML entities (but preserve soft hyphens!)
  text = decodeEntities(text);
  text = text.replace(/&#(\d+);/g, (m, n) => String.fromCharCode(parseInt(n)));
  text = text.replace(/&#x([0-9a-f]+);/gi, (m, n) => String.fromCharCode(parseInt(n, 16)));
  // Restore soft hyphens that may have been entity-encoded
  text = text.replace(/&shy;/gi, '\u00AD');
  
  // Clean up whitespace (but preserve soft hyphens and markers)
  text = text
    .replace(/\r\n/g, '\n')
    .replace(/\r/g, '\n')
    .replace(/[ \t]+/g, ' ')           // Collapse multiple spaces to one
    .replace(/ +\n/g, '\n')            // Remove trailing spaces
    .replace(/\n +/g, '\n')            // Remove leading spaces  
    .replace(/\n{3,}/g, '\n\n')        // Max 2 consecutive newlines
    .trim();
  
  return text;
}

// =============================================================================
// COVER EXTRACTION
// =============================================================================
async function findCoverInEpub(zip, opfContent, opfDir) {
  if (!opfContent) return null;
  
  let coverHref = null;
  
  // Method 1: meta name="cover"
  const coverMeta = opfContent.match(/<meta[^>]+name=["']cover["'][^>]+content=["']([^"']+)["']/i) ||
                    opfContent.match(/<meta[^>]+content=["']([^"']+)["'][^>]+name=["']cover["']/i);
  
  if (coverMeta) {
    const coverId = coverMeta[1];
    const itemMatch = opfContent.match(new RegExp(`<item[^>]+id=["']${coverId}["'][^>]+href=["']([^"']+)["']`, 'i')) ||
                      opfContent.match(new RegExp(`<item[^>]+href=["']([^"']+)["'][^>]+id=["']${coverId}["']`, 'i'));
    if (itemMatch) coverHref = itemMatch[1];
  }
  
  // Method 2: properties="cover-image"
  if (!coverHref) {
    const propMatch = opfContent.match(/<item[^>]+properties=["'][^"']*cover-image[^"']*["'][^>]+href=["']([^"']+)["']/i) ||
                      opfContent.match(/<item[^>]+href=["']([^"']+)["'][^>]+properties=["'][^"']*cover-image[^"']*["']/i);
    if (propMatch) coverHref = propMatch[1];
  }
  
  // Method 3: Common filenames
  if (!coverHref) {
    for (const path of Object.keys(zip.files)) {
      const lower = path.toLowerCase();
      if (lower.includes('cover') && (lower.endsWith('.jpg') || lower.endsWith('.jpeg') || lower.endsWith('.png'))) {
        return path;
      }
    }
  }
  
  if (coverHref) {
    return resolveHref(decodeURIComponent(coverHref), opfDir);
  }
  
  return null;
}

// =============================================================================
// IMAGE PROCESSING
// =============================================================================
async function processImage(blob, maxWidth, maxHeight, quality) {
  return new Promise((resolve) => {
    const img = new Image();
    img.onload = () => {
      let width = img.width;
      let height = img.height;
      const ratio = width / height;
      
      if (width > maxWidth) { width = maxWidth; height = width / ratio; }
      if (height > maxHeight) { height = maxHeight; width = height * ratio; }
      
      width = Math.round(width);
      height = Math.round(height);
      
      const canvas = document.createElement('canvas');
      canvas.width = width;
      canvas.height = height;
      const ctx = canvas.getContext('2d');
      
      // CRITICAL: Fill with white first to handle transparent PNGs
      // Without this, transparent pixels become black in JPEG output
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(0, 0, width, height);
      
      ctx.drawImage(img, 0, 0, width, height);
      
      // Convert to grayscale with contrast enhancement for e-ink
      const imageData = ctx.getImageData(0, 0, width, height);
      const data = imageData.data;
      
      // First pass: find min/max for auto-levels
      let minGray = 255, maxGray = 0;
      for (let i = 0; i < data.length; i += 4) {
        const gray = Math.round(0.299 * data[i] + 0.587 * data[i+1] + 0.114 * data[i+2]);
        if (gray < minGray) minGray = gray;
        if (gray > maxGray) maxGray = gray;
      }
      
      // Second pass: apply grayscale + contrast stretch + slight boost
      const range = maxGray - minGray || 1;
      const contrast = 1.15;  // Slight contrast boost for e-ink
      
      for (let i = 0; i < data.length; i += 4) {
        let gray = Math.round(0.299 * data[i] + 0.587 * data[i+1] + 0.114 * data[i+2]);
        
        // Auto-levels: stretch to full range
        gray = ((gray - minGray) / range) * 255;
        
        // Apply contrast boost
        gray = ((gray / 255 - 0.5) * contrast + 0.5) * 255;
        
        // Clamp
        gray = Math.max(0, Math.min(255, Math.round(gray)));
        
        data[i] = data[i+1] = data[i+2] = gray;
      }
      ctx.putImageData(imageData, 0, 0);
      
      canvas.toBlob(resolve, 'image/jpeg', quality);
      URL.revokeObjectURL(img.src);
    };
    img.onerror = () => { URL.revokeObjectURL(img.src); resolve(null); };
    img.src = URL.createObjectURL(blob);
  });
}

// =============================================================================
// FILE UPLOAD HELPERS
// =============================================================================
function simpleHash(str) {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    hash = ((hash << 5) - hash) + str.charCodeAt(i);
    hash = hash >>> 0;
  }
  return hash.toString(16).padStart(8, '0');
}

async function ensureDirectory(path) {
  // Create directory structure on SD card
  try {
    // First ensure parent .sumi/books exists
    await fetch('/api/files/mkdir', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: '/.sumi' })
    });
    await fetch('/api/files/mkdir', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: '/.sumi/books' })
    });
    // Then the actual target
    const resp = await fetch('/api/files/mkdir', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path })
    });
    if (!resp.ok) {
      console.log('mkdir response:', resp.status, await resp.text());
    }
  } catch (e) {
    console.log('ensureDirectory error (continuing):', e);
    // Directory might already exist, continue anyway
  }
}

async function uploadTextFile(path, content, retries = 3) {
  const blob = new Blob([content], { type: 'text/plain' });
  const filename = path.split('/').pop();
  const dir = path.substring(0, path.lastIndexOf('/'));
  
  console.log(`[UPLOAD] Text file: dir='${dir}' filename='${filename}'`);
  
  for (let attempt = 1; attempt <= retries; attempt++) {
    try {
      const formData = new FormData();
      // IMPORTANT: path must come BEFORE file for multipart parsing
      formData.append('path', dir);
      formData.append('file', new File([blob], filename, { type: 'text/plain' }));
      
      const resp = await fetch('/api/files/upload', { method: 'POST', body: formData });
      if (!resp.ok) {
        throw new Error(`HTTP ${resp.status}`);
      }
      
      // Success - add delay to let ESP32 recover
      await new Promise(r => setTimeout(r, 100));
      return;
    } catch (e) {
      console.warn(`[UPLOAD] Text attempt ${attempt}/${retries} failed:`, e.message || e);
      if (attempt < retries) {
        // Wait longer before retry
        await new Promise(r => setTimeout(r, 500 * attempt));
      } else {
        console.error('[UPLOAD] All retries failed for:', path);
        throw e;
      }
    }
  }
}

async function uploadBlobFile(path, blob, retries = 3) {
  const filename = path.split('/').pop();
  const dir = path.substring(0, path.lastIndexOf('/'));
  
  console.log(`[UPLOAD] Blob file: dir='${dir}' filename='${filename}'`);
  
  for (let attempt = 1; attempt <= retries; attempt++) {
    try {
      const formData = new FormData();
      // IMPORTANT: path must come BEFORE file for multipart parsing
      formData.append('path', dir);
      formData.append('file', new File([blob], filename, { type: blob.type }));
      
      const resp = await fetch('/api/files/upload', { method: 'POST', body: formData });
      if (!resp.ok) {
        throw new Error(`HTTP ${resp.status}`);
      }
      
      // Success - add delay to let ESP32 recover
      await new Promise(r => setTimeout(r, 150));
      return;
    } catch (e) {
      console.warn(`[UPLOAD] Attempt ${attempt}/${retries} failed:`, e.message || e);
      if (attempt < retries) {
        // Wait longer before retry
        await new Promise(r => setTimeout(r, 500 * attempt));
      } else {
        console.error('[UPLOAD] All retries failed for:', path);
        throw e;
      }
    }
  }
}

// =============================================================================
// PROCESS EXISTING BOOKS ON DEVICE
// =============================================================================

// Check if we have internet access (can reach CDN)
async function checkInternetAccess() {
  try {
    // Try to fetch a tiny resource from CDN with short timeout
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 3000);
    
    const response = await fetch('https://cdnjs.cloudflare.com/ajax/libs/jszip/3.10.1/jszip.min.js', {
      method: 'HEAD',
      signal: controller.signal,
      mode: 'no-cors'  // Won't give us status but will succeed if reachable
    });
    
    clearTimeout(timeout);
    return true;
  } catch (e) {
    return false;
  }
}

async function checkBooksNeedProcessing() {
  // Don't overwrite UI while actively processing
  if (processingState.active) {
    return;
  }
  
  const warningEl = document.getElementById('coverWarning');
  if (!warningEl) return;
  
  // If on hotspot, hide processing UI entirely
  if (window.isOnHotspot) {
    warningEl.style.display = 'none';
    const tipEl = document.getElementById('processingTip');
    if (tipEl) tipEl.style.display = 'none';
    return;
  }
  
  try {
    // Add timeout to prevent hanging
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 5000);
    
    const response = await fetch('/api/books/status', { signal: controller.signal });
    clearTimeout(timeout);
    
    if (!response.ok) {
      console.log('[PORTAL] books/status returned', response.status);
      warningEl.style.display = 'none';
      const tipEl = document.getElementById('processingTip');
      if (tipEl) tipEl.style.display = 'none';
      return;
    }
    
    const data = await response.json();
    
    if (data.unprocessed && data.unprocessed.length > 0) {
      const count = data.unprocessed.length;
      
      // We have internet (checked above) - show processing UI
      warningEl.style.display = 'block';
      warningEl.innerHTML = `
        <div style="background: linear-gradient(135deg, #d4edda 0%, #c3e6cb 100%); border: 2px solid #28a745; border-radius: 12px; padding: 16px; margin-bottom: 16px;">
          <div style="display: flex; align-items: center; gap: 12px;">
            <div style="font-size: 32px;">✅</div>
            <div style="flex: 1;">
              <div style="font-weight: 700; font-size: 14px; color: #155724; margin-bottom: 4px;">
                Ready to Process ${count} Book${count > 1 ? 's' : ''}
              </div>
              <div style="font-size: 12px; color: #155724; line-height: 1.4;">
                Extract chapters and covers for instant book loading
              </div>
            </div>
            <button class="btn" style="background: #28a745; border: none; color: white; padding: 12px 24px; font-size: 14px; font-weight: 600; border-radius: 8px; cursor: pointer; box-shadow: 0 2px 8px rgba(40,167,69,0.3);" 
                    onclick="processExistingBooks()">
              🚀 Process Now
            </button>
          </div>
        </div>
      `;
      
      // Show the lightbulb tip
      const tipEl = document.getElementById('processingTip');
      if (tipEl) {
        tipEl.style.display = 'block';
        tipEl.innerHTML = `
          <div style="background: #fff9e6; border: 1px solid #ffc107; border-radius: 8px; padding: 12px 14px; margin-bottom: 16px; display: flex; align-items: flex-start; gap: 10px;">
            <div style="font-size: 18px;">💡</div>
            <div style="flex: 1; font-size: 12px; color: #856404; line-height: 1.5;">
              <strong>Tip:</strong> Processing runs in the background! Hit "Process Now" and continue exploring settings while your books are prepared.
            </div>
          </div>
        `;
      }
    } else {
      warningEl.style.display = 'none';
      const tipEl = document.getElementById('processingTip');
      if (tipEl) tipEl.style.display = 'none';
    }
  } catch (e) {
    // API might not exist yet, or device disconnected - hide warning
    console.log('[PORTAL] checkBooksNeedProcessing error:', e.message);
    warningEl.style.display = 'none';
    const tipEl = document.getElementById('processingTip');
    if (tipEl) tipEl.style.display = 'none';
  }
}

async function processExistingBooks() {
  const warningEl = document.getElementById('coverWarning');
  
  try {
    // Check connection first
    let connectionOk = false;
    for (let attempt = 0; attempt < 3; attempt++) {
      try {
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 5000);
        const testResp = await fetch('/api/status', { signal: controller.signal });
        clearTimeout(timeout);
        if (testResp.ok) {
          connectionOk = true;
          break;
        }
      } catch (e) {
        console.log(`[PORTAL] Connection check attempt ${attempt + 1} failed`);
        if (attempt < 2) {
          toast(`Reconnecting... (${attempt + 1}/3)`, 'warning');
          await new Promise(r => setTimeout(r, 2000));
        }
      }
    }
    
    if (!connectionOk) {
      toast('Cannot connect to device. Check WiFi connection.', 'error');
      return;
    }
    
    // Get list of unprocessed books
    const response = await fetch('/api/books/status');
    const data = await response.json();
    
    if (!data.unprocessed || data.unprocessed.length === 0) {
      toast('All books are already processed!', 'success');
      return;
    }
    
    const hasJSZip = await loadJSZip();
    if (!hasJSZip) {
      toast('Connect to WiFi with internet access first', 'error');
      return;
    }
    
    // Sort books by file size (smallest first) to process easier ones first
    // This helps avoid overwhelming the ESP32 with large image-heavy books early
    let books = data.unprocessed;
    books.sort((a, b) => (a.size || 0) - (b.size || 0));
    console.log('[PORTAL] Processing order (smallest first):', books.map(b => `${b.name} (${b.size || '?'} bytes)`));
    
    // Set up global processing state
    processingState.active = true;
    processingState.total = books.length;
    processingState.current = 0;
    
    // Create floating indicator (persists across page switches)
    createFloatingIndicator(books.length);
    
    // Hide the lightbulb tip since we're processing
    const tipEl = document.getElementById('processingTip');
    if (tipEl) tipEl.style.display = 'none';
    
    // Create visual processing UI in the books page
    warningEl.innerHTML = `
      <div style="background: white; border: 2px solid #007bff; border-radius: 12px; padding: 20px; margin-bottom: 16px;">
        <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px; padding-bottom: 12px; border-bottom: 1px solid #e9ecef;">
          <div style="font-weight: 700; font-size: 16px; color: #007bff;">📚 Processing ${books.length} Book${books.length > 1 ? 's' : ''}</div>
          <div id="processOverallProgress" style="font-size: 14px; font-weight: 600; color: #666;">0 / ${books.length}</div>
        </div>
        <div id="processBookGrid" style="display: grid; grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); gap: 16px; max-height: 400px; overflow-y: auto; padding: 4px;"></div>
      </div>
    `;
    
    const bookGrid = document.getElementById('processBookGrid');
    const overallProgress = document.getElementById('processOverallProgress');
    
    // Create a card for each book
    books.forEach((book, idx) => {
      const card = document.createElement('div');
      card.id = `process-book-${idx}`;
      card.style.cssText = 'background: #f8f9fa; border: 2px solid #dee2e6; border-radius: 10px; padding: 12px; text-align: center; position: relative; transition: all 0.3s;';
      card.innerHTML = `
        <div class="book-cover" style="width: 70px; height: 105px; margin: 0 auto 10px; background: #e9ecef; border-radius: 6px; display: flex; align-items: center; justify-content: center; font-size: 32px; position: relative; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,0.1);">
          📕
        </div>
        <div class="book-title" style="font-size: 11px; font-weight: 600; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; margin-bottom: 6px; color: #333;" title="${book.name}">
          ${book.name.replace(/\.epub$/i, '').substring(0, 20)}${book.name.length > 25 ? '...' : ''}
        </div>
        <div class="book-status" style="font-size: 10px; color: #999; font-weight: 500; height: 14px;">Waiting</div>
        <div class="book-progress" style="height: 4px; background: #dee2e6; border-radius: 2px; margin-top: 8px; overflow: hidden;">
          <div class="book-progress-bar" style="height: 100%; width: 0%; background: #007bff; transition: width 0.2s;"></div>
        </div>
      `;
      bookGrid.appendChild(card);
    });
    
    // Process each book
    let completed = 0;
    for (let i = 0; i < books.length; i++) {
      const book = books[i];
      const card = document.getElementById(`process-book-${i}`);
      const coverEl = card?.querySelector('.book-cover');
      const statusEl = card?.querySelector('.book-status');
      const progressBar = card?.querySelector('.book-progress-bar');
      const titleEl = card?.querySelector('.book-title');
      
      // Update global state
      processingState.current = i + 1;
      processingState.currentBook = book.name.replace(/\.epub$/i, '');
      updateFloatingIndicator();
      
      // Highlight current book
      if (card) {
        card.style.borderColor = '#007bff';
        card.style.background = '#fff';
        card.style.transform = 'scale(1.02)';
        card.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      }
      
      let stuckTimeout = null;
      
      try {
        // Download with retry
        if (statusEl) statusEl.textContent = 'Downloading...';
        if (statusEl) statusEl.style.color = '#007bff';
        if (progressBar) progressBar.style.width = '5%';
        
        const downloadUrl = `/api/download?path=/books/${encodeURIComponent(book.name)}`;
        console.log('[PORTAL] Downloading:', downloadUrl);
        
        let epubResponse;
        let retries = 3;
        while (retries > 0) {
          try {
            const controller = new AbortController();
            const timeout = setTimeout(() => controller.abort(), 30000); // 30s timeout
            epubResponse = await fetch(downloadUrl, { signal: controller.signal });
            clearTimeout(timeout);
            if (epubResponse.ok) break;
            throw new Error(`HTTP ${epubResponse.status}`);
          } catch (fetchErr) {
            retries--;
            console.log(`[PORTAL] Download attempt failed, ${retries} retries left:`, fetchErr.message);
            if (retries === 0) {
              throw new Error(`Download failed: ${fetchErr.message}`);
            }
            if (statusEl) statusEl.textContent = `Retry (${3-retries}/3)...`;
            await new Promise(r => setTimeout(r, 1000)); // Wait 1s before retry
          }
        }
        
        if (progressBar) progressBar.style.width = '10%';
        if (statusEl) statusEl.textContent = 'Reading file...';
        
        console.log('[PORTAL] Response status:', epubResponse.status);
        
        if (!epubResponse.ok) throw new Error(`HTTP ${epubResponse.status}`);
        
        const epubBlob = await epubResponse.blob();
        const file = new File([epubBlob], book.name, { type: 'application/epub+zip' });
        
        if (progressBar) progressBar.style.width = '15%';
        
        // Process with visual progress - add timeout for stuck detection
        let lastProgressTime = Date.now();
        
        const checkStuck = () => {
          const timeSinceProgress = Date.now() - lastProgressTime;
          if (timeSinceProgress > 30000 && statusEl) { // 30 seconds without progress
            statusEl.innerHTML = `<span style="color: #dc3545; cursor: pointer;" onclick="location.reload()">⚠️ Stuck - click to retry</span>`;
          }
        };
        stuckTimeout = setInterval(checkStuck, 5000);
        
        // Process with visual progress - capture returned metadata
        const meta = await processExistingEpub(file, (step, progress) => {
          lastProgressTime = Date.now(); // Reset stuck timer
          processingState.currentStep = step;
          updateFloatingIndicator(progress); // Pass current book progress
          if (statusEl) statusEl.textContent = step;
          if (progress !== undefined && progressBar) {
            progressBar.style.width = (15 + progress * 85) + '%';
          }
        }, (url) => {
          // Cover callback - show cover immediately when extracted
          if (url && coverEl) {
            coverEl.style.background = 'white';
            coverEl.innerHTML = `<img src="${url}" style="width: 100%; height: 100%; object-fit: cover;">`;
          }
        });
        
        clearInterval(stuckTimeout);
        
        // Success! Show metadata
        if (progressBar) {
          progressBar.style.width = '100%';
          progressBar.style.background = '#28a745';
        }
        if (card) {
          card.style.borderColor = '#28a745';
          card.style.transform = 'scale(1)';
        }
        
        // Update title with actual title from metadata
        if (meta && meta.title && titleEl) {
          titleEl.textContent = meta.title.substring(0, 25) + (meta.title.length > 25 ? '...' : '');
          titleEl.title = meta.title + (meta.author ? ` by ${meta.author}` : '');
        }
        
        // Build rich metadata display
        const metaStats = [];
        if (meta) {
          // Line 1: Basic stats
          metaStats.push(`${meta.totalChapters} ch`);
          metaStats.push(`~${meta.estimatedPages} pg`);
          if (meta.totalImages > 0) {
            metaStats.push(`${meta.totalImages} img`);
          }
          if (meta.estimatedReadingMins) {
            const hrs = Math.floor(meta.estimatedReadingMins / 60);
            const mins = meta.estimatedReadingMins % 60;
            metaStats.push(hrs > 0 ? `${hrs}h ${mins}m` : `${mins}m`);
          }
        }
        
        // Build tooltip with full book info
        let tooltip = '';
        if (meta) {
          tooltip += `${meta.title}\\n`;
          if (meta.author && meta.author !== 'Unknown') tooltip += `by ${meta.author}\\n`;
          tooltip += `\\n${meta.totalChapters} chapters • ~${meta.estimatedPages} pages`;
          if (meta.totalImages > 0) tooltip += ` • ${meta.totalImages} images`;
          tooltip += `\\n${meta.totalWords?.toLocaleString() || '?'} words • ~${Math.floor((meta.estimatedReadingMins || 0) / 60)}h ${(meta.estimatedReadingMins || 0) % 60}m read\\n`;
          if (meta.publisher) tooltip += `\\nPublisher: ${meta.publisher}`;
          if (meta.pubYear) tooltip += `\\nPublished: ${meta.pubYear}`;
          if (meta.language) tooltip += `\\nLanguage: ${meta.language.toUpperCase()}`;
          if (meta.series) tooltip += `\\nSeries: ${meta.series}${meta.seriesPosition ? ` #${meta.seriesPosition}` : ''}`;
          if (meta.subjects && meta.subjects.length > 0) tooltip += `\\nGenre: ${meta.subjects.slice(0, 3).join(', ')}`;
          if (meta.description) tooltip += `\\n\\n${meta.description.substring(0, 200)}${meta.description.length > 200 ? '...' : ''}`;
        }
        
        if (statusEl) {
          statusEl.innerHTML = `<span style="color: #28a745; font-size: 9px; cursor: help;" title="${tooltip.replace(/"/g, '&quot;')}">${metaStats.join(' • ')}</span>`;
        }
        
        // Store metadata on card for later access
        if (card) {
          card.dataset.bookMeta = JSON.stringify(meta);
          card.style.cursor = 'pointer';
          card.onclick = (e) => {
            if (e.target.closest('button')) return; // Don't trigger on delete button
            showBookInfoModal(meta);
          };
        }
        
        // Add checkmark badge over cover
        if (coverEl) {
          const badge = document.createElement('div');
          badge.style.cssText = 'position: absolute; top: 8px; right: 8px; background: #28a745; color: white; border-radius: 50%; width: 24px; height: 24px; display: flex; align-items: center; justify-content: center; font-size: 14px; font-weight: bold; box-shadow: 0 2px 4px rgba(0,0,0,0.2);';
          badge.textContent = '✓';
          coverEl.appendChild(badge);
        }
        
        completed++;
        
        // Give ESP32 time to recover before processing next book
        if (i < books.length - 1) {
          if (statusEl) statusEl.innerHTML = `<span style="color: #28a745;">✓ Done - waiting...</span>`;
          await new Promise(r => setTimeout(r, 2000)); // 2 second delay between books
        }
        
      } catch (e) {
        if (stuckTimeout) clearInterval(stuckTimeout);
        console.error('Failed to process:', book.name, e);
        if (progressBar) {
          progressBar.style.background = '#dc3545';
          progressBar.style.width = '100%';
        }
        // Show actual error message
        const errorMsg = e.message || 'Unknown error';
        if (statusEl) statusEl.innerHTML = `<span style="color: #dc3545; font-size: 9px;">✗ ${errorMsg.substring(0, 30)}</span>`;
        if (card) {
          card.style.borderColor = '#dc3545';
          card.style.transform = 'scale(1)';
        }
        
        // Also show toast with full error
        toast(`${book.name}: ${errorMsg}`, 'error');
      }
      
      // Update overall progress
      if (overallProgress) overallProgress.textContent = `${i + 1} / ${books.length}`;
    }
    
    // All done!
    processingState.active = false;
    removeFloatingIndicator();
    
    if (overallProgress) overallProgress.innerHTML = `<span style="color: #28a745;">✅ ${completed}/${books.length} Complete</span>`;
    
    toast(`Processed ${completed}/${books.length} books!`, completed === books.length ? 'success' : 'warning');
    
    // Refresh book grid after short delay to show covers in "Your Books"
    setTimeout(() => {
      refreshFiles('books');
      checkBooksNeedProcessing();
    }, 1500);
    
  } catch (e) {
    console.error('Process existing books failed:', e);
    toast('Processing failed: ' + e.message, 'error');
    processingState.active = false;
    removeFloatingIndicator();
  }
}

function createFloatingIndicator(total) {
  // Remove any existing indicator
  removeFloatingIndicator();
  
  const indicator = document.createElement('div');
  indicator.id = 'processingIndicator';
  indicator.style.cssText = `
    position: fixed;
    bottom: 20px;
    right: 20px;
    background: linear-gradient(135deg, #007bff, #0056b3);
    color: white;
    padding: 14px 18px;
    border-radius: 14px;
    box-shadow: 0 4px 24px rgba(0,0,0,0.35);
    z-index: 10000;
    font-size: 13px;
    min-width: 240px;
    cursor: pointer;
    transition: transform 0.2s;
  `;
  indicator.onmouseenter = () => indicator.style.transform = 'scale(1.02)';
  indicator.onmouseleave = () => indicator.style.transform = 'scale(1)';
  indicator.onclick = () => { showPage('files'); setTimeout(() => showFileTab('books'), 100); };
  indicator.innerHTML = `
    <div style="display: flex; align-items: center; gap: 12px; margin-bottom: 10px;">
      <div style="font-size: 22px;" class="pulse">📚</div>
      <div style="flex: 1;">
        <div style="font-weight: 700; font-size: 14px;">Processing Books</div>
        <div id="floatingBookName" style="font-size: 11px; opacity: 0.85; margin-top: 2px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 180px;">Starting...</div>
      </div>
      <div id="floatingCount" style="font-size: 12px; font-weight: 600; background: rgba(255,255,255,0.2); padding: 4px 8px; border-radius: 6px;">0/${total}</div>
    </div>
    <div id="floatingStepText" style="font-size: 10px; opacity: 0.8; margin-bottom: 6px; height: 14px;">Initializing...</div>
    <div style="background: rgba(255,255,255,0.25); height: 6px; border-radius: 3px; overflow: hidden; position: relative;">
      <div id="floatingProgress" style="height: 100%; width: 0%; background: rgba(255,255,255,0.9); transition: width 0.15s; border-radius: 3px;"></div>
      <div id="floatingProgressPulse" style="position: absolute; top: 0; left: 0; height: 100%; width: 30px; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.4), transparent); animation: shimmer 1.5s infinite;"></div>
    </div>
  `;
  
  // Add animations
  const style = document.createElement('style');
  style.id = 'processingAnimStyle';
  style.textContent = `
    .pulse { animation: pulse 1.5s ease-in-out infinite; }
    @keyframes pulse { 0%, 100% { transform: scale(1); } 50% { transform: scale(1.1); } }
    @keyframes shimmer { 0% { left: -30px; } 100% { left: 100%; } }
  `;
  document.head.appendChild(style);
  
  document.body.appendChild(indicator);
}

function updateFloatingIndicator(bookProgress) {
  const bookNameEl = document.getElementById('floatingBookName');
  const countEl = document.getElementById('floatingCount');
  const stepEl = document.getElementById('floatingStepText');
  const progressEl = document.getElementById('floatingProgress');
  
  if (!processingState.active) return;
  
  if (bookNameEl) {
    bookNameEl.textContent = processingState.currentBook || 'Processing...';
    bookNameEl.title = processingState.currentBook;
  }
  
  if (countEl) {
    countEl.textContent = `${processingState.current}/${processingState.total}`;
  }
  
  if (stepEl) {
    stepEl.textContent = processingState.currentStep || 'Working...';
  }
  
  if (progressEl && processingState.total > 0) {
    // Calculate overall progress including current book's progress
    const baseProgress = ((processingState.current - 1) / processingState.total) * 100;
    const bookContribution = (1 / processingState.total) * 100;
    const currentBookProgress = (bookProgress || 0) * bookContribution;
    const totalProgress = baseProgress + currentBookProgress;
    progressEl.style.width = totalProgress + '%';
  }
}

function removeFloatingIndicator() {
  const indicator = document.getElementById('processingIndicator');
  if (indicator) indicator.remove();
  
  const style = document.getElementById('processingAnimStyle');
  if (style) style.remove();
}

// Process an existing EPUB (skip re-uploading the EPUB itself)
async function processExistingEpub(file, onProgress, onCoverReady) {
  const hash = simpleHash(file.name);
  const cacheDir = `/.sumi/books/${hash}`;
  
  onProgress('Opening...', 0);
  await new Promise(r => setTimeout(r, 0)); // Let UI update
  
  const arrayBuffer = await file.arrayBuffer();
  
  onProgress('Unzipping...', 0.03);
  await new Promise(r => setTimeout(r, 0));
  const zip = await JSZip.loadAsync(arrayBuffer);
  
  onProgress('Reading metadata...', 0.06);
  await new Promise(r => setTimeout(r, 0));
  const { opfPath, opfContent, opfDir } = await findOPF(zip);
  if (!opfContent) throw new Error('Invalid EPUB');
  
  const metadata = parseMetadata(opfContent);
  const manifest = parseManifest(opfContent);
  const spine = parseSpine(opfContent);
  
  onProgress('Finding cover...', 0.09);
  await new Promise(r => setTimeout(r, 0));
  const coverPath = await findCoverInEpub(zip, opfContent, opfDir);
  let thumbBlob = null, fullBlob = null;
  
  if (coverPath) {
    onProgress('Processing cover...', 0.12);
    await new Promise(r => setTimeout(r, 0));
    const coverFile = zip.file(coverPath);
    if (coverFile) {
      const coverData = await coverFile.async('blob');
      thumbBlob = await processImage(coverData, 120, 180, 0.75);
      fullBlob = await processImage(coverData, 300, 450, 0.8);
      
      // Immediately show cover preview if callback provided
      if (onCoverReady && thumbBlob) {
        const coverUrl = URL.createObjectURL(thumbBlob);
        onCoverReady(coverUrl);
      }
    }
  }
  
  // Ensure images directory exists for inline images
  await ensureDirectory(cacheDir + '/images');
  
  // Extract chapters with detailed progress
  const chapters = [];
  let totalChars = 0;
  let totalWords = 0;
  let imageIndex = { count: 0 };  // Shared counter for all images in book
  
  console.log(`[PORTAL] Processing ${spine.length} chapters for ${file.name}`);
  
  for (let i = 0; i < spine.length; i++) {
    const idref = spine[i];
    const item = manifest[idref];
    if (!item) continue;
    
    const progress = 0.15 + (i / spine.length) * 0.55;
    onProgress(`Extract ${i+1}/${spine.length}`, progress);
    await new Promise(r => setTimeout(r, 0)); // Let UI update every chapter
    
    const chapterPath = resolveHref(item.href, opfDir);
    const chapterFile = zip.file(chapterPath);
    
    // Get directory of chapter file for resolving relative image paths
    const chapterDir = chapterPath.includes('/') 
      ? chapterPath.substring(0, chapterPath.lastIndexOf('/') + 1)
      : '';
    
    if (chapterFile) {
      try {
        let html = await chapterFile.async('string');
        
        console.log(`[PORTAL] Chapter ${i}: ${chapterPath}, chapterDir: ${chapterDir}`);
        
        // Extract inline images BEFORE converting to plain text
        const images = await extractInlineImages(zip, html, chapterDir, cacheDir, imageIndex);
        console.log(`[PORTAL] Chapter ${i}: found ${images.length} images`);
        
        // Replace <img> tags with markers
        for (const img of images) {
          html = html.replace(img.original, img.marker);
        }
        
        // Convert HTML to plain text with rich markers
        let text = htmlToPlainText(html);
        
        // PROTECT image markers from text processing
        // Extract [!IMG:...] markers before typography/hyphenation mangles them
        const imgMarkers = [];
        text = text.replace(/\[!IMG:[^\]]+\]/g, (match) => {
          imgMarkers.push(match);
          return `\x00IMGMARKER${imgMarkers.length - 1}\x00`;
        });
        
        // Apply smart typography
        text = applySmartTypography(text);
        
        // Add soft hyphens for better line breaking
        text = addSoftHyphens(text, metadata.language || 'en');
        
        // RESTORE image markers (unchanged)
        text = text.replace(/\x00IMGMARKER(\d+)\x00/g, (m, idx) => {
          return imgMarkers[parseInt(idx)];
        });
        
        if (text.trim().length > 0) {
          const charCount = text.length;
          const wordCount = text.split(/\s+/).filter(w => w.length > 0).length;
          totalChars += charCount;
          totalWords += wordCount;
          
          chapters.push({
            index: chapters.length,
            idref: idref,
            text: text,
            chars: charCount,
            words: wordCount
          });
        }
      } catch (e) {
        console.warn(`[PORTAL] Error processing chapter ${i}:`, e);
      }
    }
  }
  
  console.log(`[PORTAL] Total images extracted: ${imageIndex.count}`);
  
  // Estimate pages: ~1800 chars per e-ink page (conservative estimate for 800x480)
  const CHARS_PER_PAGE = 1800;
  const estimatedPages = Math.ceil(totalChars / CHARS_PER_PAGE);
  
  const meta = {
    version: 3,
    hash: hash,
    filename: file.name,
    fileSize: file.size,
    title: metadata.title || file.name.replace('.epub', ''),
    author: metadata.author || 'Unknown',
    language: metadata.language || 'en',
    totalChapters: chapters.length,
    totalChars: totalChars,
    totalWords: totalWords,
    totalImages: imageIndex.count,
    estimatedPages: estimatedPages,
    estimatedReadingMins: Math.ceil(totalWords / 250), // ~250 wpm average
    processedAt: Date.now(),
    chapters: chapters.map((c, i) => ({
      index: i,
      file: `ch_${String(i).padStart(3, '0')}.txt`,
      chars: c.chars,
      words: c.words
    }))
  };
  
  onProgress('Creating dirs...', 0.72);
  await new Promise(r => setTimeout(r, 0));
  await ensureDirectory(cacheDir);
  
  onProgress('Saving meta...', 0.74);
  await new Promise(r => setTimeout(r, 0));
  await uploadTextFile(cacheDir + '/meta.json', JSON.stringify(meta, null, 2));
  
  if (thumbBlob) {
    onProgress('Saving thumb...', 0.76);
    await uploadBlobFile(cacheDir + '/cover_thumb.jpg', thumbBlob);
  }
  if (fullBlob) {
    onProgress('Saving cover...', 0.78);
    await uploadBlobFile(cacheDir + '/cover_full.jpg', fullBlob);
  }
  
  // Upload chapters with detailed progress
  for (let i = 0; i < chapters.length; i++) {
    const progress = 0.8 + (i / chapters.length) * 0.19;
    onProgress(`Upload ${i+1}/${chapters.length}`, progress);
    
    const filename = `ch_${String(i).padStart(3, '0')}.txt`;
    await uploadTextFile(cacheDir + '/' + filename, chapters[i].text);
    
    // Give ESP32 time to process and close files
    await new Promise(r => setTimeout(r, 50));
  }
  
  onProgress('Done!', 1);
  
  // Return the metadata for display
  return meta;
}

function handleDrop(e, type) {
  e.preventDefault();
  e.currentTarget.classList.remove('dragover');
  uploadFiles(e.dataTransfer.files, type);
}

// =============================================================================
// SETTINGS
// =============================================================================
// Settings keys that need to be nested under specific objects
const NESTED_SETTINGS = {
  display: ['bootToLastBook', 'invertColors', 'deepSleep', 'showBatteryHome', 
            'showBatterySleep', 'showClockHome', 'showDate', 'showWifi', 'landscape',
            'showBookWidget', 'showWeatherWidget', 'showOrientWidget'],
  bluetooth: ['btEnabled', 'btAutoConnect'],
  sync: ['kosyncEnabled']
};

function toggleOpt(el, key) {
  el.classList.toggle('on');
  const value = el.classList.contains('on');
  
  // Find which object this setting belongs to
  for (const [obj, keys] of Object.entries(NESTED_SETTINGS)) {
    if (keys.includes(key)) {
      // Map portal key names to actual setting names if different
      let settingKey = key;
      if (key === 'btEnabled') settingKey = 'enabled';
      if (key === 'btAutoConnect') settingKey = 'autoConnect';
      if (key === 'kosyncEnabled') settingKey = 'kosyncEnabled';
      
      api('/api/settings', 'POST', {[obj]: {[settingKey]: value}});
      return;
    }
  }
  
  // Not in any nested object, send as top-level
  api('/api/settings', 'POST', {[key]: value});
}

function updateSlider(el, labelId, suffix) {
  document.getElementById(labelId).textContent = el.value + suffix;
}

// =============================================================================
// READER SETTINGS
// =============================================================================
function saveReaderSetting(key, value) {
  api('/api/reader/settings', 'POST', {[key]: parseInt(value)});
}

// Simple toggle for switches that don't need immediate API call
function toggleSwitchSimple(el) {
  el.classList.toggle('on');
}

// Flashcard preview and settings
function updateFlashcardPreview() {
  const fontSize = parseInt(document.getElementById('fcFontSize')?.value || 1);
  const centerText = document.getElementById('togFcCenter')?.classList.contains('on') ?? true;
  const showProgress = document.getElementById('togFcProgress')?.classList.contains('on') ?? true;
  const showStats = document.getElementById('togFcStats')?.classList.contains('on') ?? true;
  
  // Font sizes: 0=10px, 1=14px, 2=18px, 3=22px (scaled for preview)
  const fontSizes = [10, 14, 18, 22];
  const previewFont = fontSizes[fontSize] || 14;
  
  const qText = document.getElementById('fcPreviewQText');
  const aText = document.getElementById('fcPreviewAText');
  const progress = document.getElementById('fcPreviewProgress');
  const stats = document.getElementById('fcPreviewStats');
  const qArea = document.getElementById('fcPreviewQuestion');
  const aArea = document.getElementById('fcPreviewAnswer');
  
  if (qText) {
    qText.style.fontSize = previewFont + 'px';
    qText.style.textAlign = centerText ? 'center' : 'left';
  }
  if (aText) {
    aText.style.fontSize = previewFont + 'px';
    aText.style.textAlign = centerText ? 'center' : 'left';
  }
  if (qArea) {
    qArea.style.textAlign = centerText ? 'center' : 'left';
  }
  if (aArea) {
    aArea.style.textAlign = centerText ? 'center' : 'left';
  }
  if (progress) {
    progress.style.display = showProgress ? 'block' : 'none';
  }
  if (stats) {
    stats.style.visibility = showStats ? 'visible' : 'hidden';
  }
}

async function saveFlashcardSettings() {
  const fontSize = parseInt(document.getElementById('fcFontSize')?.value || 1);
  const centerText = document.getElementById('togFcCenter')?.classList.contains('on') ?? true;
  const shuffle = document.getElementById('togFcShuffle')?.classList.contains('on') ?? true;
  const showProgressBar = document.getElementById('togFcProgress')?.classList.contains('on') ?? true;
  const showStats = document.getElementById('togFcStats')?.classList.contains('on') ?? true;
  const autoFlip = document.getElementById('togFcAutoFlip')?.classList.contains('on') ?? false;
  
  try {
    await fetch('/api/flashcards/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        fontSize,
        centerText,
        shuffle,
        showProgressBar,
        showStats,
        autoFlip
      })
    });
  } catch (e) {
    console.error('Failed to save flashcard settings:', e);
  }
}

async function loadFlashcardSettings() {
  try {
    const response = await fetch('/api/flashcards/settings');
    const data = await response.json();
    
    if (document.getElementById('fcFontSize')) {
      document.getElementById('fcFontSize').value = data.fontSize ?? 1;
    }
    if (document.getElementById('togFcCenter')) {
      document.getElementById('togFcCenter').classList.toggle('on', data.centerText ?? true);
    }
    if (document.getElementById('togFcShuffle')) {
      document.getElementById('togFcShuffle').classList.toggle('on', data.shuffle ?? true);
    }
    if (document.getElementById('togFcProgress')) {
      document.getElementById('togFcProgress').classList.toggle('on', data.showProgressBar ?? true);
    }
    if (document.getElementById('togFcStats')) {
      document.getElementById('togFcStats').classList.toggle('on', data.showStats ?? true);
    }
    if (document.getElementById('togFcAutoFlip')) {
      document.getElementById('togFcAutoFlip').classList.toggle('on', data.autoFlip ?? false);
    }
    
    updateFlashcardPreview();
  } catch (e) {
    console.error('Failed to load flashcard settings:', e);
  }
}

// =============================================================================
// IMAGE SETTINGS
// =============================================================================
function toggleImageOpt(el, key) {
  el.classList.toggle('on');
  api('/api/images/settings', 'POST', {[key]: el.classList.contains('on')});
}

// =============================================================================
// FLASHCARDS
// =============================================================================
function saveFcSetting(key, value) {
  api('/api/flashcards/settings', 'POST', {[key]: parseInt(value)});
}

function toggleFcOpt(el, key) {
  el.classList.toggle('on');
  api('/api/flashcards/settings', 'POST', {[key]: el.classList.contains('on')});
}

function showCreateDeckModal() {
  const name = prompt('Enter deck name:');
  if (name) {
    api('/api/flashcards/create', 'POST', {name});
    toast('Deck created: ' + name, 'success');
    refreshFiles('flashcards');
  }
}

// Book info modal - shows full metadata for a processed book
function showBookInfoModal(meta) {
  if (!meta) return;
  
  // Remove existing modal if any
  const existing = document.getElementById('bookInfoModal');
  if (existing) existing.remove();
  
  // Format reading time
  const hrs = Math.floor((meta.estimatedReadingMins || 0) / 60);
  const mins = (meta.estimatedReadingMins || 0) % 60;
  const readTime = hrs > 0 ? `${hrs}h ${mins}m` : `${mins} min`;
  
  // Format file size
  const sizeKB = Math.round((meta.fileSize || 0) / 1024);
  const sizeMB = (sizeKB / 1024).toFixed(1);
  const sizeStr = sizeKB > 1024 ? `${sizeMB} MB` : `${sizeKB} KB`;
  
  // Build modal HTML
  const modal = document.createElement('div');
  modal.id = 'bookInfoModal';
  modal.style.cssText = `
    position: fixed; top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,0.6); z-index: 10000;
    display: flex; align-items: center; justify-content: center;
    padding: 20px; backdrop-filter: blur(4px);
  `;
  modal.onclick = (e) => { if (e.target === modal) modal.remove(); };
  
  modal.innerHTML = `
    <div style="background: white; border-radius: 12px; max-width: 500px; width: 100%; max-height: 80vh; overflow-y: auto; box-shadow: 0 20px 60px rgba(0,0,0,0.3);">
      <div style="padding: 24px; border-bottom: 1px solid #eee;">
        <div style="display: flex; justify-content: space-between; align-items: start;">
          <div>
            <h2 style="margin: 0 0 8px 0; font-size: 20px; color: #333;">${meta.title || 'Unknown Title'}</h2>
            ${meta.author && meta.author !== 'Unknown' ? `<p style="margin: 0; color: #666; font-size: 14px;">by ${meta.author}</p>` : ''}
          </div>
          <button onclick="this.closest('#bookInfoModal').remove()" style="background: none; border: none; font-size: 24px; cursor: pointer; color: #999; padding: 0; line-height: 1;">&times;</button>
        </div>
        ${meta.series ? `<p style="margin: 8px 0 0 0; color: #888; font-size: 13px;">📚 ${meta.series}${meta.seriesPosition ? ` #${meta.seriesPosition}` : ''}</p>` : ''}
      </div>
      
      <div style="padding: 20px;">
        <!-- Stats Grid -->
        <div style="display: grid; grid-template-columns: repeat(${meta.totalImages > 0 ? 4 : 3}, 1fr); gap: 12px; margin-bottom: 20px;">
          <div style="text-align: center; padding: 12px; background: #f8f9fa; border-radius: 8px;">
            <div style="font-size: 24px; font-weight: 600; color: #333;">${meta.totalChapters || '?'}</div>
            <div style="font-size: 11px; color: #666; text-transform: uppercase;">Chapters</div>
          </div>
          <div style="text-align: center; padding: 12px; background: #f8f9fa; border-radius: 8px;">
            <div style="font-size: 24px; font-weight: 600; color: #333;">~${meta.estimatedPages || '?'}</div>
            <div style="font-size: 11px; color: #666; text-transform: uppercase;">Pages</div>
          </div>
          ${meta.totalImages > 0 ? `
          <div style="text-align: center; padding: 12px; background: #f8f9fa; border-radius: 8px;">
            <div style="font-size: 24px; font-weight: 600; color: #333;">${meta.totalImages}</div>
            <div style="font-size: 11px; color: #666; text-transform: uppercase;">Images</div>
          </div>
          ` : ''}
          <div style="text-align: center; padding: 12px; background: #f8f9fa; border-radius: 8px;">
            <div style="font-size: 24px; font-weight: 600; color: #333;">${readTime}</div>
            <div style="font-size: 11px; color: #666; text-transform: uppercase;">Read Time</div>
          </div>
        </div>
        
        <!-- Description -->
        ${meta.description ? `
          <div style="margin-bottom: 20px;">
            <h4 style="margin: 0 0 8px 0; font-size: 13px; color: #888; text-transform: uppercase;">Description</h4>
            <p style="margin: 0; font-size: 14px; color: #444; line-height: 1.5;">${meta.description}</p>
          </div>
        ` : ''}
        
        <!-- Details -->
        <div style="border-top: 1px solid #eee; padding-top: 16px;">
          <h4 style="margin: 0 0 12px 0; font-size: 13px; color: #888; text-transform: uppercase;">Details</h4>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 13px;">
            ${meta.publisher ? `<div><span style="color: #888;">Publisher:</span> ${meta.publisher}</div>` : ''}
            ${meta.pubYear ? `<div><span style="color: #888;">Published:</span> ${meta.pubYear}</div>` : ''}
            ${meta.language ? `<div><span style="color: #888;">Language:</span> ${meta.language.toUpperCase()}</div>` : ''}
            ${meta.isbn ? `<div><span style="color: #888;">ISBN:</span> ${meta.isbn}</div>` : ''}
            <div><span style="color: #888;">Words:</span> ${(meta.totalWords || 0).toLocaleString()}</div>
            <div><span style="color: #888;">Size:</span> ${sizeStr}</div>
            ${meta.epubVersion ? `<div><span style="color: #888;">EPUB:</span> v${meta.epubVersion}</div>` : ''}
          </div>
        </div>
        
        <!-- Subjects/Genres -->
        ${meta.subjects && meta.subjects.length > 0 ? `
          <div style="margin-top: 16px;">
            <h4 style="margin: 0 0 8px 0; font-size: 13px; color: #888; text-transform: uppercase;">Genres</h4>
            <div style="display: flex; flex-wrap: wrap; gap: 6px;">
              ${meta.subjects.map(s => `<span style="background: #e9ecef; padding: 4px 10px; border-radius: 12px; font-size: 12px; color: #495057;">${s}</span>`).join('')}
            </div>
          </div>
        ` : ''}
      </div>
    </div>
  `;
  
  document.body.appendChild(modal);
}

async function importAnkiDeck(file) {
  if (!file) return;
  const formData = new FormData();
  formData.append('file', file);
  try {
    await fetch('/api/flashcards/import', {method: 'POST', body: formData});
    toast('Imported: ' + file.name, 'success');
    refreshFiles('flashcards');
  } catch(e) {
    toast('Import failed', 'error');
  }
}

function createNewNote() {
  const name = prompt('Enter note name:');
  if (name) {
    api('/api/notes/create', 'POST', {name});
    toast('Note created: ' + name, 'success');
    refreshFiles('notes');
  }
}

// =============================================================================
// BLUETOOTH
// =============================================================================
async function toggleBT(el) {
  el.classList.toggle('on');
  const enabled = el.classList.contains('on');
  await api('/api/bluetooth/enable', 'POST', {enabled});
  if (enabled) loadBT();
}

async function loadBT() {
  const d = await api('/api/bluetooth/devices');
  if (d?.paired?.length) {
    document.getElementById('btPaired').innerHTML = d.paired.map(x => `
      <div class="wifi-item">
        <span class="wifi-icon">⌨️</span>
        <div class="wifi-info">
          <div class="wifi-name">${x.name || 'Keyboard'}</div>
          <div class="wifi-meta">${x.connected ? '🟢 Connected' : '⚪ Paired'}</div>
        </div>
        <button class="btn btn-sm ${x.connected ? 'btn-danger' : 'btn-secondary'}" onclick="${x.connected ? `btDisconnect('${x.address}')` : `btConnect('${x.address}')`}">${x.connected ? 'Disconnect' : 'Connect'}</button>
      </div>
    `).join('');
  } else {
    document.getElementById('btPaired').innerHTML = '<p style="color:var(--text-muted);text-align:center;padding:20px;">No devices paired yet</p>';
  }
}

async function scanBT() {
  const btn = document.getElementById('btScanBtn');
  btn.disabled = true;
  btn.textContent = '⏳ Scanning...';
  document.getElementById('btDiscovered').innerHTML = '<p style="text-align:center;padding:20px;color:var(--text-muted);">Scanning for keyboards...</p>';
  
  await api('/api/bluetooth/scan', 'POST');
  
  // Poll for results
  for (let i = 0; i < 10; i++) {
    await new Promise(r => setTimeout(r, 1000));
    const d = await api('/api/bluetooth/devices');
    if (d?.discovered?.length) {
      document.getElementById('btDiscovered').innerHTML = d.discovered.map(x => `
        <div class="wifi-item" onclick="btPair('${x.address}')">
          <span class="wifi-icon">🔵</span>
          <div class="wifi-info">
            <div class="wifi-name">${x.name || 'Unknown Device'}</div>
            <div class="wifi-meta">Signal: ${x.rssi} dBm</div>
          </div>
          <button class="btn btn-sm btn-primary">Pair</button>
        </div>
      `).join('');
      break;
    }
  }
  
  btn.disabled = false;
  btn.textContent = '🔍 Scan';
}

async function btPair(addr) {
  toast('Pairing...', 'info');
  await api('/api/bluetooth/pair', 'POST', {address: addr});
  loadBT();
  toast('Paired successfully!', 'success');
}

async function btConnect(addr) {
  toast('Connecting...', 'info');
  await api('/api/bluetooth/connect', 'POST', {address: addr});
  loadBT();
}

async function btDisconnect(addr) {
  toast('Disconnecting...', 'info');
  await api('/api/bluetooth/disconnect', 'POST', {address: addr});
  loadBT();
  toast('Disconnected', 'success');
}

// =============================================================================
// SYSTEM
// =============================================================================
function refreshDisplay() {
  api('/api/refresh', 'POST');
  toast('Display refreshed', 'success');
}

function rebootDevice() {
  if (confirm('Reboot device?')) {
    api('/api/reboot', 'POST');
    toast('Rebooting...', 'info');
  }
}

function factoryReset() {
  if (confirm('⚠️ This will erase ALL settings. Are you sure?')) {
    if (confirm('This cannot be undone. Proceed?')) {
      api('/api/factory-reset', 'POST');
      toast('Factory reset initiated...', 'info');
    }
  }
}

// =============================================================================
// INIT
// =============================================================================

// Settings restoration helpers
function setToggle(id, value) {
  const el = document.getElementById(id);
  if (el) el.classList.toggle('on', !!value);
}

function setSlider(id, value, labelId, suffix) {
  const el = document.getElementById(id);
  const label = document.getElementById(labelId);
  if (el) el.value = value;
  if (label) label.textContent = value + suffix;
}

function setSelect(id, value) {
  const el = document.getElementById(id);
  if (el) el.value = value;
}

function syncUIToConfig() {
  // Toggles - set 'on' class based on builderConfig
  setToggle('togStatusBar', builderConfig.showStatusBar);
  setToggle('togBattery', builderConfig.showBattery);
  setToggle('togClock', builderConfig.showClock);
  setToggle('togWifi', builderConfig.showWifi);
  setToggle('togSleepBattery', builderConfig.showBatterySleep);
  
  // Sliders
  setSlider('fontSizeSlider', builderConfig.fontSize, 'fontSizeVal', 'px');
  
  // Selects
  setSelect('wakeButton', builderConfig.wakeButton);
  
  // Style options - update visual selection state
  selectTheme(builderConfig.theme);
  selectIconStyle(builderConfig.iconStyle);
  selectOrientation(builderConfig.orientation);
  selectLockStyle(builderConfig.lockStyle);
  selectSleepStyle(builderConfig.sleepStyle);
}

async function loadSettings() {
  const data = await api('/api/settings');
  if (!data) {
    console.log('[PORTAL] No settings data received, using defaults');
    return;
  }
  
  // Restore homeItems selection
  if (data.homeItems && Array.isArray(data.homeItems)) {
    selected = new Set(data.homeItems);
    originalSelected = new Set(data.homeItems);
  }
  
  // Restore themeIndex
  if (typeof data.themeIndex === 'number') {
    builderConfig.theme = data.themeIndex;
  }
  
  // Restore display settings
  if (data.display) {
    const d = data.display;
    
    // Orientation & Layout
    builderConfig.orientation = d.orientation === 1 ? 'portrait' : 'landscape';
    const cols = d.hItemsPerRow || 4;
    const rows = d.vItemsPerRow || 2;
    builderConfig.grid = `${cols}x${rows}`;
    
    // Theme & Style
    builderConfig.theme = d.bgTheme ?? 0;
    builderConfig.iconStyle = ['rounded', 'circle', 'square'][d.buttonShape] || 'rounded';
    builderConfig.fontSize = d.fontSize || 12;
    
    // Home screen indicators
    builderConfig.showStatusBar = d.showStatusBar ?? true;
    builderConfig.showBattery = d.showBatteryHome ?? true;
    builderConfig.showClock = d.showClockHome ?? true;
    builderConfig.showWifi = d.showWifi ?? false;
    
    // Lock screen
    builderConfig.lockStyle = ['clock', 'photo', 'quote', 'minimal'][d.lockStyle] || 'clock';
    builderConfig.lockPhotoSource = ['shuffle', 'select', 'folder'][d.lockPhotoSource] || 'shuffle';
    builderConfig.clockStyle = ['digital', 'analog', 'minimal'][d.clockStyle] || 'digital';
    builderConfig.showDate = d.showDate ?? true;
    builderConfig.showBatteryLock = d.showBatteryLock ?? true;
    builderConfig.showWeatherLock = d.showWeatherLock ?? false;
    
    // Sleep screen
    builderConfig.sleepStyle = ['book', 'shuffle', 'off'][d.sleepStyle] || 'off';
    builderConfig.sleepSelectedImage = d.sleepSelectedImage || '';
    builderConfig.showBatterySleep = d.showBatterySleep ?? true;
    builderConfig.sleepMinutes = d.sleepMinutes || 15;
    builderConfig.wakeButton = ['any', 'select', 'power'][d.wakeButton] || 'any';
    
    // Restore display toggles
    const togBootToBook = document.getElementById('togBootToBook');
    const togInvert = document.getElementById('togInvert');
    const togDeepSleep = document.getElementById('togDeepSleep');
    
    if (togBootToBook) togBootToBook.classList.toggle('on', d.bootToLastBook === true);
    if (togInvert) togInvert.classList.toggle('on', d.invertColors === true);
    if (togDeepSleep) togDeepSleep.classList.toggle('on', d.deepSleep !== false);
    
    // Widget visibility toggles (default to on for book/weather, off for orient)
    const togBook = document.getElementById('togBookWidget');
    const togWeather = document.getElementById('togWeatherWidget');
    const togOrient = document.getElementById('togOrientWidget');
    if (togBook) togBook.classList.toggle('on', d.showBookWidget !== false);
    if (togWeather) togWeather.classList.toggle('on', d.showWeatherWidget !== false);
    if (togOrient) togOrient.classList.toggle('on', d.showOrientWidget === true);
    
    // Add widgets to selected set based on device flags
    // Book and Weather default ON, Orient defaults OFF
    if (d.showBookWidget !== false) selected.add('book');
    else selected.delete('book');
    if (d.showWeatherWidget !== false) selected.add('weather');
    else selected.delete('weather');
    if (d.showOrientWidget === true) selected.add('orient');
    else selected.delete('orient');
  }
  
  // Save original state for change detection
  originalConfig = {...builderConfig};
  
  // Restore reader settings
  // Reader settings now managed on-device only
  
  // Update all UI elements to match loaded state
  syncUIToConfig();
  renderPlugins();
  updatePreview();
  
  // Load weather settings
  if (data.weather) {
    const w = data.weather;
    const zipEl = document.getElementById('weatherZip');
    const cityEl = document.getElementById('weatherCity');
    const coordsEl = document.getElementById('weatherCoords');
    const locDiv = document.getElementById('weatherLocation');
    const celsiusEl = document.getElementById('togCelsius');
    
    // Show saved location if set
    if (w.location && w.location.length > 0) {
      if (zipEl) zipEl.value = w.zipCode || '';
      if (cityEl) cityEl.textContent = w.location;
      if (coordsEl) coordsEl.textContent = `${w.latitude?.toFixed(4) || '--'}, ${w.longitude?.toFixed(4) || '--'}`;
      if (locDiv) locDiv.style.display = 'block';
    }
    
    // Celsius toggle
    if (celsiusEl) {
      celsiusEl.classList.toggle('on', w.celsius === true);
    }
  }
  
  console.log('[PORTAL] Settings restored from device');
  
  // Load reading statistics
  loadReadingStats();
  
  // Load KOReader sync settings
  loadKOSyncSettings();
}

// =============================================================================
// READING STATISTICS
// =============================================================================
async function loadReadingStats() {
  const data = await api('/api/stats');
  if (!data) return;
  
  const pagesEl = document.getElementById('statPages');
  const hoursEl = document.getElementById('statHours');
  const booksEl = document.getElementById('statBooks');
  const avgEl = document.getElementById('statAvg');
  
  if (pagesEl) pagesEl.textContent = (data.totalPagesRead || 0).toLocaleString();
  if (hoursEl) {
    const hours = data.totalHoursRead || 0;
    const mins = (data.totalMinutesRead || 0) % 60;
    hoursEl.textContent = hours > 0 ? `${hours}h ${mins}m` : `${mins}m`;
  }
  if (booksEl) booksEl.textContent = data.booksFinished || 0;
  
  // Calculate average (assume 30 days for now)
  const avgPages = Math.round((data.totalPagesRead || 0) / 30);
  if (avgEl) avgEl.textContent = avgPages;
}

async function resetStats() {
  if (!confirm('Are you sure you want to reset all reading statistics? This cannot be undone.')) {
    return;
  }
  
  const result = await api('/api/stats', 'DELETE');
  if (result?.success) {
    toast('Statistics reset', 'success');
    loadReadingStats();
  } else {
    toast('Failed to reset statistics', 'error');
  }
}

// =============================================================================
// WEATHER SETTINGS
// =============================================================================
async function saveWeatherZip() {
  const zipEl = document.getElementById('weatherZip');
  const zip = zipEl?.value?.trim();
  
  if (!zip || zip.length !== 5 || !/^\d{5}$/.test(zip)) {
    toast('Please enter a valid 5-digit ZIP code', 'error');
    return;
  }
  
  toast('Looking up location...', 'info');
  
  // Call API to save ZIP and lookup coordinates
  const result = await api('/api/weather/location', 'POST', { zipCode: zip });
  
  if (result?.success) {
    // Update UI with returned location info
    const cityEl = document.getElementById('weatherCity');
    const coordsEl = document.getElementById('weatherCoords');
    const locDiv = document.getElementById('weatherLocation');
    
    if (cityEl) cityEl.textContent = result.location || zip;
    if (coordsEl) coordsEl.textContent = `${result.latitude?.toFixed(4) || '--'}, ${result.longitude?.toFixed(4) || '--'}`;
    if (locDiv) locDiv.style.display = 'block';
    
    toast('Location saved! Weather will use this ZIP code.', 'success');
  } else {
    toast(result?.error || 'Failed to lookup ZIP code', 'error');
  }
}

async function toggleWeatherUnit(el) {
  el.classList.toggle('on');
  const celsius = el.classList.contains('on');
  
  const result = await api('/api/weather/unit', 'POST', { celsius });
  if (result?.success) {
    toast(`Temperature unit set to ${celsius ? 'Celsius' : 'Fahrenheit'}`, 'success');
  }
}

async function clearWeatherLocation() {
  const result = await api('/api/weather/location', 'POST', { clear: true });
  
  if (result?.success) {
    const zipEl = document.getElementById('weatherZip');
    const locDiv = document.getElementById('weatherLocation');
    
    if (zipEl) zipEl.value = '';
    if (locDiv) locDiv.style.display = 'none';
    
    toast('Location cleared. Weather will auto-detect from IP.', 'success');
  } else {
    toast('Failed to clear location', 'error');
  }
}

async function saveTimezone(value) {
  if (value === 'auto') {
    // Clear manual timezone, use auto-detect
    const result = await api('/api/weather/timezone', 'POST', { auto: true });
    if (result?.success) {
      toast('Timezone set to auto-detect', 'success');
      updateTimeDisplay();
    }
  } else {
    const offset = parseInt(value);
    const result = await api('/api/weather/timezone', 'POST', { offset });
    if (result?.success) {
      toast('Timezone saved', 'success');
      updateTimeDisplay();
    }
  }
}

// Detect precise location using browser geolocation API
async function detectPreciseLocation() {
  if (!navigator.geolocation) {
    toast('Geolocation not supported by browser', 'error');
    return;
  }
  
  toast('Requesting location...', 'info');
  
  navigator.geolocation.getCurrentPosition(
    async (position) => {
      const lat = position.coords.latitude;
      const lon = position.coords.longitude;
      
      // Send to device
      const result = await api('/api/weather/location', 'POST', { 
        latitude: lat, 
        longitude: lon,
        precise: true  // Flag that this is precise (not IP-based)
      });
      
      if (result?.success) {
        // Update UI
        const locDiv = document.getElementById('weatherLocation');
        const coordsEl = document.getElementById('weatherCoords');
        
        if (locDiv) locDiv.textContent = result.location || 'Precise location set';
        if (coordsEl) {
          coordsEl.textContent = `${lat.toFixed(4)}, ${lon.toFixed(4)}`;
          coordsEl.style.display = 'block';
        }
        
        toast('Precise location saved!', 'success');
      } else {
        toast(result?.error || 'Failed to save location', 'error');
      }
    },
    (error) => {
      switch(error.code) {
        case error.PERMISSION_DENIED:
          toast('Location permission denied. Enable in browser settings.', 'error');
          break;
        case error.POSITION_UNAVAILABLE:
          toast('Location unavailable', 'error');
          break;
        case error.TIMEOUT:
          toast('Location request timed out', 'error');
          break;
        default:
          toast('Failed to get location', 'error');
      }
    },
    { enableHighAccuracy: true, timeout: 10000 }
  );
}

// Detect timezone from browser (more accurate than IP)
async function detectBrowserTimezone() {
  try {
    // Get timezone name and offset from browser
    const tzName = Intl.DateTimeFormat().resolvedOptions().timeZone;
    const offsetMinutes = -new Date().getTimezoneOffset(); // JS gives inverted offset
    const offsetSeconds = offsetMinutes * 60;
    
    // Display detected timezone
    const tzEl = document.getElementById('detectedTz');
    if (tzEl) tzEl.textContent = tzName;
    
    // Send to device
    const result = await api('/api/weather/timezone', 'POST', { 
      offset: offsetSeconds,
      tzName: tzName  // Also send name for reference
    });
    
    if (result?.success) {
      // Update dropdown to show current selection
      const select = document.getElementById('timezoneSelect');
      if (select) {
        // Check if we have a matching option
        const options = Array.from(select.options);
        const match = options.find(opt => parseInt(opt.value) === offsetSeconds);
        if (match) {
          select.value = match.value;
        }
      }
      
      toast(`Timezone set to ${tzName}`, 'success');
      updateTimeDisplay();
    } else {
      toast('Failed to save timezone', 'error');
    }
  } catch (e) {
    console.error('Timezone detection error:', e);
    toast('Failed to detect timezone', 'error');
  }
}

// Auto-setup timezone and location on first WiFi connect
async function autoSetupTimezoneAndLocation() {
  try {
    // Check if timezone/location already configured
    const settings = await api('/api/settings');
    const hasLocation = settings?.weather?.latitude && settings?.weather?.latitude !== 0;
    const hasTimezone = settings?.weather?.timezoneOffset && settings?.weather?.timezoneOffset !== 0;
    
    // Auto-set timezone from browser if not configured
    if (!hasTimezone) {
      console.log('[SETUP] Auto-detecting timezone from browser...');
      const tzName = Intl.DateTimeFormat().resolvedOptions().timeZone;
      const offsetMinutes = -new Date().getTimezoneOffset();
      const offsetSeconds = offsetMinutes * 60;
      
      await api('/api/weather/timezone', 'POST', { 
        offset: offsetSeconds,
        tzName: tzName
      });
      console.log(`[SETUP] Timezone auto-set to ${tzName} (UTC${offsetMinutes >= 0 ? '+' : ''}${offsetMinutes/60})`);
    }
    
    // Don't auto-request geolocation - user can click the button if they want precise location
    // IP-based detection will happen on the device when weather is fetched
    
  } catch (e) {
    console.error('[SETUP] Auto-setup error:', e);
  }
}

function updateTimeDisplay() {
  const el = document.getElementById('currentTime');
  if (!el) return;
  
  const now = new Date();
  el.textContent = `Device time: ${now.toLocaleString()}`;
}

// Update time display periodically
setInterval(updateTimeDisplay, 1000);

// Hide UI elements for features disabled on device
async function applyFeatureFlags() {
  const status = await api('/api/status');
  if (!status?.features) return;
  
  const f = status.features;
  
  // Hide Bluetooth/Keyboard page if BT disabled
  if (!f.bluetooth) {
    const btNavItem = document.querySelector('[data-page="bluetooth"]');
    if (btNavItem) btNavItem.style.display = 'none';
  }
  
  // Hide lockscreen tab if disabled
  if (!f.lockscreen) {
    const lockTab = document.querySelector('.builder-tab[onclick*="lockscreen"]');
    if (lockTab) lockTab.style.display = 'none';
  }
  
  // Hide reader-related plugins if reader disabled
  if (!f.reader) {
    const libraryPlugin = document.querySelector('.plugin-item[onclick*="library"]');
    if (libraryPlugin) libraryPlugin.style.display = 'none';
    // Also hide reader settings in Files page
    const readerSettings = document.querySelector('#panel-books .card:last-child');
    if (readerSettings) readerSettings.style.display = 'none';
  }
  
  // Hide flashcards plugin if flashcards disabled
  if (!f.flashcards) {
    const fcPlugin = document.querySelector('.plugin-item[onclick*="flashcards"]');
    if (fcPlugin) fcPlugin.style.display = 'none';
    // Also hide flashcards tab in Files
    const fcTab = document.querySelector('.file-tab[onclick*="flashcards"]');
    if (fcTab) fcTab.style.display = 'none';
  }
  
  // Hide weather plugin if weather disabled
  if (!f.weather) {
    const weatherPlugin = document.querySelector('.plugin-item[onclick*="weather"]');
    if (weatherPlugin) weatherPlugin.style.display = 'none';
    // Also hide weather settings page
    const weatherNavItem = document.querySelector('[data-page="weather"]');
    if (weatherNavItem) weatherNavItem.style.display = 'none';
  }
  
  // Hide games section if games disabled
  if (!f.games) {
    const gamesCategory = document.querySelector('#plugins-games')?.closest('.plugin-category');
    if (gamesCategory) gamesCategory.style.display = 'none';
  }
  
  // Show low-memory indicator
  if (f.lowMemory) {
    const variantEl = document.getElementById('deviceVariant');
    if (variantEl && !variantEl.textContent.includes('Lite')) {
      variantEl.textContent += ' (Lite Mode)';
    }
  }
  
  console.log('[PORTAL] Feature flags applied:', f);
}

// =============================================================================
// BACKUP & RESTORE
// =============================================================================
async function downloadBackup() {
  toast('Preparing backup...', 'info');
  
  try {
    const response = await fetch('/api/backup');
    if (!response.ok) {
      throw new Error('Backup failed');
    }
    
    const data = await response.json();
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `sumi-backup-${new Date().toISOString().split('T')[0]}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    toast('Backup downloaded successfully!', 'success');
  } catch (e) {
    console.error('Backup error:', e);
    toast('Backup failed', 'error');
  }
}

async function uploadRestore(input) {
  const file = input.files[0];
  if (!file) return;
  
  if (!confirm('This will replace all current settings. Are you sure?')) {
    input.value = '';
    return;
  }
  
  toast('Restoring settings...', 'info');
  
  try {
    const text = await file.text();
    const data = JSON.parse(text);
    
    // Validate it's a backup file
    if (!data.backupVersion) {
      throw new Error('Not a valid backup file');
    }
    
    const response = await fetch('/api/restore', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    });
    
    const result = await response.json();
    
    if (result.success) {
      toast('Settings restored! Reloading...', 'success');
      setTimeout(() => location.reload(), 1500);
    } else {
      throw new Error(result.error || 'Restore failed');
    }
  } catch (e) {
    console.error('Restore error:', e);
    toast('Restore failed: ' + e.message, 'error');
  }
  
  input.value = '';
}

document.addEventListener('DOMContentLoaded', async () => {
  await loadStatus();  // This will update the connection banner properly
  await applyFeatureFlags();
  await loadSettings();
  renderPlugins();
  
  // If hostname is 192.168.4.1, we already know we're on hotspot (set at top of file)
  // Only do async detection for sumi.local case
  if (window.location.hostname !== '192.168.4.1' && !window.isOnHotspot) {
    // At sumi.local - try to reach hotspot IP to determine which network
    try {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 2000);
      const resp = await fetch('http://192.168.4.1/api/status', { 
        signal: controller.signal,
        mode: 'cors'
      });
      clearTimeout(timeout);
      if (resp.ok) {
        window.isOnHotspot = true;
        window.hasInternetAccess = false;
        console.log('[PORTAL] Async check: can reach 192.168.4.1 - on hotspot');
      }
    } catch (e) {
      // Can't reach hotspot IP = we're on home network
      window.isOnHotspot = false;
      window.hasInternetAccess = true;
      console.log('[PORTAL] Async check: cannot reach 192.168.4.1 - on home network');
    }
  }
  
  console.log('[PORTAL] Final detection: isOnHotspot =', window.isOnHotspot);
  
  if (window.isOnHotspot) {
    // On hotspot - show WiFi setup first
    showPage('wifi');
  } else {
    // On home network - show Files tab
    showPage('files');
    setTimeout(() => showFileTab('books'), 100);
  }
  
  setInterval(loadStatus, 30000);
});

// Connection banner is updated in loadStatus() based on actual WiFi state

// Handle Enter key in WiFi password
document.getElementById('wifiPassword')?.addEventListener('keypress', e => {
  if (e.key === 'Enter') connectWifi();
});

// =============================================================================
// KOREADER SYNC
// =============================================================================
async function updateKOSyncSettings() {
  const url = document.getElementById('kosyncUrl')?.value || '';
  const user = document.getElementById('kosyncUser')?.value || '';
  const pass = document.getElementById('kosyncPass')?.value || '';
  
  try {
    await fetch('/api/sync/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ url, username: user, password: pass })
    });
  } catch (e) {
    console.error('KOSync settings update failed:', e);
  }
}

async function testKOSync() {
  const statusEl = document.getElementById('kosyncStatus');
  if (statusEl) statusEl.textContent = 'Testing connection...';
  
  try {
    const response = await fetch('/api/sync/test');
    const result = await response.json();
    
    if (result.success) {
      if (statusEl) statusEl.innerHTML = '<span style="color: green;">✓ Connected successfully</span>';
    } else {
      if (statusEl) statusEl.innerHTML = `<span style="color: red;">✗ ${result.error || 'Connection failed'}</span>`;
    }
  } catch (e) {
    if (statusEl) statusEl.innerHTML = '<span style="color: red;">✗ Network error</span>';
  }
}

async function loadKOSyncSettings() {
  try {
    const response = await fetch('/api/sync/settings');
    const data = await response.json();
    
    if (document.getElementById('kosyncUrl')) {
      document.getElementById('kosyncUrl').value = data.url || '';
    }
    if (document.getElementById('kosyncUser')) {
      document.getElementById('kosyncUser').value = data.username || '';
    }
    // Don't load password for security
    
    if (document.getElementById('togKOSync')) {
      document.getElementById('togKOSync').classList.toggle('on', data.enabled);
    }
  } catch (e) {
    console.error('Failed to load KOSync settings:', e);
  }
}

// =============================================================================
// BLUETOOTH PAGE TURNER
// =============================================================================
async function scanBTDevices() {
  toast('Scanning for Bluetooth devices...', 'info');
  
  try {
    const response = await fetch('/api/bluetooth/scan', { method: 'POST' });
    const result = await response.json();
    
    if (result.success) {
      toast('Scanning... Check back in 10 seconds', 'info');
      setTimeout(loadBTDevices, 10000);
    } else {
      toast('Scan failed: ' + (result.error || 'Unknown error'), 'error');
    }
  } catch (e) {
    toast('Failed to start scan', 'error');
  }
}

async function loadBTDevices() {
  try {
    const response = await fetch('/api/bluetooth/devices');
    const data = await response.json();
    
    const container = document.getElementById('btPairedDevices');
    if (!container) return;
    
    if (data.paired && data.paired.length > 0) {
      container.innerHTML = data.paired.map(device => `
        <div style="padding: 8px; border: 1px solid #ddd; border-radius: 4px; margin-bottom: 5px;">
          <strong>${device.name || 'Unknown Device'}</strong>
          <span style="color: ${device.connected ? 'green' : '#999'}; font-size: 12px;">
            ${device.connected ? '● Connected' : '○ Disconnected'}
          </span>
          <button class="btn btn-sm" onclick="connectBTDevice('${device.address}')" style="float: right;">
            ${device.connected ? 'Disconnect' : 'Connect'}
          </button>
        </div>
      `).join('');
    } else {
      container.innerHTML = '<div style="color: #666; font-size: 12px;">No paired devices</div>';
    }
    
    // Update toggle states
    if (document.getElementById('togBluetooth')) {
      document.getElementById('togBluetooth').classList.toggle('on', data.enabled);
    }
    if (document.getElementById('togBTAuto')) {
      document.getElementById('togBTAuto').classList.toggle('on', data.autoConnect);
    }
  } catch (e) {
    console.error('Failed to load BT devices:', e);
  }
}

async function connectBTDevice(address) {
  toast('Connecting...', 'info');
  
  try {
    const response = await fetch(`/api/bluetooth/connect/${address}`, { method: 'POST' });
    const result = await response.json();
    
    if (result.success) {
      toast('Connected!', 'success');
      loadBTDevices();
    } else {
      toast('Connection failed', 'error');
    }
  } catch (e) {
    toast('Connection failed', 'error');
  }
}

