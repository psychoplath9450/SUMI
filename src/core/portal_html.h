/**
 * @file portal_html.h
 * @brief Embedded web portal for Sumi - Auto-generated
 * @version 2.1.30
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
:root {
  --primary: #6366f1;
  --primary-hover: #4f46e5;
  --primary-light: rgba(99,102,241,0.1);
  --success: #22c55e;
  --warning: #f59e0b;
  --danger: #ef4444;
  --bg: #09090b;
  --surface: #18181b;
  --surface-2: #27272a;
  --border: #3f3f46;
  --text: #fafafa;
  --text-muted: #a1a1aa;
  --radius: 12px;
  --radius-sm: 8px;
}

* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; }
body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
  background: var(--bg);
  color: var(--text);
  font-size: 14px;
  line-height: 1.6;
}

/* Layout */
.app { display: flex; flex-direction: column; min-height: 100vh; }

/* Header */
.header {
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  padding: 16px 24px;
  display: flex;
  align-items: center;
  gap: 16px;
  position: sticky;
  top: 0;
  z-index: 100;
  backdrop-filter: blur(10px);
}
.logo {
  font-size: 1.5rem;
  font-weight: 700;
  letter-spacing: 2px;
  background: linear-gradient(135deg, #fff 0%, var(--primary) 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}
.header-status {
  margin-left: auto;
  display: flex;
  align-items: center;
  gap: 20px;
  font-size: 13px;
  color: var(--text-muted);
}
.status-badge {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 12px;
  background: var(--surface-2);
  border-radius: 20px;
}
.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--success);
  animation: pulse 2s infinite;
}
@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
.menu-btn {
  display: none;
  background: none;
  border: none;
  color: var(--text);
  font-size: 24px;
  cursor: pointer;
  padding: 4px;
}

/* Main Layout */
.main { display: flex; flex: 1; overflow: hidden; }

/* Sidebar */
.sidebar {
  width: 240px;
  background: var(--surface);
  border-right: 1px solid var(--border);
  overflow-y: auto;
  flex-shrink: 0;
  padding: 12px;
}
.nav-section {
  margin-bottom: 8px;
}
.nav-section-title {
  font-size: 11px;
  font-weight: 600;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.5px;
  padding: 12px 12px 8px;
}
.nav-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  border-radius: var(--radius-sm);
  cursor: pointer;
  transition: all 0.15s;
  font-size: 14px;
  color: var(--text-muted);
  margin-bottom: 2px;
}
.nav-item:hover {
  background: var(--surface-2);
  color: var(--text);
}
.nav-item.active {
  background: var(--primary-light);
  color: var(--primary);
  font-weight: 500;
}
.nav-icon { font-size: 18px; width: 24px; text-align: center; }

/* Content */
.content {
  flex: 1;
  overflow-y: auto;
  padding: 32px;
  padding-bottom: 120px;
}

/* Page */
.page { display: none; max-width: 1200px; margin: 0 auto; }
.page.active { display: block; }
.page-header { margin-bottom: 32px; }
.page-title {
  font-size: 1.75rem;
  font-weight: 600;
  margin-bottom: 8px;
}
.page-desc { color: var(--text-muted); font-size: 15px; }

/* Cards */
.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  margin-bottom: 24px;
  overflow: hidden;
}
.card-header {
  padding: 16px 20px;
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.card-title {
  font-size: 14px;
  font-weight: 600;
  color: var(--text);
}
.card-body { padding: 20px; }

/* Stats Grid */
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  gap: 16px;
  margin-bottom: 32px;
}
.stat-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 24px;
  text-align: center;
  transition: all 0.2s;
}
.stat-card:hover {
  border-color: var(--primary);
  transform: translateY(-2px);
}
.stat-icon { font-size: 32px; margin-bottom: 12px; }
.stat-value { font-size: 28px; font-weight: 700; }
.stat-label {
  font-size: 12px;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.5px;
  margin-top: 6px;
}

/* Buttons */
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  padding: 12px 20px;
  border: none;
  border-radius: var(--radius-sm);
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.15s;
}
.btn-primary {
  background: var(--primary);
  color: white;
}
.btn-primary:hover {
  background: var(--primary-hover);
  transform: translateY(-1px);
}
.btn-primary:disabled {
  opacity: 0.6;
  cursor: not-allowed;
  transform: none;
}
.btn-secondary {
  background: var(--surface-2);
  color: var(--text);
  border: 1px solid var(--border);
}
.btn-secondary:hover { background: var(--border); }
.btn-danger { background: var(--danger); color: white; }
.btn-sm { padding: 8px 14px; font-size: 13px; }
.btn-block { width: 100%; }
.btn-group { display: flex; gap: 12px; flex-wrap: wrap; }

/* Toggle Switch */
.toggle {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 16px 0;
  border-bottom: 1px solid var(--border);
}
.toggle:last-child { border-bottom: none; }
.toggle-label { font-size: 14px; }
.toggle-desc { font-size: 12px; color: var(--text-muted); margin-top: 2px; }
.toggle-switch {
  width: 48px;
  height: 28px;
  background: var(--border);
  border-radius: 14px;
  position: relative;
  cursor: pointer;
  transition: background 0.2s;
  flex-shrink: 0;
}
.toggle-switch.on { background: var(--primary); }
.toggle-switch::after {
  content: '';
  position: absolute;
  width: 24px;
  height: 24px;
  background: white;
  border-radius: 50%;
  top: 2px;
  left: 2px;
  transition: transform 0.2s;
  box-shadow: 0 2px 4px rgba(0,0,0,0.2);
}
.toggle-switch.on::after { transform: translateX(20px); }

/* Forms */
.form-group { margin-bottom: 20px; }
.form-label {
  display: block;
  font-size: 13px;
  font-weight: 500;
  color: var(--text-muted);
  margin-bottom: 8px;
}
.form-input {
  width: 100%;
  padding: 12px 16px;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  color: var(--text);
  font-size: 14px;
  transition: all 0.15s;
}
.form-input:focus {
  outline: none;
  border-color: var(--primary);
  box-shadow: 0 0 0 3px var(--primary-light);
}
.select-wrap { position: relative; }
.select-wrap select {
  width: 100%;
  padding: 12px 16px;
  padding-right: 40px;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  color: var(--text);
  font-size: 14px;
  appearance: none;
  cursor: pointer;
}
.select-wrap::after {
  content: '▾';
  position: absolute;
  right: 16px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--text-muted);
  pointer-events: none;
}

/* Slider */
.slider-row {
  display: flex;
  align-items: center;
  gap: 16px;
  padding: 12px 0;
}
.slider-label {
  min-width: 140px;
  font-size: 14px;
  color: var(--text-muted);
}
.slider-input {
  flex: 1;
  -webkit-appearance: none;
  height: 6px;
  background: var(--border);
  border-radius: 3px;
}
.slider-input::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 20px;
  height: 20px;
  background: var(--primary);
  border-radius: 50%;
  cursor: pointer;
  box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}
.slider-value {
  min-width: 60px;
  text-align: right;
  font-weight: 600;
  font-size: 14px;
}

/* Builder */
.builder-layout {
  display: grid;
  grid-template-columns: 1fr 400px;
  gap: 32px;
}
.builder-tabs {
  display: flex;
  gap: 8px;
  margin-bottom: 24px;
  overflow-x: auto;
  padding-bottom: 4px;
}
.builder-tab {
  padding: 10px 18px;
  background: var(--surface-2);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  cursor: pointer;
  white-space: nowrap;
  transition: all 0.15s;
  font-size: 13px;
  color: var(--text);
}
.builder-tab:hover { background: var(--border); }
.builder-tab.active {
  background: var(--primary);
  color: white;
  border-color: var(--primary);
}
.builder-panel { display: none; }
.builder-panel.active { display: block; }

/* Preview */
.builder-preview {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 24px;
  position: sticky;
  top: 100px;
  height: fit-content;
}
.preview-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
  font-size: 14px;
  font-weight: 500;
}
.preview-tabs {
  display: flex;
  gap: 4px;
  margin-bottom: 20px;
}
.preview-tab {
  flex: 1;
  padding: 10px;
  text-align: center;
  font-size: 12px;
  background: var(--surface-2);
  border: 1px solid var(--border);
  cursor: pointer;
  transition: all 0.15s;
}
.preview-tab:first-child { border-radius: var(--radius-sm) 0 0 var(--radius-sm); }
.preview-tab:last-child { border-radius: 0 var(--radius-sm) var(--radius-sm) 0; }
.preview-tab.active {
  background: var(--primary);
  color: white;
  border-color: var(--primary);
}

