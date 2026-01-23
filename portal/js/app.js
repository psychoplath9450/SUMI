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
const DEFAULTS = ['weather','orient','book','library','flashcards','chess','cube3d','settings'];

// =============================================================================
// STATE
// =============================================================================
let selected = new Set(DEFAULTS);
let originalSelected = new Set(DEFAULTS);
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
  sleepStyle: 'off',
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
  
  const storage = d.storage?.sd_total_mb ? Math.round(d.storage.sd_total_mb) + ' MB' : 'No SD';
  document.getElementById('statStorage').textContent = storage;
  
  // Track WiFi connection state
  wifiConnected = d.wifi?.connected ?? false;
  document.getElementById('statWifi').textContent = wifiConnected ? 'Connected' : 'AP Mode';
  document.getElementById('headerStatus').textContent = wifiConnected ? 'Connected' : 'Setup Mode';
  const statusDot = document.getElementById('statusDot');
  if (statusDot) statusDot.classList.toggle('disconnected', !wifiConnected);
  
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
      },
      // Reader settings
      reader: {
        fontSize: parseInt(document.getElementById('readerFontSize')?.value) || 18,
        lineHeight: parseInt(document.getElementById('readerLineHeight')?.value) || 150,
        margins: parseInt(document.getElementById('readerMargins')?.value) || 20,
        sceneBreakSpacing: parseInt(document.getElementById('readerSceneBreak')?.value) || 30
      }
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
    sleepStyle: 'off', showBatterySleep: true, sleepMinutes: 15, wakeButton: 'any'
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
      document.getElementById('wifiStatus').innerHTML = `
        <div style="color:var(--success);font-weight:600;margin-bottom:8px;">✓ Connected</div>
        <div>Network: ${d.wifi.ssid}</div>
        <div style="color:var(--text-muted);">IP: ${d.wifi.ip}</div>
        <div style="color:var(--text-muted);margin-top:8px;font-size:12px;">📍 Weather location auto-detected from IP</div>
        <button class="btn btn-sm btn-secondary" style="margin-top:12px;" onclick="disconnectWifi()">📴 Disconnect</button>
      `;
    } else {
      document.getElementById('wifiStatus').innerHTML = `
        <div style="color:var(--warning);font-weight:600;margin-bottom:8px;">📡 Hotspot Mode</div>
        <div style="color:var(--text-muted);">Connect to a WiFi network below to access Sumi from your home network</div>
      `;
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
    document.getElementById('wifiList').innerHTML = d.networks.map(n => `
      <div class="wifi-item" onclick="promptWifi('${n.ssid.replace(/'/g,"\\'")}')">
        <span class="wifi-icon">📶</span>
        <div class="wifi-info">
          <div class="wifi-name">${n.ssid}</div>
          <div class="wifi-meta">${n.rssi} dBm ${n.secure ? '🔒' : 'Open'}</div>
        </div>
      </div>
    `).join('');
  } else {
    document.getElementById('wifiList').innerHTML = '<p style="text-align:center;padding:20px;color:var(--text-muted);">No networks found. Try again.</p>';
  }
  
  btn.disabled = false;
  btn.textContent = '🔍 Scan';
}

function promptWifi(ssid) {
  pendingWifiSSID = ssid;
  document.getElementById('wifiModalSSID').textContent = ssid;
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
  
  const result = await api('/api/wifi/connect', 'POST', {ssid: pendingWifiSSID, password: pw});
  
  if (result?.status === 'connected' || result?.status === 'credentials_saved') {
    toast('✓ WiFi credentials saved! Device connecting...', 'success');
  } else {
    toast('Failed to save credentials.', 'error');
  }
  
  setTimeout(loadWifiStatus, 3000);
}

// =============================================================================
// FILES
// =============================================================================
function showFileTab(tab) {
  document.querySelectorAll('.file-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.file-panel').forEach(p => p.classList.remove('active'));
  document.querySelector(`.file-tab[onclick*="${tab}"]`)?.classList.add('active');
  document.getElementById('panel-' + tab)?.classList.add('active');
  refreshFiles(tab);
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
  } else {
    grid.innerHTML = files.map(f => `
      <div class="file-item" onclick="openFile('${type}','${f.name}')">
        <div class="thumb">${type === 'images' ? `<img src="/images/${f.name}" onerror="this.parentElement.innerHTML='🖼️'" style="width:100%;height:100%;object-fit:cover;">` : (type === 'books' ? getBookIcon(f.name) : '📝')}</div>
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
    formData.append('file', file);
    formData.append('path', paths[type]);
    try {
      await fetch('/api/files/upload', {method:'POST', body:formData});
      toast('Uploaded: ' + file.name, 'success');
    } catch(e) {
      toast('Upload failed: ' + file.name, 'error');
    }
  }
  refreshFiles(type);
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

function toggleReaderOpt(el, key) {
  el.classList.toggle('on');
  api('/api/reader/settings', 'POST', {[key]: el.classList.contains('on')});
}

function updateReaderPreview() {
  const fontSize = document.getElementById('readerFontSize').value;
  const lineHeight = document.getElementById('readerLineHeight').value;
  const margins = document.getElementById('readerMargins').value;
  const justify = document.getElementById('togJustify').classList.contains('on');
  
  // Update value labels
  document.getElementById('readerFontVal').textContent = fontSize + 'px';
  document.getElementById('readerLineVal').textContent = lineHeight + '%';
  document.getElementById('readerMarginVal').textContent = margins + 'px';
  
  // Update preview - scale down for the mini preview
  const preview = document.getElementById('readerPreview');
  if (preview) {
    const scaledFont = Math.max(8, Math.round(fontSize * 0.5));
    const scaledMargin = Math.max(4, Math.round(margins * 0.4));
    preview.style.fontSize = scaledFont + 'px';
    preview.style.lineHeight = (lineHeight / 100);
    preview.style.padding = scaledMargin + 'px';
    preview.style.textAlign = justify ? 'justify' : 'left';
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
    
    // Widget visibility toggles (default to on)
    const togBook = document.getElementById('togBookWidget');
    const togWeather = document.getElementById('togWeatherWidget');
    const togOrient = document.getElementById('togOrientWidget');
    if (togBook) togBook.classList.toggle('on', d.showBookWidget !== false);
    if (togWeather) togWeather.classList.toggle('on', d.showWeatherWidget !== false);
    if (togOrient) togOrient.classList.toggle('on', d.showOrientWidget !== false);
  }
  
  // Save original state for change detection
  originalConfig = {...builderConfig};
  
  // Restore reader settings
  if (data.reader) {
    const r = data.reader;
    const fsEl = document.getElementById('readerFontSize');
    const lhEl = document.getElementById('readerLineHeight');
    const mgEl = document.getElementById('readerMargins');
    const sbEl = document.getElementById('readerSceneBreak');
    if (fsEl) { fsEl.value = r.fontSize || 18; updateSlider(fsEl, 'readerFontVal', 'px'); }
    if (lhEl) { lhEl.value = r.lineHeight || 150; updateSlider(lhEl, 'readerLineVal', '%'); }
    if (mgEl) { mgEl.value = r.margins || 20; updateSlider(mgEl, 'readerMarginVal', 'px'); }
    if (sbEl) { sbEl.value = r.sceneBreakSpacing || 30; updateSlider(sbEl, 'readerSceneVal', 'px'); }
  }
  
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
  await loadStatus();
  await applyFeatureFlags();
  await loadSettings();
  renderPlugins();
  updateReaderPreview();
  setInterval(loadStatus, 30000);
});

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

