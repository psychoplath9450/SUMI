/**
 * @file portal_html.h
 * @brief Embedded web portal for Sumi - Auto-generated
 * 
 * This file is auto-generated from portal/templates, portal/css, and portal/js
 * Do not edit directly - edit the source files and run: python portal/build.py
 */

#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

#include <pgmspace.h>

const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Sumi Setup</title>
<style>
/* === styles.css === */
/* =============================================================================
   SUMI Portal - Classic Mac OS Style
   ============================================================================= */

:root {
  --mac-white: #ffffff;
  --mac-black: #000000;
  --mac-gray: #dddddd;
  --mac-gray-dark: #999999;
  --mac-gray-light: #eeeeee;
  --mac-highlight: #3366cc;
  --mac-highlight-text: #ffffff;
  --mac-desktop: #6699cc;
  --mac-success: #339933;
  --mac-error: #cc3333;
  --mac-warning: #cc9933;
}

* { box-sizing: border-box; margin: 0; padding: 0; }

body {
  font-family: 'Chicago', 'Charcoal', 'Geneva', 'Helvetica Neue', Helvetica, sans-serif;
  font-size: 12px;
  background: var(--mac-gray);
  min-height: 100vh;
  color: var(--mac-black);
}

/* =============================================================================
   WINDOW FRAME
   ============================================================================= */