/* Device Frame */
.device-container { display: flex; justify-content: center; padding: 10px; }
.device-outer {
  background: linear-gradient(145deg, #2a2a2a, #1a1a1a);
  border-radius: 24px;
  padding: 10px;
  box-shadow: 0 15px 50px rgba(0,0,0,0.6), inset 0 1px 0 rgba(255,255,255,0.08);
  transition: all 0.4s cubic-bezier(0.4,0,0.2,1);
}
.device-outer.landscape { width: 100%; max-width: 340px; }
.device-outer.portrait { width: 60%; max-width: 220px; }
.device-inner {
  background: #111;
  border-radius: 16px;
  padding: 6px;
  position: relative;
}
.device-inner::before {
  content: '';
  position: absolute;
  top: 50%;
  right: 6px;
  transform: translateY(-50%);
  width: 3px;
  height: 30px;
  background: #333;
  border-radius: 2px;
}
.eink-screen {
  border-radius: 10px;
  position: relative;
  overflow: hidden;
  transition: all 0.3s;
}
.device-outer.landscape .eink-screen { aspect-ratio: 800/480; }
.device-outer.portrait .eink-screen { aspect-ratio: 480/800; }

/* E-Ink Themes */
.eink-screen.light { background: #f5f5f0; color: #1a1a1a; }
.eink-screen.dark { background: #1a1a1a; color: #e5e5e0; }
.eink-screen.contrast { background: #ffffff; color: #000000; }
.eink-screen.light::after {
  content: '';
  position: absolute;
  inset: 0;
  background: url("data:image/svg+xml,%3Csvg viewBox='0 0 100 100' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.8' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)'/%3E%3C/svg%3E");
  opacity: 0.03;
  pointer-events: none;
}
.eink-screen.dark::after {
  content: '';
  position: absolute;
  inset: 0;
  background: url("data:image/svg+xml,%3Csvg viewBox='0 0 100 100' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.8' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)'/%3E%3C/svg%3E");
  opacity: 0.05;
  pointer-events: none;
}

/* Status Bar */
.eink-statusbar {
  height: 20px;
  padding: 2px 10px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 10px;
  font-weight: 500;
  border-bottom: 1px solid currentColor;
  opacity: 0.6;
}
.statusbar-section { display: flex; align-items: center; gap: 8px; }

/* Home Screen Grid */
.eink-grid {
  display: grid;
  padding: 10px;
  gap: 8px;
  height: calc(100% - 20px);
  align-content: center;
  justify-content: center;
}
.eink-grid.no-status { height: 100%; }
.eink-grid.l-2x2 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(2,1fr); max-width: 60%; margin: 0 auto; }
.eink-grid.l-3x2 { grid-template-columns: repeat(3,1fr); grid-template-rows: repeat(2,1fr); }
.eink-grid.l-4x2 { grid-template-columns: repeat(4,1fr); grid-template-rows: repeat(2,1fr); }
.eink-grid.l-5x3 { grid-template-columns: repeat(5,1fr); grid-template-rows: repeat(3,1fr); }
.eink-grid.p-2x2 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(2,1fr); max-height: 50%; margin: auto 0; }
.eink-grid.p-2x3 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(3,1fr); }
.eink-grid.p-2x4 { grid-template-columns: repeat(2,1fr); grid-template-rows: repeat(4,1fr); }
.eink-grid.p-3x5 { grid-template-columns: repeat(3,1fr); grid-template-rows: repeat(5,1fr); }

/* App Icons - Match device rendering (text in boxes) */
.eink-app {
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
}
.eink-app-box {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
  padding: 4px;
  text-align: center;
  font-weight: 600;
  transition: all 0.2s;
  word-break: break-word;
  line-height: 1.1;
}
.light .eink-app-box { background: #fff; border: 2px solid #333; color: #333; }
.dark .eink-app-box { background: #333; border: 2px solid #666; color: #eee; }
.contrast .eink-app-box { background: #fff; border: 3px solid #000; color: #000; }
.eink-app-box.style-rounded { border-radius: 10px; }
.eink-app-box.style-square { border-radius: 2px; }
.eink-app-box.style-circle { border-radius: 50%; }
.eink-app.selected .eink-app-box {
  background: #333;
  color: #fff;
}
.dark .eink-app.selected .eink-app-box {
  background: #fff;
  color: #333;
}
.contrast .eink-app.selected .eink-app-box {
  background: #000;
  color: #fff;
}
.eink-app.empty { opacity: 0.25; }
.eink-app.empty .eink-app-box {
  background: transparent !important;
  border-style: dashed;
}

/* Lock Screen */
.eink-lock {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  position: relative;
  text-align: center;
}
.lock-bg {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 80px;
  opacity: 0.15;
}
.lock-bg.photo { background: linear-gradient(135deg, currentColor 0%, transparent 100%); opacity: 0.08; }
.lock-time-display { font-weight: 100; line-height: 1; position: relative; z-index: 1; }
.lock-time-display.analog { font-size: 60px; }
.lock-date-display { margin-top: 8px; opacity: 0.5; font-weight: 400; position: relative; z-index: 1; }
.lock-quote-display { font-style: italic; padding: 20px; line-height: 1.5; opacity: 0.7; max-width: 90%; }
.lock-minimal-display { width: 40px; height: 40px; border: 2px solid currentColor; border-radius: 50%; opacity: 0.3; }
.lock-info {
  position: absolute;
  bottom: 15px;
  left: 0;
  right: 0;
  display: flex;
  justify-content: center;
  gap: 20px;
  font-size: 11px;
  opacity: 0.5;
}

/* Sleep Screen */
.eink-sleep { height: 100%; display: flex; align-items: center; justify-content: center; position: relative; }
.sleep-display { display: flex; align-items: center; justify-content: center; width: 100%; height: 100%; }
.sleep-display.off { background: #050505; }
.sleep-display.clock-mode { font-size: 36px; font-weight: 100; opacity: 0.4; }
.sleep-display.photo-mode { font-size: 70px; opacity: 0.2; }
.sleep-display.art-mode { background: repeating-linear-gradient(45deg, transparent, transparent 10px, currentColor 10px, currentColor 11px); opacity: 0.05; }
.sleep-battery-warn { position: absolute; bottom: 10px; right: 12px; font-size: 10px; opacity: 0.3; }

/* Preview Badge */
.preview-badge {
  background: var(--primary);
  color: #fff;
  font-size: 10px;
  font-weight: 600;
  padding: 3px 8px;
  border-radius: 4px;
}

/* Option Sections */
.option-section {
  margin-bottom: 28px;
}
.option-section h3 {
  font-size: 13px;
  font-weight: 600;
  color: var(--text-muted);
  margin-bottom: 14px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}
.style-options {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
}
.style-option {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
  padding: 16px 12px;
  background: var(--surface-2);
  border: 2px solid var(--border);
  border-radius: var(--radius);
  cursor: pointer;
  transition: all 0.15s;
  text-align: center;
}
.style-option:hover { border-color: var(--text-muted); }
.style-option.selected {
  border-color: var(--primary);
  background: var(--primary-light);
}
.style-option span { font-size: 12px; }

/* Theme Cards */
.theme-preview {
  width: 60px;
  height: 40px;
  border-radius: 6px;
  border: 1px solid var(--border);
}
.theme-preview.light { background: #f5f5f0; }
.theme-preview.dark { background: #1a1a1a; }
.theme-preview.contrast { background: #fff; border: 2px solid #000; }

/* Icon Style Preview */
.icon-preview {
  width: 48px;
  height: 48px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: 600;
  background: var(--border);
}
.icon-preview.rounded { border-radius: 12px; }
.icon-preview.square { border-radius: 4px; }
.icon-preview.circle { border-radius: 50%; }

/* Sleep Option Icons */
.sleep-opt-icon {
  width: 48px;
  height: 48px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
  font-weight: 700;
  background: var(--border);
  border-radius: 8px;
}
.sleep-opt-icon.off {
  background: #111;
}

/* Lock Option Icons */
.lock-opt-icon {
  width: 48px;
  height: 48px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: 700;
  background: var(--border);
  border-radius: 8px;
}
.lock-opt-icon.minimal {
  font-size: 24px;
  font-weight: 100;
}
.lock-opt-icon.small {
  width: 40px;
  height: 40px;
  font-size: 11px;
}

/* Grid Preview */
.grid-preview {
  width: 60px;
  height: 36px;
  display: grid;
  gap: 2px;
  padding: 3px;
  background: var(--border);
  border-radius: 4px;
}
.grid-preview > div {
  background: var(--text-muted);
  border-radius: 2px;
}
.grid-preview.g3x2 { grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(2, 1fr); }
.grid-preview.g4x2 { grid-template-columns: repeat(4, 1fr); grid-template-rows: repeat(2, 1fr); }
.grid-preview.g5x3 { grid-template-columns: repeat(5, 1fr); grid-template-rows: repeat(3, 1fr); }

/* Orientation Preview */
.orient-preview {
  background: var(--border);
  border-radius: 4px;
}
.orient-preview.landscape { width: 60px; height: 36px; }
.orient-preview.portrait { width: 36px; height: 60px; }

/* Plugin Grid */
.plugin-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(90px, 1fr));
  gap: 12px;
}
.plugin-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 16px 8px;
  background: var(--surface-2);
  border: 2px solid var(--border);
  border-radius: var(--radius);
  cursor: pointer;
  transition: all 0.15s;
  text-align: center;
}
.plugin-item:hover { border-color: var(--text-muted); }
.plugin-item.selected {
  border-color: var(--primary);
  background: var(--primary-light);
}
.plugin-item.locked {
  opacity: 0.6;
  cursor: default;
}
.plugin-icon {
  width: 36px;
  height: 36px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
  font-weight: 700;
  background: var(--border);
  border-radius: 8px;
  margin-bottom: 8px;
}
.plugin-item.selected .plugin-icon {
  background: var(--primary);
  color: white;
}
.plugin-item .name { font-size: 11px; color: var(--text-muted); }
.plugin-item.selected .name { color: var(--primary); }
.plugin-category {
  margin-bottom: 24px;
}
.plugin-category h4 {
  font-size: 12px;
  color: var(--text-muted);
  margin-bottom: 12px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

/* File Manager */
.file-tabs {
  display: flex;
  gap: 8px;
  margin-bottom: 24px;
  flex-wrap: wrap;
}
.file-tab {
  padding: 10px 18px;
  background: var(--surface-2);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  cursor: pointer;
  font-size: 13px;
  color: var(--text);
  transition: all 0.15s;
  display: flex;
  align-items: center;
  gap: 8px;
}
.file-tab:hover { background: var(--border); }
.file-tab.active {
  background: var(--primary);
  color: white;
  border-color: var(--primary);
}
.file-tab .count {
  background: rgba(255,255,255,0.2);
  padding: 2px 8px;
  border-radius: 10px;
  font-size: 11px;
}
.file-panel { display: none; }
.file-panel.active { display: block; }
.file-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
  gap: 16px;
}
.file-item {
  background: var(--surface-2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 16px;
  text-align: center;
  cursor: pointer;
  transition: all 0.15s;
  position: relative;
}
.file-item:hover {
  border-color: var(--primary);
  transform: translateY(-2px);
}
.file-item .thumb {
  width: 100%;
  aspect-ratio: 3/4;
  background: var(--border);
  border-radius: var(--radius-sm);
  margin-bottom: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 36px;
  overflow: hidden;
}
.file-item .name {
  font-size: 12px;
  font-weight: 500;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.file-item .meta {
  font-size: 11px;
  color: var(--text-muted);
  margin-top: 4px;
}
.file-empty {
  text-align: center;
  padding: 60px 20px;
  color: var(--text-muted);
}
.file-empty .icon {
  font-size: 64px;
  margin-bottom: 16px;
  opacity: 0.4;
}
.deck-card {
  background: var(--surface-2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 20px;
  cursor: pointer;
  transition: all 0.15s;
  margin-bottom: 12px;
}
.deck-card:hover {
  border-color: var(--primary);
  transform: translateY(-2px);
}
.upload-zone {
  border: 2px dashed var(--border);
  border-radius: var(--radius);
  padding: 48px 24px;
  text-align: center;
  cursor: pointer;
  transition: all 0.2s;
  margin-bottom: 24px;
}
.upload-zone:hover, .upload-zone.dragover {
  border-color: var(--primary);
  background: var(--primary-light);
}
.upload-zone .icon { font-size: 48px; margin-bottom: 12px; opacity: 0.5; }
.upload-zone .text { color: var(--text); font-size: 15px; }
.upload-zone .hint { color: var(--text-muted); font-size: 13px; margin-top: 8px; }
.upload-zone input { display: none; }

/* WiFi List */
.wifi-item {
  display: flex;
  align-items: center;
  gap: 16px;
  padding: 16px;
  background: var(--surface-2);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  margin-bottom: 8px;
  cursor: pointer;
  transition: all 0.15s;
}
.wifi-item:hover {
  border-color: var(--primary);
  background: var(--primary-light);
}
.wifi-icon { font-size: 24px; }
.wifi-info { flex: 1; }
.wifi-name { font-weight: 500; }
.wifi-meta { font-size: 12px; color: var(--text-muted); }

/* Modal */
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.8);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  opacity: 0;
  pointer-events: none;
  transition: opacity 0.2s;
  backdrop-filter: blur(4px);
}
.modal-overlay.show { opacity: 1; pointer-events: auto; }
.modal {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  width: 90%;
  max-width: 440px;
  overflow: hidden;
}
.modal-header {
  padding: 20px 24px;
  border-bottom: 1px solid var(--border);
}
.modal-title { font-size: 18px; font-weight: 600; }
.modal-body { padding: 24px; }
.modal-footer {
  padding: 16px 24px;
  border-top: 1px solid var(--border);
  display: flex;
  gap: 12px;
  justify-content: flex-end;
}

/* Toast */
.toast {
  position: fixed;
  bottom: 100px;
  left: 50%;
  transform: translateX(-50%) translateY(20px);
  background: var(--surface);
  border: 1px solid var(--border);
  padding: 16px 28px;
  border-radius: var(--radius);
  opacity: 0;
  transition: all 0.3s;
  z-index: 200;
  box-shadow: 0 12px 40px rgba(0,0,0,0.4);
  font-weight: 500;
}
.toast.show { opacity: 1; transform: translateX(-50%) translateY(0); }
.toast.success { border-left: 4px solid var(--success); }
.toast.error { border-left: 4px solid var(--danger); }

/* FAB */
.fab {
  position: fixed;
  bottom: 24px;
  right: 24px;
  z-index: 50;
  display: flex;
  gap: 12px;
}
.fab .btn {
  padding: 16px 28px;
  font-size: 15px;
  box-shadow: 0 8px 24px rgba(99,102,241,0.4);
}

/* Responsive */
@media (max-width: 1100px) {
  .builder-layout { grid-template-columns: 1fr; }
  .builder-preview {
    position: relative;
    top: 0;
    order: -1;
    max-width: 500px;
    margin: 0 auto 24px;
  }
}
@media (max-width: 768px) {
  .menu-btn { display: block; }
  .sidebar {
    display: none;
    position: fixed;
    top: 65px;
    left: 0;
    bottom: 0;
    width: 280px;
    z-index: 99;
    box-shadow: 4px 0 24px rgba(0,0,0,0.4);
  }
  .sidebar.open { display: block; }
  .content { padding: 20px; padding-bottom: 100px; }
  .stats-grid { grid-template-columns: repeat(2, 1fr); }
  .plugin-grid { grid-template-columns: repeat(auto-fill, minmax(75px, 1fr)); }
  .style-options { grid-template-columns: repeat(2, 1fr); }
}

</style>
</head>
<body>
</head>
<body>
<div class="app">
  <header class="header">
    <button class="menu-btn" onclick="toggleSidebar()">☰</button>
    <div class="logo">SUMI</div>
    <div class="header-status">
      <div class="status-badge">
        <span class="status-dot"></span>
        <span id="headerStatus">Connected</span>
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
      <!-- HOME PAGE -->
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
            <p style="color:var(--text-muted);margin-bottom:20px;">Get started with these essential steps:</p>
            <div class="btn-group" style="display:grid;grid-template-columns:repeat(2,1fr);gap:10px;">
              <button class="btn btn-primary" onclick="showPage('wifi')">📶 Connect WiFi</button>
              <button class="btn btn-secondary" onclick="showPage('builder')">🎨 Customize Device</button>
              <button class="btn btn-secondary" onclick="showPage('files')">📁 Manage Files</button>
              <button class="btn btn-secondary" onclick="showPage('bluetooth')">⌨️ Pair Keyboard</button>
              <button class="btn btn-secondary" onclick="api('/api/refresh','POST');toast('Display refreshed','success')" style="grid-column:span 2">🔄 Refresh Screen</button>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Device Info</span>
          </div>
          <div class="card-body">
            <table style="width:100%;font-size:14px;">
              <tr><td style="color:var(--text-muted);padding:10px 0">Firmware</td><td style="text-align:right" id="infoFirmware">--</td></tr>
              <tr><td style="color:var(--text-muted);padding:10px 0">Display</td><td style="text-align:right">4.26" 800×480 E-Ink</td></tr>
              <tr><td style="color:var(--text-muted);padding:10px 0">IP Address</td><td style="text-align:right" id="infoIP">--</td></tr>
              <tr><td style="color:var(--text-muted);padding:10px 0">Free Memory</td><td style="text-align:right" id="infoMem">--</td></tr>
            </table>
          </div>
        </div>
      </div>

      <!-- BUILDER PAGE -->
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
              <div class="builder-tab" onclick="showBuilderTab('lockscreen')">🔒 Lock Screen</div>
              <div class="builder-tab" onclick="showBuilderTab('sleep')">😴 Sleep Screen</div>
            </div>
            
            <!-- APPS TAB -->
            <div class="builder-panel active" id="panel-apps">
              <p style="color:var(--text-muted);margin-bottom:20px;">Select the apps to show on your home screen:</p>
              
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
                  <label class="style-option selected" onclick="selectTheme(0)">
                    <div class="theme-preview light"></div>
                    <span>Light</span>
                  </label>
                  <label class="style-option" onclick="selectTheme(1)">
                    <div class="theme-preview dark"></div>
                    <span>Dark</span>
                  </label>
                  <label class="style-option" onclick="selectTheme(2)">
                    <div class="theme-preview contrast"></div>
                    <span>High Contrast</span>
                  </label>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Corner Style</h3>
                <div class="style-options">
                  <label class="style-option selected" onclick="selectIconStyle('rounded')">
                    <div class="icon-preview rounded">Abc</div>
                    <span>Rounded</span>
                  </label>
                  <label class="style-option" onclick="selectIconStyle('square')">
                    <div class="icon-preview square">Abc</div>
                    <span>Square</span>
                  </label>
                  <label class="style-option" onclick="selectIconStyle('circle')">
                    <div class="icon-preview circle">Abc</div>
                    <span>Circle</span>
                  </label>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Font Size</h3>
                <div class="slider-row">
                  <span class="slider-label">App Labels</span>
                  <input type="range" class="slider-input" id="fontSizeSlider" min="10" max="24" value="12" oninput="updateFontSize(this.value)">
                  <span class="slider-value" id="fontSizeVal">12px</span>
                </div>
              </div>
            </div>
            
            <!-- LAYOUT TAB -->
            <div class="builder-panel" id="panel-layout">
              <div class="option-section">
                <h3>Screen Orientation</h3>
                <div class="style-options" style="grid-template-columns: repeat(2, 1fr);">
                  <label class="style-option" onclick="selectOrientation('landscape')">
                    <div class="orient-preview landscape"></div>
                    <span>Landscape</span>
                  </label>
                  <label class="style-option selected" onclick="selectOrientation('portrait')">
                    <div class="orient-preview portrait"></div>
                    <span>Portrait</span>
                  </label>
                </div>
                <p style="color:var(--text-muted);font-size:12px;margin-top:10px;">App grid will automatically adjust based on enabled apps</p>
              </div>
              
              <div class="option-section">
                <h3>Status Bar</h3>
                <div class="toggle"><span class="toggle-label">Show Status Bar</span><div class="toggle-switch on" id="togStatusBar" onclick="toggleBuilder(this,'showStatusBar')"></div></div>
                <div class="toggle"><span class="toggle-label">Show Battery</span><div class="toggle-switch on" id="togBattery" onclick="toggleBuilder(this,'showBattery')"></div></div>
                <div class="toggle"><span class="toggle-label">Show Clock</span><div class="toggle-switch on" id="togClock" onclick="toggleBuilder(this,'showClock')"></div></div>
                <div class="toggle"><span class="toggle-label">Show WiFi</span><div class="toggle-switch" id="togWifi" onclick="toggleBuilder(this,'showWifi')"></div></div>
              </div>
            </div>
            
            <!-- LOCKSCREEN TAB -->
            <div class="builder-panel" id="panel-lockscreen">
              <div class="option-section">
                <h3>Lock Screen Style</h3>
                <div class="style-options" style="grid-template-columns: repeat(2, 1fr);">
                  <label class="style-option selected" onclick="selectLockStyle('clock')">
                    <div class="lock-opt-icon">12:34</div>
                    <span>Clock</span>
                  </label>
                  <label class="style-option" onclick="selectLockStyle('photo')">
                    <div class="lock-opt-icon">IMG</div>
                    <span>Photo</span>
                  </label>
                  <label class="style-option" onclick="selectLockStyle('quote')">
                    <div class="lock-opt-icon">" "</div>
                    <span>Quote</span>
                  </label>
                  <label class="style-option" onclick="selectLockStyle('minimal')">
                    <div class="lock-opt-icon minimal">○</div>
                    <span>Minimal</span>
                  </label>
                </div>
              </div>
              
              <div class="option-section" id="lockPhotoOptions" style="display:none;">
                <h3>Photo Source</h3>
                <div class="style-options">
                  <label class="style-option selected" onclick="selectPhotoSource('shuffle')">
                    <div class="lock-opt-icon small">RND</div>
                    <span>Shuffle</span>
                  </label>
                  <label class="style-option" onclick="selectPhotoSource('select')">
                    <div class="lock-opt-icon small">ONE</div>
                    <span>Single</span>
                  </label>
                  <label class="style-option" onclick="selectPhotoSource('folder')">
                    <div class="lock-opt-icon small">DIR</div>
                    <span>Folder</span>
                  </label>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Lock Screen Info</h3>
                <div class="toggle"><span class="toggle-label">Show Date</span><div class="toggle-switch on" id="togLockDate" onclick="toggleBuilder(this,'showDate')"></div></div>
                <div class="toggle"><span class="toggle-label">Show Battery</span><div class="toggle-switch on" id="togLockBattery" onclick="toggleBuilder(this,'showBatteryLock')"></div></div>
                <div class="toggle"><span class="toggle-label">Show Weather</span><div class="toggle-switch" id="togLockWeather" onclick="toggleBuilder(this,'showWeatherLock')"></div></div>
              </div>
            </div>
            
            <!-- SLEEP SCREEN TAB -->
            <div class="builder-panel" id="panel-sleep">
              <div class="option-section">
                <h3>Sleep Screen Display</h3>
                <p style="color:var(--text-muted);font-size:13px;margin-bottom:14px;">What to show when device enters deep sleep mode</p>
                <div class="style-options" style="grid-template-columns: repeat(2, 1fr);">
                  <label class="style-option" onclick="selectSleepStyle('book')">
                    <div class="sleep-opt-icon">B</div>
                    <span>Current Book</span>
                  </label>
                  <label class="style-option" onclick="selectSleepStyle('shuffle')">
                    <div class="sleep-opt-icon">S</div>
                    <span>Shuffle Images</span>
                  </label>
                  <label class="style-option selected" onclick="selectSleepStyle('off')">
                    <div class="sleep-opt-icon off"></div>
                    <span>Black Screen</span>
                  </label>
                </div>
              </div>
              
              <div class="option-section">
                <h3>Sleep Options</h3>
                <div class="toggle"><span class="toggle-label">Show Battery Warning</span><div class="toggle-switch on" id="togSleepBattery" onclick="toggleBuilder(this,'showBatterySleep')"></div></div>
              </div>
              
              <div class="option-section">
                <h3>Wake Button</h3>
                <div class="select-wrap">
                  <select id="wakeButton" onchange="builderConfig.wakeButton=this.value">
                    <option value="any">Any Button</option>
                    <option value="select">Select Button Only</option>
                    <option value="power">Power Button Only</option>
                  </select>
                </div>
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
              <div class="preview-tab" onclick="setPreviewMode('lock')">🔒 Lock</div>
              <div class="preview-tab" onclick="setPreviewMode('sleep')">😴 Sleep</div>
            </div>
            <div class="device-container">
              <div class="device-outer portrait" id="deviceFrame">
                <div class="device-inner">
                  <div class="eink-screen light" id="einkScreen"></div>
                </div>
              </div>
            </div>
            <div id="previewStats" style="margin-top:16px;text-align:center;font-size:11px;color:var(--text-muted)"></div>
            <div style="margin-top:12px;display:flex;gap:8px;justify-content:center">
              <button class="btn btn-sm btn-secondary" onclick="resetToDefaults()">↩️ Reset All</button>
            </div>
          </div>
        </div>
      </div>

      <!-- WIFI PAGE -->
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
            <div id="wifiStatus" style="font-size:16px;">Checking...</div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Available Networks</span>
            <button class="btn btn-sm btn-secondary" onclick="scanWifi()" id="wifiScanBtn">🔍 Scan</button>
          </div>
          <div class="card-body">
            <div id="wifiList"><p style="color:var(--text-muted);text-align:center;padding:20px;">Click Scan to find networks</p></div>
          </div>
        </div>
      </div>

      <!-- FILES PAGE -->
      <div class="page" id="page-files">
        <div class="page-header">
          <h1 class="page-title">File Manager</h1>
          <p class="page-desc">Manage your books, images, and more</p>
        </div>
        
        <div class="file-tabs">
          <div class="file-tab active" onclick="showFileTab('books')">📚 Books <span class="count" id="countBooks">0</span></div>
          <div class="file-tab" onclick="showFileTab('images')">🖼️ Images <span class="count" id="countImages">0</span></div>
          <div class="file-tab" onclick="showFileTab('maps')">🗺️ Maps <span class="count" id="countMaps">0</span></div>
          <div class="file-tab" onclick="showFileTab('flashcards')">🎴 Flashcards <span class="count" id="countFlashcards">0</span></div>
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
              <button class="btn btn-sm btn-secondary" onclick="refreshFiles('books')">🔄 Refresh</button>
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
              <div class="slider-row">
                <span class="slider-label">Font Size</span>
                <input type="range" class="slider-input" id="readerFontSize" min="12" max="32" value="18" oninput="updateSlider(this,'readerFontVal','px')">
                <span class="slider-value" id="readerFontVal">18px</span>
              </div>
              <div class="slider-row">
                <span class="slider-label">Line Spacing</span>
                <input type="range" class="slider-input" id="readerLineHeight" min="100" max="200" step="10" value="150" oninput="updateSlider(this,'readerLineVal','%')">
                <span class="slider-value" id="readerLineVal">150%</span>
              </div>
              <div class="slider-row">
                <span class="slider-label">Margins</span>
                <input type="range" class="slider-input" id="readerMargins" min="5" max="40" value="20" oninput="updateSlider(this,'readerMarginVal','px')">
                <span class="slider-value" id="readerMarginVal">20px</span>
              </div>
              <div class="toggle">
                <span class="toggle-label">Justify Text</span>
                <div class="toggle-switch on" id="togJustify" onclick="toggleReaderOpt(this,'textAlign')"></div>
              </div>
              <div class="toggle">
                <span class="toggle-label">Show Progress Bar</span>
                <div class="toggle-switch on" id="togProgress" onclick="toggleReaderOpt(this,'showProgress')"></div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Images Panel -->
        <div class="file-panel" id="panel-images">
          <div class="upload-zone" onclick="this.querySelector('input').click()" ondragover="event.preventDefault();this.classList.add('dragover')" ondragleave="this.classList.remove('dragover')" ondrop="handleDrop(event,'images')">
            <div class="icon">🖼️</div>
            <div class="text">Drop images here or click to upload</div>
            <div class="hint">Supports: JPG, PNG, BMP (will convert for e-ink)</div>
            <input type="file" accept=".jpg,.jpeg,.png,.bmp" multiple onchange="uploadFiles(this.files,'images')">
          </div>
          <div class="card">
            <div class="card-header">
              <span class="card-title">Your Images</span>
              <button class="btn btn-sm btn-secondary" onclick="refreshFiles('images')">🔄 Refresh</button>
            </div>
            <div class="card-body">
              <div class="file-grid" id="imageGrid">
                <div class="file-empty"><div class="icon">🖼️</div><div>No images yet</div></div>
              </div>
            </div>
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Image Settings</span></div>
            <div class="card-body">
              <div class="toggle">
                <div><span class="toggle-label">Auto-convert to grayscale</span><div class="toggle-desc">Optimize colors for e-ink display</div></div>
                <div class="toggle-switch on" id="togGrayscale" onclick="toggleImageOpt(this,'grayscale')"></div>
              </div>
              <div class="toggle">
                <div><span class="toggle-label">Dither for e-ink</span><div class="toggle-desc">Apply Floyd-Steinberg dithering</div></div>
                <div class="toggle-switch on" id="togDither" onclick="toggleImageOpt(this,'dither')"></div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Maps Panel -->
        <div class="file-panel" id="panel-maps">
          <div class="upload-zone" onclick="this.querySelector('input').click()" ondragover="event.preventDefault();this.classList.add('dragover')" ondragleave="this.classList.remove('dragover')" ondrop="handleDrop(event,'maps')">
            <div class="icon">🗺️</div>
            <div class="text">Drop map images here or click to upload</div>
            <div class="hint">Supports: PNG, BMP, JPG (or OSM tile folders)</div>
            <input type="file" accept=".png,.bmp,.jpg,.jpeg" multiple onchange="uploadFiles(this.files,'maps')">
          </div>
          <div class="card">
            <div class="card-header">
              <span class="card-title">Your Maps</span>
              <button class="btn btn-sm btn-secondary" onclick="refreshFiles('maps')">🔄 Refresh</button>
            </div>
            <div class="card-body">
              <div class="file-grid" id="mapGrid">
                <div class="file-empty"><div class="icon">🗺️</div><div>No maps yet</div></div>
              </div>
            </div>
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Getting Offline Maps</span></div>
            <div class="card-body" style="color:var(--text-muted);font-size:14px;">
              <p style="margin-bottom:12px;"><strong>Option 1: Simple Images</strong><br>
              Upload any map image (trail maps, campus maps, transit maps) as PNG, BMP, or JPG.</p>
              <p style="margin-bottom:12px;"><strong>Option 2: OSM Tiles</strong><br>
              Use <a href="https://mobac.sourceforge.io/" target="_blank" style="color:var(--primary);">Mobile Atlas Creator</a> to download tile sets. Copy the folder to /maps/ on SD card.</p>
              <p><strong>Tile folder structure:</strong><br>
              <code style="font-size:12px;">/maps/myarea/12/2048/1365.png</code></p>
            </div>
          </div>
        </div>
        
        <!-- Flashcards Panel -->
        <div class="file-panel" id="panel-flashcards">
          <div class="btn-group" style="margin-bottom:20px;">
            <button class="btn btn-primary" onclick="showCreateDeckModal()">➕ Create Deck</button>
            <label class="btn btn-secondary">
              📥 Import Anki
              <input type="file" accept=".apkg,.txt,.csv" style="display:none" onchange="importAnkiDeck(this.files[0])">
            </label>
          </div>
          <div class="card">
            <div class="card-header">
              <span class="card-title">Your Decks</span>
              <button class="btn btn-sm btn-secondary" onclick="refreshFiles('flashcards')">🔄 Refresh</button>
            </div>
            <div class="card-body" id="deckList">
              <div class="file-empty"><div class="icon">🎴</div><div>No flashcard decks yet</div></div>
            </div>
          </div>
          <div class="card">
            <div class="card-header"><span class="card-title">Flashcard Settings</span></div>
            <div class="card-body">
              <div class="form-group">
                <label class="form-label">New Cards Per Day</label>
                <input type="number" class="form-input" id="fcNewPerDay" value="20" min="0" max="100" onchange="saveFcSetting('newPerDay',this.value)">
              </div>
              <div class="form-group">
                <label class="form-label">Daily Review Limit</label>
                <input type="number" class="form-input" id="fcReviewLimit" value="200" min="0" max="500" onchange="saveFcSetting('reviewLimit',this.value)">
              </div>
              <div class="slider-row">
                <span class="slider-label">Target Retention</span>
                <input type="range" class="slider-input" id="fcRetention" min="70" max="99" value="90" oninput="updateSlider(this,'fcRetVal','%')">
                <span class="slider-value" id="fcRetVal">90%</span>
              </div>
              <div class="toggle">
                <span class="toggle-label">Use FSRS Algorithm</span>
                <div class="toggle-switch on" id="togFsrs" onclick="toggleFcOpt(this,'useFsrs')"></div>
              </div>
              <div class="toggle">
                <span class="toggle-label">Shuffle New Cards</span>
                <div class="toggle-switch on" id="togShuffle" onclick="toggleFcOpt(this,'shuffle')"></div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Notes Panel -->
        <div class="file-panel" id="panel-notes">
          <div class="btn-group" style="margin-bottom:20px;">
            <button class="btn btn-primary" onclick="createNewNote()">📝 New Note</button>
          </div>
          <div class="card">
            <div class="card-header">
              <span class="card-title">Your Notes</span>
              <button class="btn btn-sm btn-secondary" onclick="refreshFiles('notes')">🔄 Refresh</button>
            </div>
            <div class="card-body">
              <div class="file-grid" id="noteGrid">
                <div class="file-empty"><div class="icon">📝</div><div>No notes yet</div></div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- WEATHER PAGE -->
      <div class="page" id="page-weather">
        <div class="page-header">
          <h1 class="page-title">Weather & Time</h1>
          <p class="page-desc">Location and timezone settings</p>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Location</span></div>
          <div class="card-body">
            <p style="color:var(--text-muted);margin-bottom:20px;">Enter your ZIP code to get weather for your exact location instead of using IP-based detection.</p>
            <div class="form-group">
              <label class="form-label">ZIP Code</label>
              <div style="display:flex;gap:10px;">
                <input type="text" class="form-input" id="weatherZip" placeholder="e.g. 80424" maxlength="5" pattern="[0-9]{5}" style="flex:1;">
                <button class="btn btn-primary" onclick="saveWeatherZip()">Save</button>
              </div>
            </div>
            <div id="weatherLocation" style="margin-top:15px;padding:15px;background:var(--bg-tertiary);border-radius:8px;display:none;">
              <div style="font-weight:600;" id="weatherCity">--</div>
              <div style="color:var(--text-muted);font-size:14px;" id="weatherCoords">--</div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Timezone</span></div>
          <div class="card-body">
            <p style="color:var(--text-muted);margin-bottom:15px;">Override auto-detected timezone if the time is wrong.</p>
            <div class="form-group">
              <label class="form-label">Timezone</label>
              <select class="form-input" id="timezoneSelect" onchange="saveTimezone(this.value)">
                <option value="auto">Auto-detect from IP</option>
                <option value="-18000">Eastern (UTC-5)</option>
                <option value="-21600">Central (UTC-6)</option>
                <option value="-25200">Mountain (UTC-7)</option>
                <option value="-28800">Pacific (UTC-8)</option>
                <option value="-32400">Alaska (UTC-9)</option>
                <option value="-36000">Hawaii (UTC-10)</option>
                <option value="0">UTC</option>
                <option value="3600">Central Europe (UTC+1)</option>
                <option value="7200">Eastern Europe (UTC+2)</option>
                <option value="19800">India (UTC+5:30)</option>
                <option value="28800">China/Singapore (UTC+8)</option>
                <option value="32400">Japan/Korea (UTC+9)</option>
                <option value="36000">Australia East (UTC+10)</option>
              </select>
            </div>
            <div id="currentTime" style="margin-top:15px;padding:12px;background:var(--bg-tertiary);border-radius:8px;font-family:monospace;"></div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Units</span></div>
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Use Celsius</span><div class="toggle-desc">Show temperature in °C instead of °F</div></div>
              <div class="toggle-switch" id="togCelsius" onclick="toggleWeatherUnit(this)"></div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-body">
            <button class="btn btn-secondary btn-block" onclick="clearWeatherLocation()">🗑️ Clear Location (Use Auto-Detect)</button>
          </div>
        </div>
      </div>

      <!-- DISPLAY PAGE -->
      <div class="page" id="page-display">
        <div class="page-header">
          <h1 class="page-title">Display Settings</h1>
          <p class="page-desc">Screen and refresh options</p>
        </div>
        
        <div class="card">
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Landscape Mode</span><div class="toggle-desc">Rotate screen 90 degrees</div></div>
              <div class="toggle-switch" id="togLandscape" onclick="toggleOpt(this,'landscape')"></div>
            </div>
            <div class="toggle">
              <div><span class="toggle-label">Invert Colors</span><div class="toggle-desc">White text on black background</div></div>
              <div class="toggle-switch" id="togInvert" onclick="toggleOpt(this,'invertColors')"></div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Auto-Sleep</span></div>
          <div class="card-body">
            <div class="slider-row">
              <span class="slider-label">Sleep after</span>
              <input type="range" class="slider-input" id="sleepSlider" min="5" max="60" step="5" value="15" oninput="updateSlider(this,'sleepVal',' min')">
              <span class="slider-value" id="sleepVal">15 min</span>
            </div>
          </div>
        </div>
      </div>

      <!-- BLUETOOTH PAGE -->
      <div class="page" id="page-bluetooth">
        <div class="page-header">
          <h1 class="page-title">Bluetooth Keyboard</h1>
          <p class="page-desc">Connect a wireless keyboard for typing</p>
        </div>
        
        <div class="card">
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Enable Bluetooth</span><div class="toggle-desc">Allow keyboard connections</div></div>
              <div class="toggle-switch" id="btEnabled" onclick="toggleBT(this)"></div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Paired Devices</span>
          </div>
          <div class="card-body">
            <div id="btPaired"><p style="color:var(--text-muted);text-align:center;padding:20px;">No devices paired yet</p></div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header">
            <span class="card-title">Available Devices</span>
            <button class="btn btn-sm btn-secondary" id="btScanBtn" onclick="scanBT()">🔍 Scan</button>
          </div>
          <div class="card-body">
            <div id="btDiscovered"><p style="color:var(--text-muted);text-align:center;padding:20px;">Click Scan to search for keyboards</p></div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">Keyboard Layout</span></div>
          <div class="card-body">
            <div class="select-wrap">
              <select id="btLayout" onchange="api('/api/bluetooth/layout','POST',{layout:this.value})">
                <option value="us">US English (QWERTY)</option>
                <option value="uk">UK English</option>
                <option value="de">German (QWERTZ)</option>
                <option value="fr">French (AZERTY)</option>
                <option value="es">Spanish</option>
                <option value="jp">Japanese</option>
              </select>
            </div>
          </div>
        </div>
      </div>

      <!-- POWER PAGE -->
      <div class="page" id="page-power">
        <div class="page-header">
          <h1 class="page-title">Power Management</h1>
          <p class="page-desc">Battery and power settings</p>
        </div>
        
        <div class="card">
          <div class="card-body" style="text-align:center;padding:40px;">
            <div style="font-size:64px;margin-bottom:16px;">🔋</div>
            <div style="font-size:48px;font-weight:700;" id="powerPercent">--%</div>
            <div style="color:var(--text-muted);margin-top:8px;font-size:16px;" id="powerVolts">-- V</div>
          </div>
        </div>
        
        <div class="card">
          <div class="card-body">
            <div class="toggle">
              <div><span class="toggle-label">Deep Sleep</span><div class="toggle-desc">Maximum power saving when idle</div></div>
              <div class="toggle-switch on" id="togDeepSleep" onclick="toggleOpt(this,'deepSleep')"></div>
            </div>
          </div>
        </div>
      </div>

      <!-- ABOUT PAGE -->
      <div class="page" id="page-about">
        <div class="page-header">
          <h1 class="page-title">About</h1>
          <p class="page-desc">Device information and system actions</p>
        </div>
        
        <div class="card">
          <div class="card-body">
            <table style="width:100%;font-size:14px;">
              <tr><td style="color:var(--text-muted);padding:12px 0;">Firmware</td><td style="text-align:right;" id="aboutFirmware">--</td></tr>
              <tr><td style="color:var(--text-muted);padding:12px 0;">Hardware</td><td style="text-align:right;">ESP32-C3</td></tr>
              <tr><td style="color:var(--text-muted);padding:12px 0;">Display</td><td style="text-align:right;">GDEQ0426T82 4.26"</td></tr>
              <tr><td style="color:var(--text-muted);padding:12px 0;">Resolution</td><td style="text-align:right;">800×480</td></tr>
              <tr><td style="color:var(--text-muted);padding:12px 0;">SD Card</td><td style="text-align:right;" id="aboutSD">--</td></tr>
              <tr><td style="color:var(--text-muted);padding:12px 0;">Free Memory</td><td style="text-align:right;" id="aboutMem">--</td></tr>
              <tr><td style="color:var(--text-muted);padding:12px 0;">Uptime</td><td style="text-align:right;" id="aboutUptime">--</td></tr>
            </table>
          </div>
        </div>
        
        <div class="card">
          <div class="card-header"><span class="card-title">System Actions</span></div>
          <div class="card-body" style="display:flex;flex-direction:column;gap:12px;">
            <button class="btn btn-secondary btn-block" onclick="refreshDisplay()">🔄 Refresh Display</button>
            <button class="btn btn-secondary btn-block" onclick="rebootDevice()">🔃 Reboot Device</button>
            <button class="btn btn-danger btn-block" onclick="factoryReset()">🗑️ Factory Reset</button>
          </div>
        </div>
      </div>
    </main>
  </div>
</div>

<!-- FAB -->
<div class="fab" id="fab">
  <button class="btn btn-secondary" onclick="resetToDefaults()">↩️ Reset</button>
  <button class="btn btn-primary" onclick="saveAndDeploy()" id="deployBtn">💾 Save & Deploy</button>
</div>

<!-- Toast -->
<div class="toast" id="toast"></div>

<!-- WiFi Modal -->
<div class="modal-overlay" id="wifiModal">
  <div class="modal">
    <div class="modal-header"><div class="modal-title">Connect to <span id="wifiModalSSID"></span></div></div>
    <div class="modal-body">
      <div class="form-group">
        <label class="form-label">Password</label>
        <input type="password" class="form-input" id="wifiPassword" placeholder="Enter WiFi password">
      </div>
    </div>
    <div class="modal-footer">
      <button class="btn btn-secondary" onclick="closeModal('wifiModal')">Cancel</button>
      <button class="btn btn-primary" onclick="connectWifi()">Connect</button>
    </div>
  </div>
</div>


<script>
// =============================================================================
// CONFIGURATION
// =============================================================================
// SUMI v2.1.23 - E-ink Optimized Plugins
const PLUGINS = {
  core: [
    {id:'library', name:'Library', icon:'L', desc:'Browse and read books'},
    {id:'flashcards', name:'Cards', icon:'F', desc:'Spaced repetition learning'},
    {id:'notes', name:'Notes', icon:'N', desc:'Quick text notes'},
    {id:'images', name:'Images', icon:'I', desc:'View images from SD'},
    {id:'maps', name:'Maps', icon:'P', desc:'Offline map viewer'}
  ],
  games: [
    {id:'chess', name:'Chess', icon:'C', desc:'Play against AI'},
    {id:'checkers', name:'Check', icon:'K', desc:'Classic checkers'},
    {id:'sudoku', name:'Sudoku', icon:'S', desc:'Number puzzles'},
    {id:'minesweeper', name:'Mines', icon:'M', desc:'Find the mines'},
    {id:'solitaire', name:'Solit.', icon:'A', desc:'Klondike solitaire'}
  ],
  tools: [
    {id:'tools', name:'Tools', icon:'T', desc:'Calculator, Timer, Stopwatch'},
    {id:'todo', name:'To-Do', icon:'D', desc:'Task management'}
  ],
  widgets: [
    {id:'weather', name:'Weather', icon:'W', desc:'Current conditions & forecast'}
  ],
  system: [
    {id:'settings', name:'Settings', icon:'G', desc:'WiFi, Display, System options', locked:true}
  ]
};

// Default apps: Library, Flashcards, Chess, Sudoku, Weather, Settings
const DEFAULTS = ['library','flashcards','chess','sudoku','weather','settings'];

// =============================================================================
// STATE
// =============================================================================
let selected = new Set(DEFAULTS);
let originalSelected = new Set(DEFAULTS);
let builderConfig = {
  // Theme & Layout
  theme: 0,
  iconStyle: 'rounded',
  orientation: 'landscape',
  grid: '4x2',
  fontSize: 12,
  
  // Home screen indicators
  showStatusBar: true,
  showBattery: true,
  showClock: true,
  showWifi: false,
  
  // Lock screen
  lockStyle: 'clock',
  lockPhotoSource: 'shuffle',
  clockStyle: 'digital',
  showDate: true,
  showBatteryLock: true,
  showWeatherLock: false,
  
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
  
  document.getElementById('statWifi').textContent = d.wifi?.connected ? 'Connected' : 'AP Mode';
  document.getElementById('headerStatus').textContent = d.wifi?.connected ? 'Connected' : 'Setup Mode';
  
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
          <div class="plugin-item selected locked" title="${p.desc} (always enabled)">
            <div class="plugin-icon">${p.icon}</div>
            <div class="name">${p.name}</div>
            <div class="lock-icon">🔒</div>
          </div>`;
      }
      return `
        <div class="plugin-item ${selected.has(p.id)?'selected':''}" onclick="togglePlugin('${p.id}')" title="${p.desc}">
          <div class="plugin-icon">${p.icon}</div>
          <div class="name">${p.name}</div>
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
  document.getElementById('previewBadge').textContent = {home:'Home',lock:'Lock',sleep:'Sleep'}[mode];
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
  } else if (previewMode === 'lock') {
    renderLockPreview(screen, stats);
  } else {
    renderSleepPreview(screen, stats);
  }
}

function renderHomePreview(screen, stats) {
  const isPortrait = builderConfig.orientation === 'portrait';
  
  // Get enabled plugins first to determine count
  const all = Object.values(PLUGINS).flat();
  let enabled = all.filter(p => selected.has(p.id) && p.id !== 'settings');
  
  // Settings item is permanently visible
  const settingsPlugin = all.find(p => p.id === 'settings');
  if (settingsPlugin) {
    enabled.push(settingsPlugin);
  }
  
  const appCount = enabled.length;
  
  // Auto-calculate grid based on app count and orientation
  let gridClass, cols, rows;
  if (isPortrait) {
    // Portrait: prefer 2 columns
    if (appCount <= 4) { cols = 2; rows = 2; gridClass = 'p-2x2'; }
    else if (appCount <= 6) { cols = 2; rows = 3; gridClass = 'p-2x3'; }
    else if (appCount <= 8) { cols = 2; rows = 4; gridClass = 'p-2x4'; }
    else { cols = 3; rows = 5; gridClass = 'p-3x5'; }
  } else {
    // Landscape: prefer 2 rows
    if (appCount <= 4) { cols = 2; rows = 2; gridClass = 'l-2x2'; }
    else if (appCount <= 6) { cols = 3; rows = 2; gridClass = 'l-3x2'; }
    else if (appCount <= 8) { cols = 4; rows = 2; gridClass = 'l-4x2'; }
    else { cols = 5; rows = 3; gridClass = 'l-5x3'; }
  }
  
  const maxItems = cols * rows;
  
  // Icon sizing based on grid density
  let iconSize, emojiSize, labelSize;
  if (maxItems <= 4) { iconSize = 44; emojiSize = 24; labelSize = 11; }
  else if (maxItems <= 6) { iconSize = 36; emojiSize = 20; labelSize = 10; }
  else if (maxItems <= 8) { iconSize = 32; emojiSize = 18; labelSize = 9; }
  else if (maxItems <= 10) { iconSize = 28; emojiSize = 16; labelSize = 8; }
  else { iconSize = 22; emojiSize = 14; labelSize = 7; }
  
  // Apply font size scaling
  labelSize = Math.round(labelSize * (builderConfig.fontSize / 12));
  
  // Build status bar
  let statusBar = '';
  if (builderConfig.showStatusBar) {
    statusBar = `<div class="eink-statusbar">
      <div class="statusbar-section">${builderConfig.showClock ? '12:34 PM' : ''}</div>
      <div class="statusbar-section">
        ${builderConfig.showWifi ? '<span style="font-size:10px">WiFi</span>' : ''}
        ${builderConfig.showBattery ? '85%' : ''}
      </div>
    </div>`;
  }
  
  // Build app grid - match device appearance 
  let apps = '';
  for (let i = 0; i < maxItems; i++) {
    if (i < enabled.length) {
      const p = enabled[i];
      const isSelected = i === 0; // First item selected for preview
      apps += `<div class="eink-app ${isSelected ? 'selected' : ''}">
        <div class="eink-app-box style-${builderConfig.iconStyle}" style="font-size:${labelSize}px">${p.name}</div>
      </div>`;
    } else {
      apps += `<div class="eink-app empty">
        <div class="eink-app-box style-${builderConfig.iconStyle}" style="font-size:${labelSize}px;opacity:0.3">+</div>
      </div>`;
    }
  }
  
  screen.innerHTML = `
    ${statusBar}
    <div class="eink-grid ${gridClass} ${builderConfig.showStatusBar ? '' : 'no-status'}">${apps}</div>
  `;
  
  const themeLabel = ['Light', 'Dark', 'High Contrast'][builderConfig.theme];
  stats.innerHTML = `${appCount} apps · ${themeLabel} theme · ${builderConfig.iconStyle} corners`;
}

function renderLockPreview(screen, stats) {
  const themeLabel = ['Light', 'Dark', 'High Contrast'][builderConfig.theme];
  let html = '';
  
  // Background layer for photo mode
  if (builderConfig.lockStyle === 'photo') {
    html += `<div class="lock-bg photo"></div>`;
  }
  
  // Main lock content
  html += `<div class="eink-lock">`;
  
  if (builderConfig.lockStyle === 'quote') {
    html += `<div class="lock-quote-display">"The journey of a thousand miles begins with a single step."</div>`;
  } else if (builderConfig.lockStyle === 'minimal') {
    html += `<div class="lock-minimal-display"></div>`;
  } else {
    // Clock display
    if (builderConfig.clockStyle === 'analog') {
      html += `<div class="lock-time-display analog" style="font-size:48px;font-weight:100">10:10</div>`;
    } else {
      const fontSize = builderConfig.clockStyle === 'minimal' ? '52px' : '38px';
      const weight = builderConfig.clockStyle === 'minimal' ? '50' : '100';
      html += `<div class="lock-time-display" style="font-size:${fontSize};font-weight:${weight}">12:34</div>`;
    }
    
    if (builderConfig.lockShowDate) {
      html += `<div class="lock-date-display" style="font-size:12px">Thursday, December 18</div>`;
    }
  }
  
  html += `</div>`;
  
  // Footer info
  if (builderConfig.lockShowBattery || builderConfig.lockShowWeather) {
    html += `<div class="lock-info">
      ${builderConfig.lockShowWeather ? '<span>72°F</span>' : ''}
      ${builderConfig.lockShowBattery ? '<span>85%</span>' : ''}
    </div>`;
  }
  
  screen.innerHTML = html;
  
  const styleLabel = {clock: 'Clock', photo: 'Photo', quote: 'Quote', minimal: 'Minimal'}[builderConfig.lockStyle];
  const clockLabel = (builderConfig.lockStyle !== 'quote' && builderConfig.lockStyle !== 'minimal') ? 
    ` · ${builderConfig.clockStyle} clock` : '';
  stats.innerHTML = `${styleLabel} style${clockLabel} · ${themeLabel} theme`;
}

function renderSleepPreview(screen, stats) {
  const themeLabel = ['Light', 'Dark', 'High Contrast'][builderConfig.theme];
  let html = '<div class="eink-sleep">';
  
  if (builderConfig.sleepStyle === 'off') {
    html += `<div class="sleep-display off"></div>`;
  } else if (builderConfig.sleepStyle === 'book') {
    html += `<div class="sleep-display clock-mode" style="flex-direction:column;gap:10px;">
      <div style="font-size:16px;font-weight:bold;">BOOK</div>
      <div style="font-size:11px;opacity:0.6;">Current Book Cover</div>
    </div>`;
  } else if (builderConfig.sleepStyle === 'shuffle') {
    html += `<div class="sleep-display clock-mode" style="flex-direction:column;gap:10px;">
      <div style="font-size:16px;font-weight:bold;">SHUFFLE</div>
      <div style="font-size:11px;opacity:0.6;">Random Image</div>
    </div>`;
  } else if (builderConfig.sleepStyle === 'photo') {
    html += `<div class="sleep-display clock-mode" style="flex-direction:column;gap:10px;">
      <div style="font-size:16px;font-weight:bold;">IMAGE</div>
      <div style="font-size:11px;opacity:0.6;">Selected Image</div>
    </div>`;
  }
  
  html += '</div>';
  
  // Battery warning
  if (builderConfig.sleepShowBattery && builderConfig.sleepStyle !== 'off') {
    html += `<div class="sleep-battery-warn">Low battery warning enabled</div>`;
  }
  
  screen.innerHTML = html;
  
  const styleLabel = {off: 'Black Screen', book: 'Current Book', shuffle: 'Shuffle Images', photo: 'Selected Image'}[builderConfig.sleepStyle];
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
        sleepStyle: {book:0, shuffle:1, photo:2, off:3}[builderConfig.sleepStyle] || 3,
        sleepSelectedImage: builderConfig.sleepSelectedImage || '',
        showBatterySleep: builderConfig.showBatterySleep,
        sleepMinutes: builderConfig.sleepMinutes || 15,
        wakeButton: {any:0, select:1, power:2}[builderConfig.wakeButton] || 0
      },
      // Reader settings
      reader: {
        fontSize: parseInt(document.getElementById('readerFontSize')?.value) || 18,
        lineHeight: parseInt(document.getElementById('readerLineHeight')?.value) || 150,
        margins: parseInt(document.getElementById('readerMargins')?.value) || 20
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
    lockStyle: 'clock', lockPhotoSource: 'shuffle', clockStyle: 'digital',
    showDate: true, showBatteryLock: true, showWeatherLock: false,
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
    files = files.filter(f => !f.dir && /\.(jpg|jpeg|png|bmp|gif)$/i.test(f.name));
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
function toggleOpt(el, key) {
  el.classList.toggle('on');
  api('/api/settings', 'POST', {[key]: el.classList.contains('on')});
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
  setToggle('togLockDate', builderConfig.showDate);
  setToggle('togLockBattery', builderConfig.showBatteryLock);
  setToggle('togLockWeather', builderConfig.showWeatherLock);
  setToggle('togSleepBattery', builderConfig.showBatterySleep);
  setToggle('togSleepRefresh', builderConfig.sleepRefresh);
  
  // Sliders
  setSlider('fontSizeSlider', builderConfig.fontSize, 'fontSizeVal', 'px');
  
  // Selects
  setSelect('wakeButton', builderConfig.wakeButton);
  
  // Style options - update visual selection state
  selectTheme(builderConfig.theme);
  selectIconStyle(builderConfig.iconStyle);
  selectOrientation(builderConfig.orientation);
  // Grid is auto-calculated from app count
  selectLockStyle(builderConfig.lockStyle);
  selectSleepStyle(builderConfig.sleepStyle);
  selectClockStyle(builderConfig.clockStyle);
  selectPhotoSource(builderConfig.lockPhotoSource);
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
    builderConfig.sleepStyle = ['book', 'shuffle', 'photo', 'off'][d.sleepStyle] || 'off';
    builderConfig.sleepSelectedImage = d.sleepSelectedImage || '';
    builderConfig.showBatterySleep = d.showBatterySleep ?? true;
    builderConfig.sleepMinutes = d.sleepMinutes || 15;
    builderConfig.wakeButton = ['any', 'select', 'power'][d.wakeButton] || 'any';
  }
  
  // Save original state for change detection
  originalConfig = {...builderConfig};
  
  // Restore reader settings
  if (data.reader) {
    const r = data.reader;
    const fsEl = document.getElementById('readerFontSize');
    const lhEl = document.getElementById('readerLineHeight');
    const mgEl = document.getElementById('readerMargins');
    if (fsEl) { fsEl.value = r.fontSize || 18; updateSlider(fsEl, 'readerFontVal', 'px'); }
    if (lhEl) { lhEl.value = r.lineHeight || 150; updateSlider(lhEl, 'readerLineVal', '%'); }
    if (mgEl) { mgEl.value = r.margins || 20; updateSlider(mgEl, 'readerMarginVal', 'px'); }
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

document.addEventListener('DOMContentLoaded', async () => {
  await loadStatus();
  await applyFeatureFlags();
  await loadSettings();
  renderPlugins();
  setInterval(loadStatus, 30000);
});

// Handle Enter key in WiFi password
document.getElementById('wifiPassword')?.addEventListener('keypress', e => {
  if (e.key === 'Enter') connectWifi();
});

</script>
</body>
</html>
)rawliteral";

#endif // PORTAL_HTML_H
