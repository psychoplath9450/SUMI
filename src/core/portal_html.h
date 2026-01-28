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

/* Spinner Animation */
@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.spinner {
  display: inline-block;
  width: 16px;
  height: 16px;
  border: 2px solid var(--mac-gray);
  border-top-color: var(--mac-highlight);
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

/* Book cover preview in grid */
.book-cover-preview {
  width: 100%;
  height: 80px;
  object-fit: contain;
  border-radius: 2px;
  background: var(--mac-gray-light);
}

.book-item-with-cover {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
}

.cover-status {
  font-size: 9px;
  padding: 2px 6px;
  border-radius: 4px;
  margin-top: 2px;
}

.cover-status.has-cover {
  background: #d4edda;
  color: #155724;
}

.cover-status.no-cover {
  background: #fff3cd;
  color: #856404;
}

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
  
  <!-- Connection Mode Banner -->
  <div id="connectionBanner" style="display: none;"></div>
  
  <div class="main">
    <nav class="sidebar" id="sidebar">
      <div class="nav-section">
        <div class="nav-section-title">Setup</div>
        <div class="nav-item" data-page="summary"><span class="nav-icon">📋</span>My SUMI</div>
        <div class="nav-item" data-page="builder"><span class="nav-icon">🎨</span>Customize</div>
        <div class="nav-item active" data-page="wifi"><span class="nav-icon">📶</span>WiFi</div>
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
      <!-- ==================== MY SUMI SUMMARY PAGE ==================== -->
      <div class="page" id="page-summary">
        <div class="page-header">
          <h1 class="page-title">My SUMI</h1>
          <p class="page-desc">Your device configuration at a glance</p>
        </div>
        
        <!-- Device Status -->
        <div class="card" style="margin-bottom: 16px;">
          <div class="card-header" style="background: linear-gradient(90deg, #e3f2fd 0%, #bbdefb 100%);">
            <span class="card-title">📟 Device Status</span>
          </div>
          <div class="card-body" id="summaryDevice">
            <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; text-align: center;">
              <div><div style="font-size: 24px;">🔋</div><div style="font-weight: 600;" id="sumBattery">--%</div><div style="font-size: 11px; color: #666;">Battery</div></div>
              <div><div style="font-size: 24px;">💾</div><div style="font-weight: 600;" id="sumStorage">--</div><div style="font-size: 11px; color: #666;">Storage</div></div>
              <div><div style="font-size: 24px;">🧠</div><div style="font-weight: 600;" id="sumMemory">--</div><div style="font-size: 11px; color: #666;">Free RAM</div></div>
              <div><div style="font-size: 24px;">⏱️</div><div style="font-weight: 600;" id="sumUptime">--</div><div style="font-size: 11px; color: #666;">Uptime</div></div>
            </div>
          </div>
        </div>
        
        <!-- Connection & Location -->
        <div class="card" style="margin-bottom: 16px;">
          <div class="card-header" style="background: linear-gradient(90deg, #e8f5e9 0%, #c8e6c9 100%);">
            <span class="card-title">🌐 Connection & Location</span>
          </div>
          <div class="card-body" id="summaryConnection">
            <table style="width: 100%; font-size: 12px;">
              <tr><td style="padding: 6px 0; color: #666;">WiFi Network</td><td style="text-align: right; font-weight: 500;" id="sumWifiSSID">Not connected</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">IP Address</td><td style="text-align: right; font-weight: 500;" id="sumWifiIP">--</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">Weather Location</td><td style="text-align: right; font-weight: 500;" id="sumLocation">Not set</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">Timezone</td><td style="text-align: right; font-weight: 500;" id="sumTimezone">--</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">Current Time</td><td style="text-align: right; font-weight: 500;" id="sumTime">--</td></tr>
            </table>
          </div>
        </div>
        
        <!-- SD Card Contents -->
        <div class="card" style="margin-bottom: 16px;">
          <div class="card-header" style="background: linear-gradient(90deg, #fff3e0 0%, #ffe0b2 100%);">
            <span class="card-title">💾 SD Card Contents</span>
          </div>
          <div class="card-body" id="summaryContent">
            <div style="display: grid; grid-template-columns: repeat(5, 1fr); gap: 8px; text-align: center; margin-bottom: 12px;">
              <div style="background: #f8f9fa; padding: 12px 8px; border-radius: 8px;">
                <div style="font-size: 20px;">📚</div>
                <div style="font-weight: 700; font-size: 18px;" id="sumBooks">0</div>
                <div style="font-size: 10px; color: #666;">Books</div>
              </div>
              <div style="background: #f8f9fa; padding: 12px 8px; border-radius: 8px;">
                <div style="font-size: 20px;">🖼️</div>
                <div style="font-weight: 700; font-size: 18px;" id="sumImages">0</div>
                <div style="font-size: 10px; color: #666;">Images</div>
              </div>
              <div style="background: #f8f9fa; padding: 12px 8px; border-radius: 8px;">
                <div style="font-size: 20px;">🗺️</div>
                <div style="font-weight: 700; font-size: 18px;" id="sumMaps">0</div>
                <div style="font-size: 10px; color: #666;">Maps</div>
              </div>
              <div style="background: #f8f9fa; padding: 12px 8px; border-radius: 8px;">
                <div style="font-size: 20px;">🎴</div>
                <div style="font-weight: 700; font-size: 18px;" id="sumCards">0</div>
                <div style="font-size: 10px; color: #666;">Flashcards</div>
              </div>
              <div style="background: #f8f9fa; padding: 12px 8px; border-radius: 8px;">
                <div style="font-size: 20px;">📝</div>
                <div style="font-weight: 700; font-size: 18px;" id="sumNotes">0</div>
                <div style="font-size: 10px; color: #666;">Notes</div>
              </div>
            </div>
            <div id="sumBooksStatus" style="background: #e8f5e9; border-radius: 6px; padding: 8px 12px; font-size: 11px; display: none;">
              <span style="color: #2e7d32;">✓ All books processed and ready</span>
            </div>
          </div>
        </div>
        
        <!-- Home Screen Customization -->
        <div class="card" style="margin-bottom: 16px;">
          <div class="card-header" style="background: linear-gradient(90deg, #f3e5f5 0%, #e1bee7 100%);">
            <span class="card-title">🎨 Home Screen</span>
          </div>
          <div class="card-body" id="summaryHomeScreen">
            <table style="width: 100%; font-size: 12px;">
              <tr><td style="padding: 6px 0; color: #666;">Theme</td><td style="text-align: right; font-weight: 500;" id="sumTheme">--</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">Icon Style</td><td style="text-align: right; font-weight: 500;" id="sumIconStyle">--</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">Orientation</td><td style="text-align: right; font-weight: 500;" id="sumOrientation">--</td></tr>
              <tr><td style="padding: 6px 0; color: #666;">Status Bar</td><td style="text-align: right; font-weight: 500;" id="sumStatusBar">--</td></tr>
            </table>
            <div style="margin-top: 12px; padding-top: 12px; border-top: 1px solid #eee;">
              <div style="font-size: 11px; color: #666; margin-bottom: 8px;">Enabled Apps:</div>
              <div id="sumApps" style="display: flex; flex-wrap: wrap; gap: 6px;"></div>
            </div>
          </div>
        </div>
        
        <!-- Display & Power Settings -->
        <div class="card" style="margin-bottom: 16px;">
          <div class="card-header" style="background: linear-gradient(90deg, #e0f7fa 0%, #b2ebf2 100%);">
            <span class="card-title">⚙️ Settings</span>
          </div>
          <div class="card-body" id="summarySettings">
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
              <div>
                <div style="font-size: 11px; color: #666; margin-bottom: 6px; font-weight: 600;">Display</div>
                <table style="width: 100%; font-size: 11px;">
                  <tr><td style="padding: 4px 0; color: #888;">Boot to Last Book</td><td style="text-align: right;" id="sumBootBook">--</td></tr>
                  <tr><td style="padding: 4px 0; color: #888;">Invert Colors</td><td style="text-align: right;" id="sumInvert">--</td></tr>
                  <tr><td style="padding: 4px 0; color: #888;">Sleep Timer</td><td style="text-align: right;" id="sumSleepTimer">--</td></tr>
                </table>
              </div>
              <div>
                <div style="font-size: 11px; color: #666; margin-bottom: 6px; font-weight: 600;">Reader</div>
                <table style="width: 100%; font-size: 11px;">
                  <tr><td style="padding: 4px 0; color: #888;">Font Size</td><td style="text-align: right;" id="sumFontSize">--</td></tr>
                  <tr><td style="padding: 4px 0; color: #888;">Line Height</td><td style="text-align: right;" id="sumLineHeight">--</td></tr>
                  <tr><td style="padding: 4px 0; color: #888;">Justify Text</td><td style="text-align: right;" id="sumJustify">--</td></tr>
                </table>
              </div>
            </div>
            <div style="margin-top: 12px; padding-top: 12px; border-top: 1px solid #eee; display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
              <div>
                <div style="font-size: 11px; color: #666; margin-bottom: 6px; font-weight: 600;">Sync</div>
                <table style="width: 100%; font-size: 11px;">
                  <tr><td style="padding: 4px 0; color: #888;">KOSync</td><td style="text-align: right;" id="sumKosync">--</td></tr>
                </table>
              </div>
              <div>
                <div style="font-size: 11px; color: #666; margin-bottom: 6px; font-weight: 600;">Keyboard</div>
                <table style="width: 100%; font-size: 11px;">
                  <tr><td style="padding: 4px 0; color: #888;">Bluetooth</td><td style="text-align: right;" id="sumBluetooth">--</td></tr>
                </table>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Last Updated -->
        <div style="text-align: center; font-size: 11px; color: #999; margin-top: 16px;">
          <span id="sumLastUpdated">Last updated: --</span>
          <button class="btn btn-sm btn-secondary" style="margin-left: 12px;" onclick="refreshSummary()">🔄 Refresh</button>
        </div>
      </div>

      <!-- ==================== OLD HOME PAGE (hidden, keep for stat IDs) ==================== -->
      <div class="page" id="page-home" style="display:none;">
        <div id="statBattery" style="display:none;">--%</div>
        <div id="statStorage" style="display:none;">--</div>
        <div id="statWifi" style="display:none;">--</div>
        <div id="statUptime" style="display:none;">--</div>
        <div id="infoFirmware" style="display:none;">--</div>
        <div id="infoIP" style="display:none;">--</div>
        <div id="infoMem" style="display:none;">--</div>
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
      <div class="page active" id="page-wifi">
        <div class="page-header">
          <h1 class="page-title">WiFi Settings</h1>
          <p class="page-desc">Connect to your home network</p>
        </div>
        
        <!-- Important info banner - emphasize book requirement -->
        <div class="card" style="background: linear-gradient(135deg, #fff3cd 0%, #ffeeba 100%); border: 2px solid #ffc107; margin-bottom: 12px;">
          <div class="card-body" style="padding: 14px;">
            <div style="display: flex; align-items: flex-start; gap: 12px;">
              <div style="font-size: 28px;">📶</div>
              <div style="flex: 1;">
                <div style="font-weight: 700; font-size: 14px; color: #856404; margin-bottom: 8px;">Home WiFi Required for Books</div>
                <div style="font-size: 12px; color: #856404; line-height: 1.5; margin-bottom: 10px;">
                  SUMI processes EPUB files in your browser, which requires internet access. Without home WiFi, you can only play games and use offline features.
                </div>
                <div style="font-size: 11px; color: #666; line-height: 1.5; padding: 10px; background: rgba(255,255,255,0.5); border-radius: 6px;">
                  <strong>With WiFi:</strong> Upload & read books • Weather widget • Time sync • KOSync reading progress<br>
                  <strong>Without WiFi:</strong> Games only (Sudoku, Chess, Minesweeper, etc.)
                </div>
              </div>
            </div>
          </div>
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
          <!-- HOTSPOT MODE WARNING - Book processing unavailable -->
          <div id="hotspotBookWarning" style="display: none;">
            <div class="card" style="background: linear-gradient(135deg, #f8d7da 0%, #f5c6cb 100%); border: 2px solid #dc3545; margin-bottom: 12px;">
              <div class="card-body" style="padding: 14px;">
                <div style="display: flex; align-items: flex-start; gap: 12px;">
                  <div style="font-size: 28px;">⚠️</div>
                  <div style="flex: 1;">
                    <div style="font-weight: 700; font-size: 14px; color: #721c24; margin-bottom: 6px;">Home WiFi Required for Book Processing</div>
                    <div style="font-size: 12px; color: #721c24; line-height: 1.5;">
                      You're connected via hotspot mode. To upload and process books, SUMI needs to be connected to your home WiFi network. 
                      <strong>Games and offline features still work!</strong>
                    </div>
                    <a href="#" onclick="showPage('wifi'); return false;" style="display: inline-block; margin-top: 10px; background: #dc3545; color: white; padding: 8px 16px; border-radius: 6px; text-decoration: none; font-size: 12px; font-weight: 600;">Setup Home WiFi →</a>
                  </div>
                </div>
              </div>
            </div>
          </div>
          
          <!-- Cover Processing Info Card -->
          <div id="bookProcessingInfo" class="card" style="background: linear-gradient(135deg, #e8f4fd 0%, #f0e6ff 100%); border: 1px solid #b8d4e8; margin-bottom: 12px;">
            <div class="card-body" style="padding: 12px;">
              <div style="display: flex; align-items: flex-start; gap: 10px;">
                <div style="font-size: 24px;">🚀</div>
                <div style="flex: 1;">
                  <div style="font-weight: 600; font-size: 13px; margin-bottom: 4px;">Smart Book Processing</div>
                  <div style="font-size: 11px; color: #555; line-height: 1.4;">
                    When you upload EPUBs, your browser pre-processes everything: extracts chapters as plain text, 
                    optimizes cover art, and creates a ready-to-read cache. SUMI loads books instantly with zero processing!
                  </div>
                </div>
              </div>
            </div>
          </div>
          
          <!-- Dynamic warning for unprocessed books (populated by JS) -->
          <div id="coverWarning"></div>
          
          <!-- Lightbulb tip for processing (populated by JS when on home network) -->
          <div id="processingTip" style="display: none;"></div>
          
          <div class="upload-zone" id="bookUploadZone" onclick="this.querySelector('input').click()" ondragover="event.preventDefault();this.classList.add('dragover')" ondragleave="this.classList.remove('dragover')" ondrop="handleBookDrop(event)">
            <div class="icon">📚</div>
            <div class="text">Drop books here or click to upload</div>
            <div class="hint">Supports: EPUB (with automatic cover extraction)</div>
            <input type="file" accept=".epub,.pdf,.txt" multiple onchange="uploadBooks(this.files)">
          </div>
          
          <!-- Upload Progress (hidden by default) -->
          <div id="uploadProgress" style="display: none; margin-bottom: 12px;">
            <div class="card">
              <div class="card-body" style="padding: 12px;">
                <div style="display: flex; align-items: center; gap: 10px; margin-bottom: 8px;">
                  <div class="spinner" style="width: 20px; height: 20px; border: 2px solid #ddd; border-top-color: #007aff; border-radius: 50%; animation: spin 1s linear infinite;"></div>
                  <div id="uploadStatusText" style="font-size: 12px; font-weight: 500;">Processing...</div>
                </div>
                <div class="progress" style="height: 6px; margin-bottom: 6px;">
                  <div class="progress-bar" id="uploadProgressBar" style="width: 0%; transition: width 0.3s;"></div>
                </div>
                <div id="uploadDetailText" style="font-size: 10px; color: #888;"></div>
              </div>
            </div>
          </div>
          
          <div class="card">
            <div class="card-header">
              <span class="card-title">Your Books</span>
              <div style="display:flex;gap:4px;">
                <button class="btn btn-sm btn-secondary" onclick="reprocessAllBooks()" title="Reprocess All Books">♻️</button>
                <button class="btn btn-sm btn-danger" onclick="clearAllCache()" title="Clear All Cache">🗑️</button>
                <button class="btn btn-sm btn-secondary" onclick="refreshFiles('books')">🔄</button>
              </div>
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
          <!-- Info box -->
          <div style="background: #e8f4fd; border: 1px solid #bee5eb; border-radius: 8px; padding: 12px 14px; margin-bottom: 12px;">
            <div style="display: flex; align-items: flex-start; gap: 10px;">
              <div style="font-size: 16px;">💡</div>
              <div style="flex: 1; font-size: 11px; color: #0c5460; line-height: 1.5;">
                <strong>Image Tips:</strong><br>
                • <strong>Format:</strong> BMP files (1-bit monochrome or 24-bit color)<br>
                • <strong>Processing:</strong> Color images auto-convert to grayscale with dithering<br>
                • <strong>Size:</strong> Images are scaled to fit the 800×480 e-ink display<br>
                • <strong>Sleep Screen:</strong> Enable "Shuffle Images" in Settings to use as a rotating sleep screen slideshow
              </div>
            </div>
          </div>
          
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
          
          <!-- Info box -->
          <div style="background: #e8f4fd; border: 1px solid #bee5eb; border-radius: 8px; padding: 12px 14px; margin-bottom: 12px;">
            <div style="display: flex; align-items: flex-start; gap: 10px;">
              <div style="font-size: 16px;">💡</div>
              <div style="flex: 1; font-size: 11px; color: #0c5460; line-height: 1.5;">
                <strong>Supported Formats:</strong><br>
                • <strong>CSV/TSV (recommended):</strong> <code>question,answer</code> or <code>term⇥definition</code> — works with Quizlet & Anki exports<br>
                • <strong>TXT:</strong> Alternating lines (question on one line, answer on next) or <code>question;answer</code> per line<br>
                • <strong>JSON:</strong> Array of objects with field pairs: front/back, question/answer, term/definition, word/meaning, or kanji/reading
              </div>
            </div>
          </div>
          
          <div class="card">
            <div class="card-header"><span class="card-title">Your Decks</span><button class="btn btn-sm btn-secondary" onclick="refreshFiles('flashcards')">🔄</button></div>
            <div class="card-body">
              <div id="deckList">
                <div class="file-empty"><div class="icon">🎴</div><div>No decks yet</div></div>
              </div>
            </div>
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Flashcard Settings</span></div>
            <div class="card-body">
              <div style="display:grid;grid-template-columns:1fr 180px;gap:12px;">
                <div>
                  <div class="slider-row">
                    <span class="slider-label">Font Size</span>
                    <select id="fcFontSize" onchange="updateFlashcardPreview();saveFlashcardSettings()">
                      <option value="0">Small</option>
                      <option value="1" selected>Medium</option>
                      <option value="2">Large</option>
                      <option value="3">Extra Large</option>
                    </select>
                  </div>
                  <div class="toggle">
                    <span class="toggle-label">Center Text</span>
                    <div class="toggle-switch on" id="togFcCenter" onclick="toggleSwitchSimple(this);updateFlashcardPreview();saveFlashcardSettings()"></div>
                  </div>
                  <div class="toggle" style="margin-top:8px;">
                    <span class="toggle-label">Shuffle Cards</span>
                    <div class="toggle-switch on" id="togFcShuffle" onclick="toggleSwitchSimple(this);saveFlashcardSettings()"></div>
                  </div>
                  <div class="toggle" style="margin-top:8px;">
                    <span class="toggle-label">Show Progress Bar</span>
                    <div class="toggle-switch on" id="togFcProgress" onclick="toggleSwitchSimple(this);updateFlashcardPreview();saveFlashcardSettings()"></div>
                  </div>
                  <div class="toggle" style="margin-top:8px;">
                    <span class="toggle-label">Show Stats</span>
                    <div class="toggle-switch on" id="togFcStats" onclick="toggleSwitchSimple(this);updateFlashcardPreview();saveFlashcardSettings()"></div>
                  </div>
                  <div class="toggle" style="margin-top:8px;">
                    <span class="toggle-label">Auto-advance</span>
                    <div class="toggle-switch" id="togFcAutoFlip" onclick="toggleSwitchSimple(this);saveFlashcardSettings()"></div>
                  </div>
                </div>
                <div>
                  <div style="font-size:9px;font-weight:bold;margin-bottom:4px;color:var(--mac-gray-dark);">Preview</div>
                  <div id="flashcardPreview" style="background:#f5f5f0;border:1px solid var(--mac-black);height:200px;overflow:hidden;font-family:sans-serif;position:relative;">
                    <!-- Progress bar -->
                    <div id="fcPreviewProgress" style="height:6px;background:#ddd;margin:6px 8px;">
                      <div style="width:40%;height:100%;background:#333;"></div>
                    </div>
                    <div style="font-size:8px;padding:0 8px;color:#666;">Card 2 of 5</div>
                    <div style="border-top:1px solid #333;margin:4px 0;"></div>
                    <!-- Question area -->
                    <div id="fcPreviewQuestion" style="padding:4px 8px;">
                      <div style="font-size:8px;color:#666;margin-bottom:2px;">Question:</div>
                      <div id="fcPreviewQText" style="font-size:14px;font-weight:bold;">What is 2 + 2?</div>
                    </div>
                    <div style="border-top:1px solid #333;margin:4px 8px;"></div>
                    <!-- Answer area -->
                    <div id="fcPreviewAnswer" style="padding:4px 8px;">
                      <div style="font-size:8px;color:#666;margin-bottom:2px;">Answer:</div>
                      <div id="fcPreviewAText" style="font-size:14px;font-weight:bold;">4</div>
                    </div>
                    <!-- Footer -->
                    <div style="position:absolute;bottom:0;left:0;right:0;border-top:1px solid #333;padding:4px 8px;background:#f5f5f0;display:flex;justify-content:space-between;font-size:8px;">
                      <span id="fcPreviewStats">2/0</span>
                      <span>&lt;Wrong  Right&gt;</span>
                    </div>
                  </div>
                </div>
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
      
      // Reader settings
      document.getElementById('sumFontSize').textContent = (settings.reader?.fontSize || 18) + 'px';
      document.getElementById('sumLineHeight').textContent = (settings.reader?.lineHeight || 150) + '%';
      document.getElementById('sumJustify').textContent = settings.reader?.justify ? '✓ On' : '✗ Off';
      
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
      },
      // Reader settings
      reader: {
        fontSize: parseInt(document.getElementById('readerFontSize')?.value) || 18,
        lineHeight: parseInt(document.getElementById('readerLineHeight')?.value) || 150,
        margins: parseInt(document.getElementById('readerMargins')?.value) || 20,
        textAlign: document.getElementById('togJustify')?.classList.contains('on') ? 1 : 0
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
    // Immediately update the global banner to show connection status
    loadStatus();
    checkWifiWarnings();
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

// Clear all preprocessed cache (/.sumi folder)
async function clearAllCache() {
  if (!confirm('Delete all preprocessed book cache?\\n\\nThis will require reprocessing all books.')) return;
  
  toast('Clearing cache...', 'info');
  
  try {
    // Delete entire .sumi folder
    await api('/api/files/delete', 'POST', {path: '/.sumi', recursive: true});
    
    // Recreate base structure
    await api('/api/files/mkdir', 'POST', {path: '/.sumi'});
    await api('/api/files/mkdir', 'POST', {path: '/.sumi/books'});
    
    toast('Cache cleared! Reprocess books to use them.', 'success');
    refreshFiles('books');
  } catch(e) {
    toast('Clear cache failed: ' + e.message, 'error');
  }
}

// Reprocess all books
async function reprocessAllBooks() {
  if (!confirm('Reprocess all books?\\n\\nThis may take a while.')) return;
  
  // Get list of EPUB files
  const epubs = fileCache.books.filter(f => f.name.toLowerCase().endsWith('.epub'));
  if (epubs.length === 0) {
    toast('No EPUB files to process', 'info');
    return;
  }
  
  toast(`Reprocessing ${epubs.length} books...`, 'info');
  
  // First clear existing cache
  try {
    await api('/api/files/delete', 'POST', {path: '/.sumi/books', recursive: true});
    await api('/api/files/mkdir', 'POST', {path: '/.sumi/books'});
  } catch(e) {
    // Ignore errors if folder doesn't exist
  }
  
  let processed = 0;
  let failed = 0;
  
  for (const epub of epubs) {
    try {
      // Download the EPUB file
      const response = await fetch('/api/download?path=/books/' + encodeURIComponent(epub.name));
      if (!response.ok) throw new Error('Download failed');
      const blob = await response.blob();
      const file = new File([blob], epub.name, {type: 'application/epub+zip'});
      
      // Process it
      await processEpubFile(file, (msg, pct) => {
        // Update progress silently
      });
      processed++;
      toast(`Processed ${processed}/${epubs.length}: ${epub.name}`, 'success');
    } catch(e) {
      failed++;
      console.error('Failed to process:', epub.name, e);
    }
  }
  
  toast(`Done! ${processed} processed, ${failed} failed.`, processed > 0 ? 'success' : 'error');
  refreshFiles('books');
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
    
    if (chapterFile) {
      try {
        let html = await chapterFile.async('string');
        
        // Extract inline images BEFORE converting to plain text
        // This uploads images and returns markers to insert
        const images = await extractInlineImages(zip, html, opfDir, cacheDir, imageIndex);
        
        // Replace <img> tags with markers
        for (const img of images) {
          html = html.replace(img.original, img.marker);
        }
        
        // Convert HTML to plain text with rich markers
        let text = htmlToPlainText(html);
        
        // Apply smart typography (quotes, em-dashes, ellipsis)
        text = applySmartTypography(text);
        
        // Add soft hyphens for better line breaking
        text = addSoftHyphens(text, language);
        
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
  
  onProgress('Updating library...', 0.99);
  await updateLibraryIndex();
  
  onProgress('Done!', 1);
  
  // Return the metadata for display
  return meta;
}

// Build master library index from all processed books
async function updateLibraryIndex() {
  try {
    // List all processed book directories
    const response = await fetch('/api/files/list?path=/.sumi/books');
    if (!response.ok) return;
    
    const data = await response.json();
    if (!data.files) return;
    
    const books = [];
    
    for (const dir of data.files) {
      if (!dir.isDir) continue;
      
      // Try to load meta.json for this book
      try {
        const metaResp = await fetch(`/api/download?path=/.sumi/books/${dir.name}/meta.json`);
        if (!metaResp.ok) continue;
        
        const meta = await metaResp.json();
        
        // Check if chapters exist (at least ch_000.txt)
        const ch0Resp = await fetch(`/api/files/exists?path=/.sumi/books/${dir.name}/ch_000.txt`);
        const ch0Data = await ch0Resp.json();
        if (!ch0Data.exists) continue;
        
        // Check for cover
        const thumbResp = await fetch(`/api/files/exists?path=/.sumi/books/${dir.name}/cover_thumb.jpg`);
        const thumbData = await thumbResp.json();
        
        books.push({
          hash: dir.name,
          filename: meta.filename || '',
          title: meta.title || 'Unknown',
          author: meta.author || '',
          totalChapters: meta.totalChapters || 0,
          totalWords: meta.totalWords || 0,
          estimatedPages: meta.estimatedPages || 0,
          pubYear: meta.pubYear || 0,
          hasCover: thumbData.exists || false,
          coverPath: thumbData.exists ? `/.sumi/books/${dir.name}/cover_thumb.jpg` : ''
        });
      } catch (e) {
        console.log('Skip book dir:', dir.name, e);
      }
    }
    
    // Sort by title
    books.sort((a, b) => a.title.localeCompare(b.title));
    
    // Save library index
    const index = {
      version: 1,
      timestamp: Date.now(),
      bookCount: books.length,
      books: books
    };
    
    await uploadTextFile('/.sumi/library.json', JSON.stringify(index));
    console.log('[PORTAL] Library index updated:', books.length, 'books');
    
  } catch (e) {
    console.error('Failed to update library index:', e);
  }
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
  // Smart quotes (straight to curly)
  // Opening double quotes: after space, newline, or start
  text = text.replace(/(^|[\s\n])"/g, "$1\u201C");
  // Closing double quotes: before space, newline, punctuation, or end
  text = text.replace(/"([\s\n.,;:!?\)]|$)/g, "\u201D$1");
  // Remaining double quotes (likely closing)
  text = text.replace(/"/g, "\u201D");
  
  // Single quotes / apostrophes
  // Opening: after space or newline
  text = text.replace(/(^|[\s\n])'/g, "$1\u2018");
  // Apostrophes in contractions (don't, won't, etc)
  text = text.replace(/(\w)'(\w)/g, "$1\u2019$2");
  // Remaining single quotes (likely closing)
  text = text.replace(/'/g, "\u2019");
  
  // Em-dashes
  text = text.replace(/--/g, "\u2014");
  text = text.replace(/ - /g, " \u2014 ");
  
  // Ellipsis
  text = text.replace(/\.\.\./g, "\u2026");
  
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
async function extractInlineImages(zip, html, opfDir, cacheDir, imageIndex) {
  const images = [];
  const imgRegex = /<img[^>]+src=["']([^"']+)["'][^>]*>/gi;
  let match;
  
  // Target size for e-ink display (480x800 with margins)
  // Leave room for status bar and comfortable margins
  const MAX_IMG_WIDTH = 420;
  const MAX_IMG_HEIGHT = 680;
  const MIN_IMG_SIZE = 80;  // Skip icons/spacers smaller than this
  
  while ((match = imgRegex.exec(html)) !== null) {
    const src = match[1];
    
    // Skip data: URLs and tiny embedded images
    if (src.startsWith('data:')) continue;
    
    const imgPath = resolveHref(decodeURIComponent(src), opfDir);
    const imgFile = zip.file(imgPath);
    
    if (imgFile) {
      try {
        const imgData = await imgFile.async('blob');
        
        // Check original dimensions first
        const origDims = await getImageDimensions(imgData);
        
        // Skip tiny images (icons, spacers, decorations)
        if (origDims.width < MIN_IMG_SIZE && origDims.height < MIN_IMG_SIZE) {
          console.log('[PORTAL] Skipping tiny image:', imgPath, origDims);
          continue;
        }
        
        // Resize image for e-ink display
        const resized = await processImage(imgData, MAX_IMG_WIDTH, MAX_IMG_HEIGHT, 0.8);
        
        if (resized && resized.size > 100) {
          const imgFilename = `img_${String(imageIndex.count).padStart(3, '0')}.jpg`;
          imageIndex.count++;
          
          // Upload image to cache directory
          await uploadBlobFile(`${cacheDir}/images/${imgFilename}`, resized);
          
          // Get resized dimensions for marker
          const dims = await getImageDimensions(resized);
          
          images.push({
            original: match[0],
            marker: `[!IMG:${imgFilename},w=${dims.width},h=${dims.height}]`,
            filename: imgFilename
          });
          
          console.log(`[PORTAL] Extracted image: ${imgFilename} (${dims.width}x${dims.height})`);
        }
      } catch (e) {
        console.warn('[PORTAL] Failed to extract image:', imgPath, e);
      }
    }
  }
  
  return images;
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
  text = text.replace(/<h[1-6][^>]*>([\s\S]*?)<\/h[1-6]>/gi, (m, content) => {
    // Strip nested tags but preserve word boundaries
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    return '\n\n# ' + clean + '\n\n';
  });
  
  // Bold: **text** (add space after to prevent concatenation)
  text = text.replace(/<(b|strong)[^>]*>([\s\S]*?)<\/\1>/gi, (m, tag, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    return '**' + clean + '** ';
  });
  
  // Italic: *text* (add space after to prevent concatenation)
  text = text.replace(/<(i|em)[^>]*>([\s\S]*?)<\/\1>/gi, (m, tag, content) => {
    const clean = content.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    return '*' + clean + '* ';
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

async function uploadTextFile(path, content) {
  const blob = new Blob([content], { type: 'text/plain' });
  const filename = path.split('/').pop();
  const dir = path.substring(0, path.lastIndexOf('/'));
  
  console.log(`[UPLOAD] Text file: dir='${dir}' filename='${filename}'`);
  
  const formData = new FormData();
  // IMPORTANT: path must come BEFORE file for multipart parsing
  formData.append('path', dir);
  formData.append('file', new File([blob], filename, { type: 'text/plain' }));
  
  const resp = await fetch('/api/files/upload', { method: 'POST', body: formData });
  if (!resp.ok) {
    console.error('[UPLOAD] Failed:', resp.status);
  }
}

async function uploadBlobFile(path, blob) {
  const filename = path.split('/').pop();
  const dir = path.substring(0, path.lastIndexOf('/'));
  
  console.log(`[UPLOAD] Blob file: dir='${dir}' filename='${filename}'`);
  
  const formData = new FormData();
  // IMPORTANT: path must come BEFORE file for multipart parsing
  formData.append('path', dir);
  formData.append('file', new File([blob], filename, { type: blob.type }));
  
  const resp = await fetch('/api/files/upload', { method: 'POST', body: formData });
  if (!resp.ok) {
    console.error('[UPLOAD] Failed:', resp.status);
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
    
    const books = data.unprocessed;
    
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
    
    // Update master library index once after all books
    if (completed > 0) {
      toast('Updating library index...', 'info');
      await updateLibraryIndex();
    }
    
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
  
  // Extract TOC
  onProgress('Extracting TOC...', 0.08);
  await new Promise(r => setTimeout(r, 0));
  const toc = await extractToc(zip, opfContent, opfDir, manifest);
  
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
  
  // Extract chapters with detailed progress
  const chapters = [];
  let totalChars = 0;
  let totalWords = 0;
  
  for (let i = 0; i < spine.length; i++) {
    const idref = spine[i];
    const item = manifest[idref];
    if (!item) continue;
    
    const progress = 0.15 + (i / spine.length) * 0.55;
    onProgress(`Extract ${i+1}/${spine.length}`, progress);
    await new Promise(r => setTimeout(r, 0)); // Let UI update every chapter
    
    const chapterPath = resolveHref(item.href, opfDir);
    const chapterFile = zip.file(chapterPath);
    
    if (chapterFile) {
      try {
        const html = await chapterFile.async('string');
        const text = htmlToPlainText(html);
        
        if (text.trim().length > 0) {
          const charCount = text.length;
          const wordCount = text.split(/\s+/).filter(w => w.length > 0).length;
          totalChars += charCount;
          totalWords += wordCount;
          
          chapters.push({
            index: chapters.length,
            idref: idref,
            href: item.href,  // For TOC matching
            text: text,
            chars: charCount,
            words: wordCount
          });
        }
      } catch (e) {}
    }
  }
  
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
  
  // Map TOC entries to chapter indices and save
  if (toc.length > 0) {
    const tocMapped = toc.map((entry, i) => {
      let chapterIndex = i;
      // Try to find matching chapter by href
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
    await uploadTextFile(cacheDir + '/toc.json', JSON.stringify(tocMapped, null, 2));
    console.log('[PORTAL] Saved TOC with', tocMapped.length, 'entries');
  }
  
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
  if (data.reader) {
    const r = data.reader;
    const fsEl = document.getElementById('readerFontSize');
    const lhEl = document.getElementById('readerLineHeight');
    const mgEl = document.getElementById('readerMargins');
    const justifyEl = document.getElementById('togJustify');
    if (fsEl) { fsEl.value = r.fontSize || 18; updateSlider(fsEl, 'readerFontVal', 'px'); }
    if (lhEl) { lhEl.value = r.lineHeight || 150; updateSlider(lhEl, 'readerLineVal', '%'); }
    if (mgEl) { mgEl.value = r.margins || 20; updateSlider(mgEl, 'readerMarginVal', 'px'); }
    if (justifyEl) { justifyEl.classList.toggle('on', r.textAlign === 1 || r.textAlign === undefined); }
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
  await loadStatus();  // This will update the connection banner properly
  await applyFeatureFlags();
  await loadSettings();
  renderPlugins();
  updateReaderPreview();
  
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
  
  setInterval(loadStatus, 5000);
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


</script>
</body>
</html>
)rawliteral";

#endif // PORTAL_HTML_H