.app {
  background: var(--mac-white);
  border: none;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

/* Title Bar */
.header {
  background: linear-gradient(180deg, #fff 0%, #fff 50%, #ccc 50%, #ccc 100%);
  background-size: 100% 2px;
  border-bottom: 1px solid var(--mac-black);
  padding: 2px 8px;
  display: flex;
  align-items: center;
  gap: 8px;
  flex-shrink: 0;
}

.window-controls {
  display: flex;
  gap: 6px;
}

.window-control {
  width: 12px;
  height: 12px;
  border: 1px solid var(--mac-black);
  background: var(--mac-white);
}

.close-btn { position: relative; }
.close-btn::before, .close-btn::after {
  content: '';
  position: absolute;
  background: var(--mac-black);
}
.close-btn::before { width: 8px; height: 1px; top: 5px; left: 1px; }
.close-btn::after { width: 1px; height: 8px; top: 1px; left: 5px; }

.logo {
  font-weight: bold;
  font-size: 12px;
  padding: 0 8px;
}

.header-status {
  flex: 1;
  display: flex;
  justify-content: flex-end;
  align-items: center;
  gap: 12px;
  font-size: 11px;
}

.status-badge {
  display: flex;
  align-items: center;
  gap: 4px;
}

.status-dot {
  width: 8px;
  height: 8px;
  background: var(--mac-success);
  border-radius: 50%;
  border: 1px solid var(--mac-black);
}

.status-dot.disconnected { background: var(--mac-error); }

/* =============================================================================
   MAIN LAYOUT
   ============================================================================= */
.main {
  display: flex;
  flex: 1;
  background: var(--mac-white);
  overflow: hidden;
  height: calc(100vh - 30px); /* Full height minus header */
}

/* Sidebar - always visible */
.sidebar {
  width: 120px;
  background: var(--mac-white);
  border-right: 1px solid var(--mac-black);
  flex-shrink: 0;
  overflow-y: auto;
}

.nav-section-title {
  background: var(--mac-gray);
  padding: 2px 6px;
  font-size: 9px;
  font-weight: bold;
  border-bottom: 1px solid var(--mac-black);
  border-top: 1px solid var(--mac-black);
  text-transform: uppercase;
}

.nav-section:first-child .nav-section-title {
  border-top: none;
}

.nav-item {
  padding: 3px 6px 3px 12px;
  cursor: pointer;
  display: flex;
  align-items: center;
  gap: 4px;
  border-bottom: 1px solid var(--mac-gray);
  font-size: 11px;
}

.nav-item:hover {
  background: var(--mac-highlight);
  color: var(--mac-white);
}

.nav-item.active {
  background: var(--mac-highlight);
  color: var(--mac-white);
}

.nav-icon { width: 14px; text-align: center; font-size: 11px; }

/* Content Area */
.content {
  flex: 1;
  padding: 10px;
  background: var(--mac-gray-light);
  overflow-y: auto;
  display: flex;
  flex-direction: column;
}

/* =============================================================================
   PANELS & CARDS
   ============================================================================= */
.card, .panel {
  border: 1px solid var(--mac-black);
  margin-bottom: 8px;
  background: var(--mac-white);
}

.card-header, .panel-header {
  background: linear-gradient(180deg, var(--mac-gray), #eee);
  border-bottom: 1px solid var(--mac-black);
  padding: 3px 8px;
  font-weight: bold;
  font-size: 11px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.card-title { font-weight: bold; }

.card-body, .panel-body { padding: 8px; }

/* Page Header */
.page-header {
  margin-bottom: 8px;
  padding-bottom: 6px;
  border-bottom: 1px solid var(--mac-gray);
}

.page-title {
  font-size: 13px;
  font-weight: bold;
  margin-bottom: 1px;
}

.page-desc {
  font-size: 10px;
  color: var(--mac-gray-dark);
}

/* =============================================================================
   BUTTONS
   ============================================================================= */
.btn {
  background: linear-gradient(180deg, var(--mac-white), var(--mac-gray));
  border: 1px solid var(--mac-black);
  border-radius: 4px;
  padding: 4px 12px;
  font-family: inherit;
  font-size: 11px;
  cursor: pointer;
  box-shadow: 0 1px 0 var(--mac-black);
  white-space: nowrap;
}

.btn:active {
  background: linear-gradient(180deg, var(--mac-gray), var(--mac-white));
  box-shadow: none;
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-primary {
  background: linear-gradient(180deg, #4488ee, #2266cc);
  color: white;
  border-color: #1155aa;
}

.btn-primary:active {
  background: linear-gradient(180deg, #2266cc, #4488ee);
}

.btn-secondary {
  background: linear-gradient(180deg, var(--mac-white), var(--mac-gray));
}

.btn-danger {
  background: linear-gradient(180deg, #ff6666, #cc3333);
  color: white;
  border-color: #aa2222;
}

.btn-sm { padding: 2px 8px; font-size: 10px; }

.btn-group {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
}

/* =============================================================================
   ACTION BAR (Footer inside window)
   ============================================================================= */
.action-bar {
  background: linear-gradient(180deg, var(--mac-gray), #bbb);
  border-top: 1px solid var(--mac-black);
  padding: 6px 12px;
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  flex-shrink: 0;
}

/* =============================================================================
   FORM ELEMENTS
   ============================================================================= */
.checkbox, label.checkbox {
  display: flex;
  align-items: center;
  gap: 6px;
  margin: 4px 0;
  cursor: pointer;
  font-size: 11px;
}

.checkbox input[type="checkbox"], input[type="checkbox"] {
  appearance: none;
  width: 12px;
  height: 12px;
  border: 1px solid var(--mac-black);
  border-radius: 2px;
  background: var(--mac-white);
  cursor: pointer;
  position: relative;
  flex-shrink: 0;
}

.checkbox input[type="checkbox"]:checked::after, input[type="checkbox"]:checked::after {
  content: '✓';
  position: absolute;
  top: -2px;
  left: 1px;
  font-size: 12px;
  font-weight: bold;
}

.radio, label.radio {
  display: flex;
  align-items: center;
  gap: 6px;
  margin: 4px 0;
  cursor: pointer;
  font-size: 11px;
}

.radio input[type="radio"], input[type="radio"] {
  appearance: none;
  width: 12px;
  height: 12px;
  border: 1px solid var(--mac-black);
  border-radius: 50%;
  background: var(--mac-white);
  cursor: pointer;
  flex-shrink: 0;
}

.radio input[type="radio"]:checked, input[type="radio"]:checked {
  background: radial-gradient(circle, var(--mac-black) 35%, var(--mac-white) 35%);
}

/* Toggle Switch */
.toggle {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 0;
}

.toggle-label { font-size: 11px; }
.toggle-desc { font-size: 10px; color: var(--mac-gray-dark); margin-top: 2px; }

.toggle-switch {
  width: 36px;
  height: 18px;
  background: var(--mac-gray);
  border: 1px solid var(--mac-black);
  border-radius: 9px;
  cursor: pointer;
  position: relative;
  transition: background 0.2s;
}

.toggle-switch::after {
  content: '';
  position: absolute;
  width: 14px;
  height: 14px;
  background: var(--mac-white);
  border: 1px solid var(--mac-black);
  border-radius: 50%;
  top: 1px;
  left: 1px;
  transition: left 0.2s;
}

.toggle-switch.on {
  background: var(--mac-highlight);
}

.toggle-switch.on::after {
  left: 19px;
}

/* Text Input */
.text-input, .field, input[type="text"], input[type="password"], input[type="number"] {
  background: var(--mac-white);
  border: 1px solid var(--mac-black);
  padding: 4px 6px;
  font-family: inherit;
  font-size: 11px;
  box-shadow: inset 1px 1px 2px rgba(0,0,0,0.1);
}

.text-input:focus, .field:focus, input[type="text"]:focus {
  outline: 2px solid var(--mac-highlight);
}

/* Dropdown/Select */
select, .dropdown, .select-wrap select {
  background: linear-gradient(180deg, var(--mac-white), var(--mac-gray));
  border: 1px solid var(--mac-black);
  border-radius: 4px;
  padding: 4px 20px 4px 6px;
  font-family: inherit;
  font-size: 11px;
  appearance: none;
  cursor: pointer;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M0 0 L5 6 L10 0' fill='%23000'/%3E%3C/svg%3E");
  background-repeat: no-repeat;
  background-position: right 5px center;
}

/* Slider */
.slider-row {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 6px 0;
  font-size: 11px;
}

.slider-row label, .slider-label { min-width: 80px; }
.slider-row input[type="range"], .slider-input { flex: 1; }
.slider-row .value, .slider-value { min-width: 45px; text-align: right; font-size: 10px; }

input[type="range"] {
  height: 12px;
  background: var(--mac-gray);
  border: 1px solid var(--mac-black);
  border-radius: 6px;
  cursor: pointer;
}

/* Form Row */
.form-row, .input-row {
  display: flex;
  align-items: center;
  margin: 6px 0;
  gap: 8px;
  font-size: 11px;
}

.form-row label, .input-label { min-width: 80px; }

/* =============================================================================
   TABS
   ============================================================================= */
.tabs, .builder-tabs, .file-tabs {
  display: flex;
  margin-bottom: 0;
  padding-left: 4px;
  gap: 1px;
  flex-wrap: wrap;
}

.tab, .builder-tab, .file-tab {
  background: linear-gradient(180deg, var(--mac-gray), #bbb);
  border: 1px solid var(--mac-black);
  border-bottom: none;
  border-radius: 6px 6px 0 0;
  padding: 4px 10px;
  cursor: pointer;
  font-size: 10px;
  position: relative;
  top: 1px;
  white-space: nowrap;
}

.tab.active, .builder-tab.active, .file-tab.active {
  background: var(--mac-white);
  border-bottom: 1px solid var(--mac-white);
}

.tab-content, .builder-panel, .file-panel {
  border: 1px solid var(--mac-black);
  padding: 12px;
  background: var(--mac-white);
}

.builder-panel, .file-panel { display: none; }
.builder-panel.active, .file-panel.active { display: block; }

.file-tab .count {
  background: var(--mac-gray-dark);
  color: white;
  font-size: 9px;
  padding: 1px 4px;
  border-radius: 8px;
  margin-left: 4px;
}

/* =============================================================================
   LISTS
   ============================================================================= */
.list-view {
  background: var(--mac-white);
  border: 1px solid var(--mac-black);
}

.list-view-header {
  display: flex;
  background: linear-gradient(180deg, var(--mac-gray), #bbb);
  border-bottom: 1px solid var(--mac-black);
  font-size: 10px;
  font-weight: bold;
}

.list-view-header > div {
  padding: 2px 6px;
  border-right: 1px solid var(--mac-gray-dark);
}

.list-view-row, .wifi-item {
  display: flex;
  padding: 4px 8px;
  border-bottom: 1px solid var(--mac-gray);
  align-items: center;
  gap: 6px;
  cursor: pointer;
  font-size: 11px;
}

.list-view-row:hover, .wifi-item:hover {
  background: var(--mac-highlight);
  color: var(--mac-white);
}

.wifi-icon { font-size: 14px; }
.wifi-info { flex: 1; }
.wifi-name { font-weight: bold; }
.wifi-meta { font-size: 10px; color: var(--mac-gray-dark); }
.wifi-item:hover .wifi-meta { color: rgba(255,255,255,0.7); }

/* =============================================================================
   STATS GRID
   ============================================================================= */
.stats-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 8px;
  margin-bottom: 12px;
}

.stat-card {
  background: var(--mac-white);
  border: 1px solid var(--mac-black);
  padding: 10px 6px;
  text-align: center;
}

.stat-icon { font-size: 16px; margin-bottom: 4px; }
.stat-value { font-size: 16px; font-weight: bold; color: var(--mac-highlight); }
.stat-label { font-size: 9px; color: var(--mac-gray-dark); margin-top: 2px; }

/* =============================================================================
   PLUGIN GRID
   ============================================================================= */
.plugin-category { margin-bottom: 8px; }
.plugin-category h4 {
  font-size: 10px;
  font-weight: bold;
  margin-bottom: 4px;
  padding-bottom: 3px;
  border-bottom: 1px solid var(--mac-gray);
}

.plugin-grid, .icon-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(60px, 1fr));
  gap: 4px;
  padding: 4px;
  background: var(--mac-white);
  border: 1px solid var(--mac-gray);
}

.plugin-card, .icon-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 6px 3px;
  cursor: pointer;
  text-align: center;
  border: 2px solid transparent;
  border-radius: 4px;
  transition: all 0.15s;
}

.plugin-card:hover, .icon-item:hover {
  background: rgba(51, 102, 204, 0.1);
  border-color: var(--mac-highlight);
}

.plugin-card.selected, .icon-item.selected {
  background: var(--mac-highlight);
  color: var(--mac-white);
  border-color: var(--mac-highlight);
}

.plugin-card.locked {
  opacity: 0.6;
  cursor: default;
}

.plugin-icon { font-size: 22px; margin-bottom: 2px; }
.plugin-name, .icon-item .label { font-size: 9px; font-weight: bold; }
.plugin-desc { font-size: 8px; color: var(--mac-gray-dark); margin-top: 1px; }
.plugin-card.selected .plugin-desc { color: rgba(255,255,255,0.7); }

/* =============================================================================
   STYLE OPTIONS
   ============================================================================= */
.style-options {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 6px;
}

.style-option {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  padding: 8px 4px;
  background: var(--mac-white);
  border: 2px solid var(--mac-gray);
  border-radius: 4px;
  cursor: pointer;
  font-size: 9px;
  transition: all 0.15s;
}

.style-option:hover { border-color: var(--mac-gray-dark); }
.style-option.selected { border-color: var(--mac-highlight); background: rgba(51,102,204,0.1); }

.theme-preview {
  width: 40px;
  height: 26px;
  border-radius: 3px;
  border: 1px solid var(--mac-gray-dark);
}

.theme-preview.light { background: #f5f5f0; }
.theme-preview.dark { background: #1a1a1a; }
.theme-preview.contrast { background: #fff; border: 2px solid #000; }

.icon-preview {
  width: 28px;
  height: 28px;
  background: var(--mac-gray);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 10px;
  font-weight: bold;
}

.icon-preview.rounded { border-radius: 6px; }
.icon-preview.square { border-radius: 2px; }
.icon-preview.circle { border-radius: 50%; }

.orient-preview {
  background: var(--mac-gray);
  border: 1px solid var(--mac-black);
  border-radius: 3px;
}

/* =============================================================================
   BUILDER PREVIEW
   ============================================================================= */
.builder-layout {
  display: grid;
  grid-template-columns: 1fr 240px;
  gap: 10px;
  flex: 1;
  min-height: 0; /* Allow shrinking */
}

.builder-options { 
  overflow-y: auto; 
  flex: 1;
  min-height: 0;
}

.builder-preview {
  background: var(--mac-gray);
  border: 1px solid var(--mac-black);
  padding: 8px;
  position: sticky;
  top: 0;
  height: fit-content;
  max-height: calc(100vh - 120px);
}

.preview-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
  font-size: 10px;
  font-weight: bold;
}

.preview-badge {
  background: var(--mac-highlight);
  color: white;
  font-size: 9px;
  padding: 2px 6px;
  border-radius: 3px;
}

.preview-tabs {
  display: flex;
  gap: 2px;
  margin-bottom: 8px;
}

.preview-tab {
  flex: 1;
  padding: 4px;
  text-align: center;
  font-size: 9px;
  background: var(--mac-white);
  border: 1px solid var(--mac-black);
  cursor: pointer;
}

.preview-tab:first-child { border-radius: 3px 0 0 3px; }
.preview-tab:last-child { border-radius: 0 3px 3px 0; }

.preview-tab.active {
  background: var(--mac-highlight);
  color: white;
}

/* Device Frame */
.device-container { display: flex; justify-content: center; padding: 8px; }

.device-outer {
  background: linear-gradient(145deg, #444, #222);
  border-radius: 12px;
  padding: 6px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.4);
  transition: all 0.3s;
}

.device-outer.landscape { width: 100%; max-width: 280px; }
.device-outer.portrait { width: 70%; max-width: 180px; }

.device-inner {
  background: #111;
  border-radius: 8px;
  padding: 4px;
}

.eink-screen {
  border-radius: 6px;
  position: relative;
  overflow: hidden;
}

.device-outer.landscape .eink-screen { aspect-ratio: 800/480; }
.device-outer.portrait .eink-screen { aspect-ratio: 480/800; }

/* E-Ink Themes */
.eink-screen.light { background: #f5f5f0; color: #1a1a1a; }
.eink-screen.dark { background: #1a1a1a; color: #e5e5e0; }
.eink-screen.contrast { background: #ffffff; color: #000000; }

/* Status Bar */
.eink-statusbar {
  height: 12px;
  padding: 1px 4px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 6px;
  font-weight: 500;
  border-bottom: 1px solid currentColor;
  opacity: 0.6;
}

.statusbar-section { display: flex; align-items: center; gap: 4px; }

/* Home Grid */
.eink-grid {
  display: grid;
  padding: 4px;
  gap: 3px;
  height: calc(100% - 12px);
  align-content: center;
}

.eink-grid.no-status { height: 100%; }
.eink-grid.l-2x2 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(2,1fr); }
.eink-grid.l-3x2 { grid-template-columns: repeat(3,1fr); grid-template-rows: repeat(2,1fr); }
.eink-grid.l-4x2 { grid-template-columns: repeat(4,1fr); grid-template-rows: repeat(2,1fr); }
.eink-grid.l-5x3 { grid-template-columns: repeat(5,1fr); grid-template-rows: repeat(3,1fr); }
.eink-grid.p-2x2 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(2,1fr); }
.eink-grid.p-2x3 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(3,1fr); }
.eink-grid.p-2x4 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(4,1fr); }
.eink-grid.p-3x5 { grid-template-columns: repeat(3,1fr); grid-template-rows: repeat(5,1fr); }

.eink-app {
  display: flex;
  align-items: center;
  justify-content: center;
}

.eink-app-box {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
  padding: 2px;
  text-align: center;
  font-weight: 600;
  font-size: 5px;
  line-height: 1;
}

.light .eink-app-box { background: #fff; border: 1px solid #333; color: #333; }
.dark .eink-app-box { background: #333; border: 1px solid #666; color: #eee; }
.contrast .eink-app-box { background: #fff; border: 2px solid #000; color: #000; }

.eink-app-box.style-rounded { border-radius: 3px; }
.eink-app-box.style-square { border-radius: 1px; }
.eink-app-box.style-circle { border-radius: 50%; }

.eink-app.selected .eink-app-box { background: #333; color: #fff; }
.dark .eink-app.selected .eink-app-box { background: #fff; color: #333; }
.eink-app.empty .eink-app-box { opacity: 0.25; background: transparent !important; border-style: dashed; }

/* Widget cells - smaller than app cells */
.eink-widget-cell {
  display: flex;
  align-items: center;
  justify-content: center;
}

.eink-widget-cell .eink-app-box {
  width: 100%;
  height: 100%;
  min-height: 24px;
}

/* Lock Screen */
.eink-lock {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  text-align: center;
  position: relative;
}

.lock-time-display { font-size: 20px; font-weight: 100; }
.lock-date-display { font-size: 7px; opacity: 0.5; margin-top: 2px; }
.lock-quote-display { font-style: italic; padding: 8px; font-size: 6px; opacity: 0.7; }
.lock-minimal-display { width: 20px; height: 20px; border: 1px solid currentColor; border-radius: 50%; opacity: 0.3; }
.lock-info { position: absolute; bottom: 6px; font-size: 6px; opacity: 0.4; display: flex; gap: 8px; }
.lock-bg { position: absolute; inset: 0; opacity: 0.1; }
.lock-bg.photo { background: linear-gradient(135deg, currentColor, transparent); }

/* Sleep Screen */
.eink-sleep { height: 100%; display: flex; align-items: center; justify-content: center; position: relative; }
.sleep-display { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 4px; }
.sleep-display.off { background: #080808; width: 100%; height: 100%; }
.sleep-battery-warn { position: absolute; bottom: 6px; right: 6px; font-size: 6px; opacity: 0.3; }

/* =============================================================================
   UPLOAD ZONE
   ============================================================================= */
.upload-zone {
  border: 2px dashed var(--mac-gray-dark);
  padding: 20px;
  text-align: center;
  margin-bottom: 10px;
  cursor: pointer;
  transition: all 0.2s;
}

.upload-zone:hover, .upload-zone.dragover {
  border-color: var(--mac-highlight);
  background: rgba(51, 102, 204, 0.05);
}

.upload-zone .icon { font-size: 28px; margin-bottom: 6px; }
.upload-zone .text { font-size: 11px; }
.upload-zone .hint { font-size: 9px; color: var(--mac-gray-dark); margin-top: 4px; }
.upload-zone input[type="file"] { display: none; }

/* =============================================================================
   FILE GRID
   ============================================================================= */
.file-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(100px, 1fr));
  gap: 10px;
}

.file-item {
  background: var(--mac-white);
  border: 1px solid var(--mac-gray);
  padding: 10px;
  text-align: center;
  cursor: pointer;
  position: relative;
  transition: all 0.15s;
}

.file-item:hover {
  border-color: var(--mac-highlight);
  background: rgba(51,102,204,0.05);
}

.file-item .thumb {
  width: 48px;
  height: 48px;
  margin: 0 auto 6px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 32px;
  background: var(--mac-gray-light);
  border-radius: 4px;
  overflow: hidden;
}

.file-item .name {
  font-size: 10px;
  font-weight: bold;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.file-item .meta {
  font-size: 9px;
  color: var(--mac-gray-dark);
  margin-top: 2px;
}

.file-empty {
  grid-column: 1 / -1;
  text-align: center;
  padding: 30px;
  color: var(--mac-gray-dark);
}

.file-empty .icon { font-size: 32px; margin-bottom: 8px; }

.deck-card {
  background: var(--mac-white);
  border: 1px solid var(--mac-gray);
  padding: 12px;
  cursor: pointer;
  transition: all 0.15s;
}

.deck-card:hover {
  border-color: var(--mac-highlight);
}

/* =============================================================================
   PROGRESS BAR
   ============================================================================= */
.progress {
  height: 14px;
  border: 1px solid var(--mac-black);
  border-radius: 7px;
  background: var(--mac-white);
  overflow: hidden;
  padding: 2px;
}

.progress-bar {
  height: 100%;
  border-radius: 5px;
  background: repeating-linear-gradient(
    -45deg,
    var(--mac-highlight),
    var(--mac-highlight) 3px,
    #5588dd 3px,
    #5588dd 6px
  );
  transition: width 0.3s;
}

/* =============================================================================
   TOAST
   ============================================================================= */
.toast {
  position: fixed;
  bottom: 60px;
  left: 50%;
  transform: translateX(-50%) translateY(20px);
  background: var(--mac-black);
  color: var(--mac-white);
  padding: 8px 16px;
  border-radius: 4px;
  border: 1px solid var(--mac-gray-dark);
  opacity: 0;
  transition: all 0.3s;
  font-size: 11px;
  z-index: 1000;
  box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}

.toast.show { opacity: 1; transform: translateX(-50%) translateY(0); }
.toast.success { background: var(--mac-success); }
.toast.error { background: var(--mac-error); }

/* =============================================================================
   MODAL
   ============================================================================= */
.modal {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 200;
  opacity: 0;
  visibility: hidden;
  transition: all 0.2s;
}

.modal.show { opacity: 1; visibility: visible; }

.modal-content {
  background: var(--mac-white);
  border: 1px solid var(--mac-black);
  border-radius: 8px;
  width: 90%;
  max-width: 320px;
  box-shadow: 4px 4px 0 rgba(0,0,0,0.3);
}

.modal-header {
  background: linear-gradient(180deg, #fff 0%, #fff 50%, #ccc 50%, #ccc 100%);
  background-size: 100% 2px;
  padding: 6px 10px;
  border-radius: 8px 8px 0 0;
  border-bottom: 1px solid var(--mac-black);
  font-weight: bold;
  font-size: 11px;
}

.modal-body { padding: 16px; }

.modal-footer {
  padding: 10px 16px;
  border-top: 1px solid var(--mac-gray);
  display: flex;
  justify-content: flex-end;
  gap: 8px;
}

/* =============================================================================
   PAGES
   ============================================================================= */
.page { 
  display: none; 
  flex: 1;
  flex-direction: column;
  min-height: 0;
}
.page.active { 
  display: flex; 
}

/* Make builder page fill the space */
#page-builder.active {
  display: flex;
  flex-direction: column;
}

#page-builder .builder-layout {
  flex: 1;
  min-height: 0;
}

/* =============================================================================
   OPTION SECTIONS
   ============================================================================= */
.option-section {
  margin-bottom: 16px;
}

.option-section h3 {
  font-size: 10px;
  font-weight: bold;
  color: var(--mac-gray-dark);
  margin-bottom: 8px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  padding-bottom: 4px;
  border-bottom: 1px solid var(--mac-gray);
}

/* =============================================================================
   RESPONSIVE
   ============================================================================= */
@media (max-width: 800px) {
  .builder-layout { grid-template-columns: 1fr; }
  .builder-preview { position: relative; order: -1; }
  .stats-grid { grid-template-columns: repeat(2, 1fr); }
  
  .sidebar {
    width: 140px;
  }
  
  .nav-item {
    padding: 3px 6px 3px 12px;
    font-size: 10px;
  }
  
  .nav-icon { font-size: 11px; }
}

@media (max-width: 500px) {
  .content { padding: 8px; }
  .style-options { grid-template-columns: repeat(2, 1fr); }
  .plugin-grid { grid-template-columns: repeat(3, 1fr); }
  
  .sidebar { width: 120px; }
  
  .nav-section-title { font-size: 8px; padding: 2px 6px; }
  .nav-item { padding: 3px 4px 3px 8px; font-size: 9px; }
}

/* =============================================================================
   HELPERS
   ============================================================================= */
.flex { display: flex; }
.gap-6 { gap: 6px; }
.gap-8 { gap: 8px; }
.flex-wrap { flex-wrap: wrap; }
.mb-8 { margin-bottom: 8px; }
.mt-8 { margin-top: 8px; }
.text-center { text-align: center; }
.text-muted { color: var(--mac-gray-dark); }

</style>
</head>
<body>
<div class="app">
  <header class="header">
    <div class="window-controls">
      <div class="window-control close-btn"></div>
      <div class="window-control"></div>
    </div>
    <div class="logo">📟 SUMI</div>
    <div class="header-status">
      <div class="status-badge">
        <span class="status-dot" id="statusDot"></span>
        <span id="headerStatus">Checking...</span>
      </div>
      <span id="headerBattery">🔋 --%</span>
    </div>
  </header>
  
  <div class="main">
    <nav class="sidebar" id="sidebar">
      <div class="nav-section">
        <div class="nav-section-title">Setup</div>
        <div class="nav-item active" data-page="home"><span class="nav-icon">🏠</span>Dashboard</div>
        <div class="nav-item" data-page="builder"><span class="nav-icon">🎨</span>Customize</div>
        <div class="nav-item" data-page="wifi"><span class="nav-icon">📶</span>WiFi</div>
      </div>
      <div class="nav-section">
        <div class="nav-section-title">Content</div>
        <div class="nav-item" data-page="files"><span class="nav-icon">📁</span>Files</div>
      </div>
      <div class="nav-section">
        <div class="nav-section-title">Settings</div>
        <div class="nav-item" data-page="weather"><span class="nav-icon">🌤️</span>Weather</div>
        <div class="nav-item" data-page="display"><span class="nav-icon">🖥️</span>Display</div>
        <div class="nav-item" data-page="bluetooth"><span class="nav-icon">⌨️</span>Keyboard</div>
        <div class="nav-item" data-page="power"><span class="nav-icon">🔋</span>Power</div>
        <div class="nav-item" data-page="about"><span class="nav-icon">ℹ️</span>About</div>
      </div>
    </nav>
    
    <main class="content">
      <!-- ==================== HOME PAGE ==================== -->
      <div class="page active" id="page-home">
        <div class="page-header">
          <h1 class="page-title">Welcome to Sumi</h1>
          <p class="page-desc">Your e-ink companion is ready to be configured</p>
        </div>
        
        <div class="stats-grid">
          <div class="stat-card">
            <div class="stat-icon">🔋</div>
            <div class="stat-value" id="statBattery">--%</div>
            <div class="stat-label">Battery</div>
          </div>
          <div class="stat-card">
            <div class="stat-icon">💾</div>
            <div class="stat-value" id="statStorage">--</div>
            <div class="stat-label">Storage</div>
          </div>
          <div class="stat-card">
            <div class="stat-icon">📶</div>
            <div class="stat-value" id="statWifi">--</div>
            <div class="stat-label">WiFi</div>
          </div>
          <div class="stat-card">
            <div class="stat-icon">⏱️</div>
            <div class="stat-value" id="statUptime">--</div>
            <div class="stat-label">Uptime</div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Quick Setup</span>
          </div>
          <div class="card-body">
            <p class="text-muted mb-8">Get started with these essential steps:</p>
            <div class="btn-group" style="display:grid;grid-template-columns:repeat(2,1fr);gap:8px;">
              <button class="btn btn-primary" onclick="showPage('wifi')">📶 Connect WiFi</button>
              <button class="btn btn-secondary" onclick="showPage('builder')">🎨 Customize</button>
              <button class="btn btn-secondary" onclick="showPage('files')">📁 Manage Files</button>
              <button class="btn btn-secondary" onclick="showPage('bluetooth')">⌨️ Pair Keyboard</button>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Device Info</span>
          </div>
          <div class="card-body">
            <table style="width:100%;font-size:11px;">
              <tr><td class="text-muted" style="padding:6px 0">Firmware</td><td style="text-align:right" id="infoFirmware">--</td></tr>
              <tr><td class="text-muted" style="padding:6px 0">Display</td><td style="text-align:right">4.26" 800×480 E-Ink</td></tr>
              <tr><td class="text-muted" style="padding:6px 0">IP Address</td><td style="text-align:right" id="infoIP">--</td></tr>
              <tr><td class="text-muted" style="padding:6px 0">Free Memory</td><td style="text-align:right" id="infoMem">--</td></tr>
            </table>
          </div>
        </div>
      </div>

      <!-- ==================== BUILDER PAGE ==================== -->
      <div class="page" id="page-builder">
        <div class="page-header">
          <h1 class="page-title">Customize Your Sumi</h1>
          <p class="page-desc">Choose your apps, theme, and layout</p>
        </div>
        
        <div class="builder-layout">
          <div class="builder-options">
            <div class="builder-tabs">
              <div class="builder-tab active" onclick="showBuilderTab('apps')">📱 Apps</div>
              <div class="builder-tab" onclick="showBuilderTab('theme')">🎨 Theme</div>
              <div class="builder-tab" onclick="showBuilderTab('layout')">📐 Layout</div>
              <div class="builder-tab" onclick="showBuilderTab('sleep')">😴 Sleep</div>
            </div>
            
            <!-- APPS TAB -->
            <div class="builder-panel active" id="panel-apps">
              <p class="text-muted mb-8">Select apps for your home screen:</p>
              
              <div class="plugin-category">
                <h4>Core Apps</h4>
                <div class="plugin-grid" id="plugins-core"></div>
              </div>
              
              <div class="plugin-category">
                <h4>Games</h4>
                <div class="plugin-grid" id="plugins-games"></div>
              </div>
              
              <div class="plugin-category">
                <h4>Tools</h4>
                <div class="plugin-grid" id="plugins-tools"></div>
              </div>
              
              <div class="plugin-category">
                <h4>Widgets</h4>
                <div class="plugin-grid" id="plugins-widgets"></div>
                <div id="wifiWarning" style="display:none;"></div>
              </div>
              
              <div class="plugin-category">
                <h4>System</h4>
                <div class="plugin-grid" id="plugins-system"></div>
              </div>
            </div>
            
            <!-- THEME TAB -->
            <div class="builder-panel" id="panel-theme">
              <div class="option-section">
                <h3>Color Theme</h3>
                <div class="style-options">
                  <div class="style-option selected" onclick="selectTheme(0)">
                    <div class="theme-preview light"></div>
                    <span>Light</span>
                  </div>
                  <div class="style-option" onclick="selectTheme(1)">
                    <div class="theme-preview dark"></div>
                    <span>Dark</span>
                  </div>
                  <div class="style-option" onclick="selectTheme(2)">
                    <div class="theme-preview contrast"></div>
                    <span>Contrast</span>
                  </div>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Icon Style</h3>
                <div class="style-options">
                  <div class="style-option selected" onclick="selectIconStyle('rounded')">
                    <div class="icon-preview rounded">A</div>
                    <span>Rounded</span>
                  </div>
                  <div class="style-option" onclick="selectIconStyle('square')">
                    <div class="icon-preview square">A</div>
                    <span>Square</span>
                  </div>
                  <div class="style-option" onclick="selectIconStyle('circle')">
                    <div class="icon-preview circle">A</div>
                    <span>Circle</span>
                  </div>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Font Size</h3>
                <div class="slider-row">
                  <label>Label Size:</label>
                  <input type="range" id="fontSizeSlider" min="10" max="18" value="12" oninput="updateFontSize(this.value)">
                  <span class="value" id="fontSizeVal">12px</span>
                </div>
              </div>
            </div>
            
            <!-- LAYOUT TAB -->
            <div class="builder-panel" id="panel-layout">
              <div class="option-section">
                <h3>Orientation</h3>
                <div class="style-options" style="grid-template-columns: repeat(2, 1fr);">
                  <div class="style-option" onclick="selectOrientation('landscape')">
                    <div class="orient-preview" style="width:40px;height:24px;"></div>
                    <span>Landscape</span>
                  </div>
                  <div class="style-option selected" onclick="selectOrientation('portrait')">
                    <div class="orient-preview" style="width:24px;height:40px;"></div>
                    <span>Portrait</span>
                  </div>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Status Bar</h3>
                <div class="toggle"><span class="toggle-label">Show Status Bar</span><div class="toggle-switch on" id="togStatusBar" onclick="toggleBuilder(this,'showStatusBar')"></div></div>
                <div class="toggle"><span class="toggle-label">Show Battery</span><div class="toggle-switch on" id="togBattery" onclick="toggleBuilder(this,'showBattery')"></div></div>
                <div class="toggle"><span class="toggle-label">Show Clock</span><div class="toggle-switch on" id="togClock" onclick="toggleBuilder(this,'showClock')"></div></div>
                <div class="toggle"><span class="toggle-label">Show WiFi</span><div class="toggle-switch" id="togWifi" onclick="toggleBuilder(this,'showWifi')"></div></div>
              </div>
            </div>
            
            <!-- SLEEP SCREEN TAB -->
            <div class="builder-panel" id="panel-sleep">
              <div class="option-section">
                <h3>Sleep Display</h3>
                <div class="style-options">
                  <div class="style-option" onclick="selectSleepStyle('book')">
                    <div style="font-size:18px;">📕</div>
                    <span>Book Cover</span>
                  </div>
                  <div class="style-option" onclick="selectSleepStyle('shuffle')">
                    <div style="font-size:18px;">🔀</div>
                    <span>Shuffle</span>
                  </div>
                  <div class="style-option selected" onclick="selectSleepStyle('off')">
                    <div style="font-size:18px;">💤</div>
                    <span>Wake Me</span>
                  </div>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Sleep Options</h3>
                <div class="toggle"><span class="toggle-label">Show Battery Warning</span><div class="toggle-switch on" id="togSleepBattery" onclick="toggleBuilder(this,'showBatterySleep')"></div></div>
              </div>
              
              <div class="option-section">
                <h3>Wake Button</h3>
                <select id="wakeButton" onchange="builderConfig.wakeButton=this.value" style="width:100%;">
                  <option value="any">Any Button</option>
                  <option value="select">Select Button Only</option>
                  <option value="power">Power Button Only</option>
                </select>
              </div>
            </div>
          </div>
          
          <!-- PREVIEW -->
          <div class="builder-preview">
            <div class="preview-header">
              <span>📱 Live Preview</span>
              <span class="preview-badge" id="previewBadge">Home</span>
            </div>
            <div class="preview-tabs">
              <div class="preview-tab active" onclick="setPreviewMode('home')">🏠 Home</div>
              <div class="preview-tab" onclick="setPreviewMode('sleep')">😴 Sleep</div>
            </div>
            <div class="device-container">
              <div class="device-outer portrait" id="deviceFrame">
                <div class="device-inner">
                  <div class="eink-screen light" id="einkScreen"></div>
                </div>
              </div>
            </div>
            <div style="text-align:center;margin-top:8px;">
              <button class="btn btn-sm" onclick="togglePreviewOrientation()">🔄 Rotate</button>
            </div>
            <div id="previewStats" style="margin-top:6px;text-align:center;font-size:9px;color:var(--mac-gray-dark)"></div>
          </div>
        </div>
      </div>

      <!-- ==================== WIFI PAGE ==================== -->
      <div class="page" id="page-wifi">
        <div class="page-header">
          <h1 class="page-title">WiFi Settings</h1>
          <p class="page-desc">Connect to your home network</p>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Current Connection</span>
          </div>
          <div class="card-body">
            <div id="wifiStatus">Checking...</div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Available Networks</span>
            <button class="btn btn-sm btn-secondary" onclick="scanWifi()" id="wifiScanBtn">🔍 Scan</button>
          </div>
          <div class="card-body" style="padding:0;">
            <div id="wifiList" class="list-view" style="min-height:100px;border:none;">
              <div style="padding:20px;text-align:center;color:var(--mac-gray-dark);">Click Scan to find networks</div>
            </div>
          </div>
        </div>
      </div>

      <!-- ==================== FILES PAGE ==================== -->
      <div class="page" id="page-files">
        <div class="page-header">
          <h1 class="page-title">File Manager</h1>
          <p class="page-desc">Manage your books, images, and more</p>
        </div>
        
        <div class="file-tabs">
          <div class="file-tab active" onclick="showFileTab('books')">📚 Books <span class="count" id="countBooks">0</span></div>
          <div class="file-tab" onclick="showFileTab('images')">🖼️ Images <span class="count" id="countImages">0</span></div>
          <div class="file-tab" onclick="showFileTab('maps')">🗺️ Maps <span class="count" id="countMaps">0</span></div>
          <div class="file-tab" onclick="showFileTab('flashcards')">🎴 Cards <span class="count" id="countFlashcards">0</span></div>
          <div class="file-tab" onclick="showFileTab('notes')">📝 Notes <span class="count" id="countNotes">0</span></div>
        </div>
        
        <!-- Books Panel -->
        <div class="file-panel active" id="panel-books">
          <div class="upload-zone" onclick="this.querySelector('input').click()" ondragover="event.preventDefault();this.classList.add('dragover')" ondragleave="this.classList.remove('dragover')" ondrop="handleDrop(event,'books')">
            <div class="icon">📚</div>
            <div class="text">Drop books here or click to upload</div>
            <div class="hint">Supports: EPUB, PDF, TXT</div>
            <input type="file" accept=".epub,.pdf,.txt" multiple onchange="uploadFiles(this.files,'books')">
          </div>
          <div class="card">
            <div class="card-header">
              <span class="card-title">Your Books</span>
              <button class="btn btn-sm btn-secondary" onclick="refreshFiles('books')">🔄</button>
            </div>
            <div class="card-body">
              <div class="file-grid" id="bookGrid">
                <div class="file-empty"><div class="icon">📚</div><div>No books yet</div></div>
              </div>
            </div>
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Reader Settings</span></div>
            <div class="card-body">
              <div style="display:grid;grid-template-columns:1fr 180px;gap:12px;">
                <div>
                  <div class="slider-row">
                    <span class="slider-label">Font Size</span>
                    <input type="range" class="slider-input" id="readerFontSize" min="12" max="32" value="18" oninput="updateReaderPreview()">
                    <span class="slider-value" id="readerFontVal">18px</span>
                  </div>
                  <div class="slider-row">
                    <span class="slider-label">Line Height</span>
                    <input type="range" class="slider-input" id="readerLineHeight" min="100" max="200" step="10" value="150" oninput="updateReaderPreview()">
                    <span class="slider-value" id="readerLineVal">150%</span>
                  </div>
                  <div class="slider-row">
                    <span class="slider-label">Margins</span>
                    <input type="range" class="slider-input" id="readerMargins" min="5" max="40" value="20" oninput="updateReaderPreview()">
                    <span class="slider-value" id="readerMarginVal">20px</span>
                  </div>
                  <div class="toggle">
                    <span class="toggle-label">Justify Text</span>
                    <div class="toggle-switch on" id="togJustify" onclick="toggleReaderOpt(this,'textAlign');updateReaderPreview()"></div>
                  </div>
                </div>
                <div>
                  <div style="font-size:9px;font-weight:bold;margin-bottom:4px;color:var(--mac-gray-dark);">Preview</div>
                  <div id="readerPreview" style="background:#f5f5f0;border:1px solid var(--mac-black);height:140px;overflow:hidden;padding:8px;font-family:Georgia,serif;">
                    <p style="margin:0;">It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness...</p>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Images Panel -->
        <div class="file-panel" id="panel-images">
          <div class="upload-zone" onclick="this.querySelector('input').click()" ondragover="event.preventDefault();this.classList.add('dragover')" ondragleave="this.classList.remove('dragover')" ondrop="handleDrop(event,'images')">
            <div class="icon">🖼️</div>
            <div class="text">Drop images here</div>
            <div class="hint">Supports: BMP (1-bit or 24-bit)</div>
            <input type="file" accept=".bmp" multiple onchange="uploadFiles(this.files,'images')">
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Your Images</span><button class="btn btn-sm btn-secondary" onclick="refreshFiles('images')">🔄</button></div>
            <div class="card-body">
              <div class="file-grid" id="imageGrid">
                <div class="file-empty"><div class="icon">🖼️</div><div>No images yet</div></div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Maps Panel -->
        <div class="file-panel" id="panel-maps">
          <div class="upload-zone" onclick="this.querySelector('input').click()" ondragover="event.preventDefault();this.classList.add('dragover')" ondragleave="this.classList.remove('dragover')" ondrop="handleDrop(event,'maps')">
            <div class="icon">🗺️</div>
            <div class="text">Drop map images here</div>
            <div class="hint">Supports: PNG, BMP, JPG</div>
            <input type="file" accept=".png,.bmp,.jpg,.jpeg" multiple onchange="uploadFiles(this.files,'maps')">
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Your Maps</span><button class="btn btn-sm btn-secondary" onclick="refreshFiles('maps')">🔄</button></div>
            <div class="card-body">
              <div class="file-grid" id="mapGrid">
                <div class="file-empty"><div class="icon">🗺️</div><div>No maps yet</div></div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Flashcards Panel -->
        <div class="file-panel" id="panel-flashcards">
          <div class="btn-group mb-8">
            <button class="btn btn-primary" onclick="showCreateDeckModal()">➕ Create Deck</button>
            <button class="btn btn-secondary" onclick="document.getElementById('ankiImport').click()">📥 Import</button>
            <input type="file" id="ankiImport" accept=".txt,.csv,.apkg" style="display:none" onchange="importAnkiDeck(this.files[0])">
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Your Decks</span><button class="btn btn-sm btn-secondary" onclick="refreshFiles('flashcards')">🔄</button></div>
            <div class="card-body">
              <div id="deckList">
                <div class="file-empty"><div class="icon">🎴</div><div>No decks yet</div></div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Notes Panel -->
        <div class="file-panel" id="panel-notes">
          <div class="btn-group mb-8">
            <button class="btn btn-primary" onclick="createNewNote()">📝 New Note</button>
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Your Notes</span><button class="btn btn-sm btn-secondary" onclick="refreshFiles('notes')">🔄</button></div>
            <div class="card-body">
              <div class="file-grid" id="noteGrid">
                <div class="file-empty"><div class="icon">📝</div><div>No notes yet</div></div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- ==================== WEATHER PAGE ==================== -->
      <div class="page" id="page-weather">
        <div class="page-header">
          <h1 class="page-title">Weather Settings</h1>
          <p class="page-desc">Configure weather display</p>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Location</span></div>
          <div class="card-body">
            <div id="weatherLocation" class="mb-8 text-muted">Auto-detected from IP</div>
            <div class="form-row">
              <label>ZIP Code:</label>
              <input type="text" class="text-input" id="weatherZip" placeholder="e.g. 80424" style="width:80px;">
              <button class="btn btn-sm" onclick="saveWeatherZip()">Set</button>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Timezone</span></div>
          <div class="card-body">
            <select id="timezoneSelect" style="width:100%;" onchange="saveTimezone()">
              <option value="auto">Auto-detect from IP</option>
              <option value="-18000">Eastern (UTC-5)</option>
              <option value="-21600">Central (UTC-6)</option>
              <option value="-25200">Mountain (UTC-7)</option>
              <option value="-28800">Pacific (UTC-8)</option>
              <option value="0">UTC</option>
            </select>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Units</span></div>
          <div class="card-body">
            <div class="toggle">
              <span class="toggle-label">Use Celsius (°C)</span>
              <div class="toggle-switch" id="togCelsius" onclick="toggleOpt(this,'useCelsius')"></div>
            </div>
          </div>
        </div>
      </div>

      <!-- ==================== DISPLAY PAGE ==================== -->
      <div class="page" id="page-display">
        <div class="page-header">
          <h1 class="page-title">Display Settings</h1>
          <p class="page-desc">Configure your e-ink display</p>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Display Options</span></div>
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Landscape Mode</span><div class="toggle-desc">Rotate display 90°</div></div>
              <div class="toggle-switch" id="togLandscape" onclick="toggleOpt(this,'landscape')"></div>
            </div>
            <div class="toggle">
              <div><span class="toggle-label">Invert Colors</span><div class="toggle-desc">Dark background, light text</div></div>
              <div class="toggle-switch" id="togInvert" onclick="toggleOpt(this,'invertColors')"></div>
            </div>
            <div class="toggle">
              <div><span class="toggle-label">Boot to Last Book</span><div class="toggle-desc">Resume reading on startup</div></div>
              <div class="toggle-switch" id="togBootToBook" onclick="toggleOpt(this,'bootToLastBook')"></div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Auto-Sleep</span></div>
          <div class="card-body">
            <div class="slider-row">
              <span class="slider-label">Sleep after:</span>
              <input type="range" class="slider-input" id="sleepSlider" min="5" max="60" step="5" value="15" oninput="updateSlider(this,'sleepVal',' min')">
              <span class="slider-value" id="sleepVal">15 min</span>
            </div>
          </div>
        </div>
      </div>

      <!-- ==================== BLUETOOTH PAGE ==================== -->
      <div class="page" id="page-bluetooth">
        <div class="page-header">
          <h1 class="page-title">Keyboard Settings</h1>
          <p class="page-desc">Connect Bluetooth keyboards and page turners</p>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Bluetooth</span></div>
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Enable Bluetooth</span><div class="toggle-desc">Allow BT connections</div></div>
              <div class="toggle-switch" id="togBTEnabled" onclick="toggleBT(this)"></div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Paired Devices</span></div>
          <div class="card-body" id="btPaired">
            <div class="text-center text-muted">No devices paired yet</div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Available Devices</span>
            <button class="btn btn-sm btn-secondary" id="btScanBtn" onclick="scanBT()">🔍 Scan</button>
          </div>
          <div class="card-body" id="btDiscovered">
            <div class="text-center text-muted">Click Scan to search</div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Keyboard Layout</span></div>
          <div class="card-body">
            <select id="kbLayout" style="width:100%;">
              <option>US English (QWERTY)</option>
              <option>UK English</option>
              <option>German (QWERTZ)</option>
              <option>French (AZERTY)</option>
            </select>
          </div>
        </div>
      </div>

      <!-- ==================== POWER PAGE ==================== -->
      <div class="page" id="page-power">
        <div class="page-header">
          <h1 class="page-title">Power</h1>
          <p class="page-desc">Battery status and power settings</p>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Battery Status</span></div>
          <div class="card-body text-center">
            <div style="font-size:48px;">🔋</div>
            <div style="font-size:28px;font-weight:bold;" id="powerPercent">--%</div>
            <div class="text-muted" id="powerVolts">-- V</div>
            <div class="progress" style="margin-top:12px;">
              <div class="progress-bar" id="powerBar" style="width:0%;"></div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Power Settings</span></div>
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Deep Sleep Mode</span><div class="toggle-desc">Lower power consumption when sleeping</div></div>
              <div class="toggle-switch on" id="togDeepSleep" onclick="toggleOpt(this,'deepSleep')"></div>
            </div>
          </div>
        </div>
      </div>

      <!-- ==================== ABOUT PAGE ==================== -->
      <div class="page" id="page-about">
        <div class="page-header">
          <h1 class="page-title">About</h1>
          <p class="page-desc">Device information and system tools</p>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Device Information</span></div>
          <div class="card-body">
            <table style="width:100%;font-size:11px;">
              <tr><td class="text-muted" style="padding:4px 0">Firmware</td><td style="text-align:right" id="aboutFirmware">--</td></tr>
              <tr><td class="text-muted" style="padding:4px 0">Hardware</td><td style="text-align:right">ESP32-C3</td></tr>
              <tr><td class="text-muted" style="padding:4px 0">Display</td><td style="text-align:right">GDEQ0426T82 4.26"</td></tr>
              <tr><td class="text-muted" style="padding:4px 0">Resolution</td><td style="text-align:right">800×480</td></tr>
              <tr><td class="text-muted" style="padding:4px 0">SD Card</td><td style="text-align:right" id="aboutSD">--</td></tr>
              <tr><td class="text-muted" style="padding:4px 0">Free Memory</td><td style="text-align:right" id="aboutMem">--</td></tr>
              <tr><td class="text-muted" style="padding:4px 0">Uptime</td><td style="text-align:right" id="aboutUptime">--</td></tr>
            </table>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">System Actions</span></div>
          <div class="card-body">
            <div class="btn-group">
              <button class="btn btn-secondary" onclick="refreshDisplay()">🔄 Refresh Display</button>
              <button class="btn btn-secondary" onclick="rebootDevice()">🔃 Reboot</button>
              <button class="btn btn-danger" onclick="factoryReset()">🗑️ Factory Reset</button>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Backup & Restore</span></div>
          <div class="card-body">
            <div class="btn-group">
              <button class="btn btn-secondary" onclick="downloadBackup()">📥 Download Backup</button>
              <button class="btn btn-secondary" onclick="document.getElementById('restoreFile').click()">📤 Restore</button>
              <input type="file" id="restoreFile" accept=".json" style="display:none" onchange="restoreBackup(this.files[0])">
            </div>
          </div>
        </div>
      </div>
    </main>
  </div>
  
  <!-- Action Bar (inside window) -->
  <div class="action-bar">
    <button class="btn btn-secondary" onclick="resetToDefaults()">↩️ Reset</button>
    <button class="btn btn-primary" id="deployBtn" onclick="saveAndDeploy()">💾 Save & Deploy</button>
  </div>
</div>

<!-- Toast -->
<div class="toast" id="toast"></div>

<!-- WiFi Modal -->
<div class="modal" id="wifiModal">
  <div class="modal-content">
    <div class="modal-header">Connect to WiFi</div>
    <div class="modal-body">
      <p class="mb-8">Network: <strong id="wifiModalSSID"></strong></p>
      <div class="input-row">
        <label>Password:</label>
        <input type="password" class="text-input" id="wifiPassword" style="flex:1;" onkeypress="if(event.key==='Enter')connectWifi()">
      </div>
    </div>
    <div class="modal-footer">
      <button class="btn btn-secondary" onclick="closeModal('wifiModal')">Cancel</button>
      <button class="btn btn-primary" onclick="connectWifi()">Connect</button>
    </div>
  </div>
</div>

<script>
// === app.js ===
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


</script>
</body>
</html>
)rawliteral";

#endif // PORTAL_HTML_H
