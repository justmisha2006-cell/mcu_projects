#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Настройки точки доступа
const char* ap_ssid = "Database_AP";
const char* ap_password = "qwerty123"; // Минимум 8 символов

// Логин и пароль для входа на веб-сайт
const char* www_username = "admin";
const char* www_password = "admin";

WebServer server(80);
const char* DB_FILE = "/database.json";

// Проверка авторизации
bool checkAuth() {
  if (!server.authenticate(www_username, www_password)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// Главная HTML-страница (со встроенным CSS, JS для динамических полей, поиска и сохранения)
void handleRoot() {
  if (!checkAuth()) return;

String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>База данных ESP32 — Pro Edition v2</title>
    <style>
        :root {
            --primary: #6366f1;
            --primary-hover: #4f46e5;
            --primary-light: rgba(99, 102, 241, 0.1);
            --primary-glow: rgba(99, 102, 241, 0.25);
            --success: #10b981;
            --success-hover: #059669;
            --success-light: rgba(16, 185, 129, 0.1);
            --danger: #ef4444;
            --danger-hover: #dc2626;
            --danger-light: rgba(239, 68, 68, 0.1);
            --warning: #f59e0b;
            --warning-hover: #d97706;
            --warning-light: rgba(245, 158, 11, 0.1);
            --background: #f8fafc;
            --surface: #ffffff;
            --surface-glass: rgba(255, 255, 255, 0.85);
            --text: #0f172a;
            --text-secondary: #64748b;
            --border: #e2e8f0;
            --table-header: #f1f5f9;
            --table-hover: #f8fafc;
            --badge-bg: #f1f5f9;
            --card-shadow: 0 10px 30px -5px rgba(0, 0, 0, 0.05);
            --radius-main: 18px;
        }

        body.dark-theme {
            --background: #090d16;
            --surface: #131c2e;
            --surface-glass: rgba(19, 28, 46, 0.85);
            --text: #f8fafc;
            --text-secondary: #94a3b8;
            --border: #1e293b;
            --table-header: #182238;
            --table-hover: #1c2840;
            --badge-bg: #1e293b;
            --card-shadow: 0 10px 30px -5px rgba(0, 0, 0, 0.35);
        }

        body { 
            font-family: 'Inter', system-ui, -apple-system, sans-serif; 
            margin: 0; 
            padding: 24px; 
            background-color: var(--background); 
            color: var(--text);
            transition: background-color 0.3s, color 0.3s;
            -webkit-tap-highlight-color: transparent;
        }

        .container {
            max-width: 1360px;
            margin: 0 auto;
        }

        /* --- HEADER & BAR --- */
        .title-block {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 15px;
            margin-bottom: 24px;
            flex-wrap: wrap;
        }

        .header-title-group {
            display: flex;
            align-items: center;
            gap: 12px;
            flex-wrap: wrap;
        }

        h2 { 
            color: var(--text); 
            font-size: 24px; 
            font-weight: 800;
            margin: 0;
            letter-spacing: -0.6px;
        }

        .status-badge {
            padding: 6px 14px;
            border-radius: 30px;
            font-size: 12px;
            font-weight: 700;
            display: inline-flex;
            align-items: center;
            gap: 6px;
            transition: all 0.3s ease;
            backdrop-filter: blur(8px);
        }
        .status-online { background: rgba(16, 185, 129, 0.12); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.2); }
        .status-offline { background: rgba(239, 68, 68, 0.12); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.2); animation: pulse 1.5s infinite; }

        .btn-theme-toggle, .btn-header-action {
            background: var(--surface);
            border: 1px solid var(--border);
            color: var(--text);
            padding: 9px 16px;
            border-radius: 12px;
            font-size: 13px;
            cursor: pointer;
            font-weight: 700;
            transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
            display: inline-flex;
            align-items: center;
            gap: 8px;
            box-shadow: var(--card-shadow);
        }
        .btn-theme-toggle:hover, .btn-header-action:hover {
            border-color: var(--primary);
            color: var(--primary);
            transform: translateY(-2px);
            box-shadow: 0 8px 15px var(--primary-glow);
        }

        /* --- DASHBOARD WIDGETS --- */
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 16px;
            margin-bottom: 24px;
        }

        .dash-card {
            background: var(--surface);
            border: 1px solid var(--border);
            border-radius: var(--radius-main);
            padding: 20px;
            display: flex;
            align-items: center;
            gap: 16px;
            box-shadow: var(--card-shadow);
            transition: all 0.25s ease;
            position: relative;
            overflow: hidden;
        }
        .dash-card:hover { transform: translateY(-3px); border-color: var(--primary); box-shadow: 0 12px 25px -5px var(--primary-glow); }

        .dash-icon {
            font-size: 24px;
            width: 52px;
            height: 52px;
            border-radius: 14px;
            display: flex;
            align-items: center;
            justify-content: center;
            background: var(--primary-light);
            color: var(--primary);
            flex-shrink: 0;
        }

        .dash-info { display: flex; flex-direction: column; }
        .dash-value { font-size: 24px; font-weight: 800; color: var(--text); letter-spacing: -0.5px; line-height: 1.1; }
        .dash-label { font-size: 11px; color: var(--text-secondary); font-weight: 700; text-transform: uppercase; letter-spacing: 0.6px; margin-top: 4px; }

        /* --- CONTROLS & MULTI-FILTER --- */
        .controls-wrapper {
            display: flex;
            flex-direction: column;
            gap: 16px;
            margin-bottom: 20px;
        }

        .actions-buttons-group {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
        }

        .filter-tags {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            align-items: center;
            background: var(--surface);
            padding: 12px 16px;
            border-radius: var(--radius-main);
            border: 1px solid var(--border);
            box-shadow: var(--card-shadow);
        }

        .filter-tag {
            font-size: 12px;
            padding: 8px 14px;
            background: var(--background);
            color: var(--text-secondary);
            border: 1px solid var(--border);
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.2s;
            font-weight: 700;
            user-select: none;
        }
        .filter-tag:hover { border-color: var(--primary); color: var(--primary); }
        .filter-tag.active { background: var(--primary); color: white; border-color: var(--primary); box-shadow: 0 4px 10px var(--primary-glow); }

        .filter-select {
            font-size: 12px;
            padding: 8px 14px;
            background: var(--background);
            color: var(--text-secondary);
            border: 1px solid var(--border);
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.2s;
            font-weight: 700;
            outline: none;
        }
        .filter-select.active { background: var(--primary); color: white; border-color: var(--primary); }

        /* --- SEARCH BAR --- */
        .search-container {
            position: relative;
            width: 100%;
            display: flex;
            align-items: center;
            background: var(--background);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 2px 12px;
            box-sizing: border-box;
            transition: all 0.2s ease-in-out;
        }

        .search-container:focus-within {
            border-color: var(--primary);
            box-shadow: 0 0 0 3px var(--primary-light);
            background: var(--surface);
        }

        .search-icon {
            font-size: 14px;
            color: var(--text-secondary);
            margin-right: 8px;
            user-select: none;
        }

        .search-box { 
            width: 100%;
            padding: 9px 0;
            font-size: 13px; 
            font-weight: 600;
            border: none; 
            outline: none;
            background: transparent;
            color: var(--text);
        }

        .search-clear-btn {
            background: none;
            border: none;
            color: var(--text-secondary);
            font-size: 14px;
            cursor: pointer;
            display: none;
            padding: 2px 6px;
            border-radius: 50%;
            line-height: 1;
            transition: color 0.2s;
        }
        .search-clear-btn:hover { color: var(--danger); }

        .search-counter {
            font-size: 11px;
            font-weight: 700;
            color: var(--primary);
            background: var(--primary-light);
            padding: 2px 8px;
            border-radius: 12px;
            margin-right: 6px;
            display: none;
        }

        mark { background: rgba(245, 158, 11, 0.35); color: inherit; padding: 1px 4px; border-radius: 4px; }

        /* --- BUTTONS --- */
        .btn { 
            padding: 10px 18px; 
            border: none; 
            border-radius: 12px; 
            cursor: pointer; 
            font-weight: 700; 
            font-size: 13px; 
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            transition: all 0.2s ease;
            height: 42px;
            box-sizing: border-box;
        }
        .btn-main-excel { background-color: var(--success-light); color: var(--success); border: 1px solid rgba(16, 185, 129, 0.2); }
        .btn-main-excel:hover { background-color: var(--success); color: white; transform: translateY(-1px); box-shadow: 0 4px 12px rgba(16, 185, 129, 0.3); }
        
        .btn-main-backup { background-color: var(--warning-light); color: var(--warning); border: 1px solid rgba(245, 158, 11, 0.2); }
        .btn-main-backup:hover { background-color: var(--warning); color: white; transform: translateY(-1px); box-shadow: 0 4px 12px rgba(245, 158, 11, 0.3); }

        .btn-main-log { background-color: var(--primary-light); color: var(--primary); border: 1px solid rgba(99, 102, 241, 0.2); }
        .btn-main-log:hover { background-color: var(--primary); color: white; transform: translateY(-1px); box-shadow: 0 4px 12px var(--primary-glow); }

        .btn-main-del { background-color: var(--danger-light); color: var(--danger); border: 1px solid rgba(239, 68, 68, 0.2); }
        .btn-main-del:hover { background-color: var(--danger); color: white; transform: translateY(-1px); box-shadow: 0 4px 12px rgba(239, 68, 68, 0.3); }

        .btn-main-add { background-color: var(--primary); color: white; box-shadow: 0 4px 12px var(--primary-glow); }
        .btn-main-add:hover { background-color: var(--primary-hover); transform: translateY(-1px); box-shadow: 0 6px 16px var(--primary-glow); }

        .btn-submit { background-color: var(--success); color: white; width: 100%; padding: 12px; margin-top: 20px; font-size: 15px; height: auto; }
        .btn-submit:hover { background-color: var(--success-hover); }
        
        .btn-danger-submit { background-color: var(--danger); color: white; width: 100%; padding: 12px; margin-top: 15px; font-size: 14px; height: auto; }
        .btn-danger-submit:hover { background-color: var(--danger-hover); }

        .btn-close { background-color: transparent; color: var(--text-secondary); width: 100%; padding: 8px; margin-top: 6px; height: auto; }
        .btn-close:hover { color: var(--text); background: var(--border); }

        /* MODERN TABLE ACTIONS */
        .action-buttons-cell { display: flex; gap: 8px; align-items: center; justify-content: center; }
        .btn-action-icon {
            background: var(--background);
            border: 1px solid var(--border);
            border-radius: 10px;
            width: 34px;
            height: 34px;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
            color: var(--text-secondary);
            display: inline-flex;
            align-items: center;
            justify-content: center;
        }
        .btn-action-edit:hover { 
            background-color: var(--primary); 
            border-color: var(--primary); 
            color: white; 
            transform: scale(1.08); 
            box-shadow: 0 4px 10px var(--primary-glow);
        }
        .btn-action-delete:hover { 
            background-color: var(--danger); 
            border-color: var(--danger); 
            color: white; 
            transform: scale(1.08); 
            box-shadow: 0 4px 10px rgba(239, 68, 68, 0.3);
        }

        /* --- TABLE STYLES --- */
        .table-relative-wrapper { position: relative; min-height: 200px; }
        .spinner-overlay {
            position: absolute;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(255, 255, 255, 0.7);
            display: none;
            align-items: center;
            justify-content: center;
            z-index: 10;
            border-radius: var(--radius-main);
            backdrop-filter: blur(4px);
        }
        body.dark-theme .spinner-overlay { background: rgba(9, 13, 22, 0.7); }
        .spinner {
            width: 42px; height: 42px;
            border: 3px solid var(--border);
            border-top: 3px solid var(--primary);
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
        }

        .table-container {
            background: var(--surface); 
            box-shadow: var(--card-shadow); 
            border-radius: var(--radius-main); 
            overflow: hidden;
            border: 1px solid var(--border);
            max-height: 62vh;
            overflow-y: auto;
            transition: all 0.2s;
        }

        table { width: 100%; border-collapse: separate; border-spacing: 0; text-align: left; }
        
        th { 
            background-color: var(--table-header); 
            color: var(--text-secondary); 
            font-weight: 700; 
            cursor: pointer; 
            user-select: none; 
            position: sticky; 
            top: 0; 
            z-index: 2; 
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.8px;
            padding: 16px 20px;
            border-bottom: 1px solid var(--border);
            transition: background-color 0.2s;
        }
        th:hover { background-color: var(--border); color: var(--text); }
        th.sort-asc::after { content: " ↑"; color: var(--primary); font-weight: bold; }
        th.sort-desc::after { content: " ↓"; color: var(--primary); font-weight: bold; }
        
        td { 
            padding: 14px 20px; 
            border-bottom: 1px solid var(--border); 
            font-size: 14px;
            vertical-align: middle;
        }

        /* Compact Table Mode Class */
        .table-compact td { padding: 8px 16px; font-size: 13px; }
        .table-compact th { padding: 10px 16px; }

        tr:last-child td { border-bottom: none; }
        tr { transition: background-color 0.15s ease; }
        tr:hover { background-color: var(--table-hover); }

        /* Badges for Data Elements inside Table */
        .flat-badge {
            font-weight: 800;
            font-size: 14px;
            color: var(--primary);
            background: var(--primary-light);
            padding: 6px 12px;
            border-radius: 10px;
            display: inline-block;
            border: 1px solid rgba(99, 102, 241, 0.2);
            letter-spacing: -0.2px;
        }

        .resident-name {
            font-weight: 600;
            color: var(--text);
        }

        .table-pills-list {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            margin: 0;
            padding: 0;
            list-style: none;
        }

        /* License Plate Styling */
        .car-pill {
            background: var(--badge-bg);
            color: var(--text);
            font-family: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, monospace;
            font-size: 12px;
            font-weight: 700;
            padding: 4px 10px;
            border-radius: 8px;
            border: 1px solid var(--border);
            letter-spacing: 0.5px;
            display: inline-flex;
            align-items: center;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        .car-pill:hover {
            border-color: var(--primary);
            color: var(--primary);
            background: var(--surface);
            transform: translateY(-1px);
        }

        .phone-pill {
            background: var(--badge-bg);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 4px 8px;
            display: inline-flex;
            align-items: center;
            gap: 6px;
            font-size: 13px;
            transition: all 0.2s ease;
        }
        .phone-pill:hover {
            border-color: var(--primary);
            background: var(--surface);
            transform: translateY(-1px);
        }

        .phone-link {
            color: var(--text);
            text-decoration: none;
            font-weight: 600;
        }
        .phone-link:hover { color: var(--primary); }
        .wa-link { 
            text-decoration: none; 
            font-size: 13px; 
            line-height: 1;
            transition: transform 0.2s;
        }
        .wa-link:hover { transform: scale(1.25); }

        .empty-placeholder {
            color: var(--text-secondary);
            font-style: italic;
            font-size: 13px;
            opacity: 0.6;
        }

        /* --- PAGINATION & FOOTER --- */
        .pagination-container {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-top: 18px;
            padding: 0 4px;
            flex-wrap: wrap;
            gap: 12px;
        }
        .pagination-info { font-size: 13px; color: var(--text-secondary); font-weight: 600; }
        .pagination-controls { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
        .rows-per-page {
            padding: 6px 12px;
            border-radius: 10px;
            border: 1px solid var(--border);
            background: var(--surface);
            color: var(--text);
            font-size: 12px;
            font-weight: 700;
            outline: none;
            cursor: pointer;
        }
        .pagination-buttons { display: flex; gap: 6px; }
        .page-btn {
            padding: 6px 12px;
            border: 1px solid var(--border);
            background: var(--surface);
            border-radius: 8px;
            cursor: pointer;
            font-weight: 700;
            font-size: 13px;
            color: var(--text-secondary);
            transition: all 0.2s;
        }
        .page-btn:hover { border-color: var(--primary); color: var(--primary); }
        .page-btn.active { background: var(--primary); color: white; border-color: var(--primary); box-shadow: 0 2px 8px var(--primary-glow); }
        .page-btn:disabled { opacity: 0.4; cursor: not-allowed; }

        /* --- MODAL BASE --- */
        .modal { 
            display: none; 
            position: fixed; 
            z-index: 1000; 
            left: 0; top: 0; 
            width: 100%; height: 100%; 
            background-color: rgba(15, 23, 42, 0.65); 
            backdrop-filter: blur(8px);
            align-items: center; 
            justify-content: center; 
            opacity: 0;
            transition: opacity 0.25s cubic-bezier(0.4, 0, 0.2, 1);
        }
        .modal.show { display: flex; opacity: 1; }
        
        .modal-content { 
            background-color: var(--surface); 
            color: var(--text);
            padding: 24px; 
            border-radius: 24px; 
            width: 100%; 
            max-width: 520px; 
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.3); 
            box-sizing: border-box; 
            max-height: 90vh; 
            overflow-y: auto; 
            transform: translateY(20px) scale(0.97);
            transition: transform 0.25s cubic-bezier(0.4, 0, 0.2, 1);
        }
        .modal.show .modal-content { transform: translateY(0) scale(1); }
        .modal h3 { margin-top: 0; font-size: 20px; font-weight: 800; margin-bottom: 15px; letter-spacing: -0.3px; }

        /* --- MODERN ADD/EDIT MODAL STYLES --- */
        .modal-header-modern {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 20px;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border);
        }
        .modal-header-modern h3 {
            margin: 0;
            font-size: 20px;
            font-weight: 800;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .modal-close-icon {
            background: var(--background);
            border: 1px solid var(--border);
            color: var(--text-secondary);
            width: 32px;
            height: 32px;
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.2s ease;
        }
        .modal-close-icon:hover {
            background: rgba(239, 68, 68, 0.1);
            color: var(--danger);
            border-color: rgba(239, 68, 68, 0.3);
        }

        .form-section-card {
            background: var(--background);
            border: 1px solid var(--border);
            border-radius: 16px;
            padding: 16px;
            margin-bottom: 16px;
            transition: border-color 0.2s ease;
        }
        .form-section-card:focus-within {
            border-color: var(--primary);
        }
        .form-section-title {
            font-size: 11px;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 0.6px;
            color: var(--text-secondary);
            margin-bottom: 10px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .flat-input-wrapper {
            position: relative;
            display: flex;
            align-items: center;
        }
        .flat-input-wrapper input {
            font-size: 16px !important;
            font-weight: 800 !important;
            letter-spacing: 0.5px;
            padding-left: 14px !important;
        }

        /* Modern Checkbox Pills */
        .pill-group-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
        }
        .pill-checkbox-label {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
            padding: 8px;
            background: var(--surface);
            border: 1px solid var(--border);
            border-radius: 10px;
            font-size: 12px;
            font-weight: 700;
            color: var(--text-secondary);
            cursor: pointer;
            user-select: none;
            transition: all 0.2s ease;
        }
        .pill-checkbox-label input { display: none; }
        .pill-checkbox-label:hover {
            border-color: var(--primary);
            color: var(--primary);
        }
        .pill-checkbox-label:has(input:checked) {
            background: rgba(99, 102, 241, 0.12);
            border-color: var(--primary);
            color: var(--primary);
        }

        /* Dynamic Inputs */
        .dynamic-item-modern {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-bottom: 8px;
        }
        .dynamic-item-modern:last-child {
            margin-bottom: 0;
        }
        .btn-inline-add {
            background: transparent;
            color: var(--primary);
            border: 1px dashed var(--primary);
            border-radius: 10px;
            padding: 9px 12px;
            font-size: 12px;
            font-weight: 700;
            cursor: pointer;
            width: 100%;
            margin-top: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
            transition: all 0.2s;
        }
        .btn-inline-add:hover {
            background: var(--primary-light);
            border-style: solid;
        }
        .btn-icon-del {
            background: rgba(239, 68, 68, 0.08);
            border: 1px solid rgba(239, 68, 68, 0.2);
            color: var(--danger);
            width: 38px;
            height: 38px;
            border-radius: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            flex-shrink: 0;
            transition: all 0.2s;
        }
        .btn-icon-del:hover {
            background: var(--danger);
            color: white;
        }

        .modal-actions-group {
            display: flex;
            gap: 10px;
            margin-top: 20px;
        }
        .btn-cta-submit {
            background: linear-gradient(135deg, var(--primary), var(--primary-hover));
            color: white;
            flex: 2;
            padding: 12px;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 700;
            box-shadow: 0 4px 12px var(--primary-glow);
            transition: all 0.2s ease;
        }
        .btn-cta-submit:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 16px var(--primary-glow);
        }
        .btn-modal-cancel {
            background: var(--background);
            border: 1px solid var(--border);
            color: var(--text-secondary);
            flex: 1;
            padding: 12px;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 700;
        }
        .btn-modal-cancel:hover {
            background: var(--border);
            color: var(--text);
        }

        /* Base input styles */
        .form-group { margin-bottom: 16px; }
        label { display: block; font-weight: 700; margin-bottom: 6px; font-size: 12px; color: var(--text-secondary); }
        
        input[type="text"], input[type="password"], input[type="file"] { 
            width: 100%; 
            padding: 10px 14px; 
            border: 1px solid var(--border); 
            border-radius: 10px; 
            box-sizing: border-box; 
            font-size: 14px;
            outline: none;
            background: var(--surface);
            color: var(--text);
            transition: border-color 0.2s, box-shadow 0.2s;
        }
        input[type="text"]:focus, input[type="password"]:focus { border-color: var(--primary); box-shadow: 0 0 0 3px var(--primary-light); }
        input.input-duplicate-warning { border-color: var(--warning) !important; background-color: rgba(245, 158, 11, 0.05); }

        .checkbox-label { display: flex; align-items: center; gap: 8px; font-size: 12px; cursor: pointer; margin: 0; user-select: none; }
        .checkbox-label input { width: 16px; height: 16px; accent-color: var(--primary); }

        /* --- DELETE LIST STYLE --- */
        .delete-list-wrapper {
            border: 1px solid var(--border);
            border-radius: 12px;
            background: var(--background);
            overflow: hidden;
        }

        .delete-list-toolbar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 8px 12px;
            background: var(--table-header);
            border-bottom: 1px solid var(--border);
            font-size: 12px;
        }

        .delete-list-container {
            max-height: 220px;
            overflow-y: auto;
            padding: 6px;
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .delete-item-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 8px 12px;
            border-radius: 8px;
            background: var(--surface);
            border: 1px solid var(--border);
            transition: all 0.15s ease;
        }
        .delete-item-row:hover {
            border-color: var(--primary);
            background: var(--primary-light);
        }

        .dup-hint {
            font-size: 11px;
            color: var(--warning-hover);
            margin-top: 4px;
            display: none;
            font-weight: 700;
        }

        /* --- TOAST NOTIFICATIONS --- */
        .toast-container { position: fixed; bottom: 20px; right: 20px; z-index: 9999; display: flex; flex-direction: column; gap: 8px; }
        .toast {
            background: #1e293b; color: white; padding: 12px 20px; border-radius: 12px; font-size: 13px; font-weight: 600;
            display: flex; align-items: center; gap: 10px; transform: translateY(15px); opacity: 0; transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
            box-shadow: 0 10px 25px rgba(0,0,0,0.25);
            backdrop-filter: blur(8px);
        }
        .toast.show { transform: translateY(0); opacity: 1; }
        .toast-success { border-left: 4px solid var(--success); }
        .toast-error { border-left: 4px solid var(--danger); }
        .toast-warning { border-left: 4px solid var(--warning); }

        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }

        @media screen and (max-width: 768px) {
            body { padding: 15px; }
            .dashboard-grid { grid-template-columns: 1fr 1fr; }
            table, thead, tbody, th, td, tr { display: block; }
            thead { display: none; }
            tr { margin-bottom: 12px; border: 1px solid var(--border); border-radius: 16px; background: var(--surface); padding: 12px 16px; box-shadow: var(--card-shadow); }
            td { border-bottom: 1px solid var(--border); padding: 8px 0; display: flex; flex-direction: column; text-align: left !important; }
            td:last-child { border-bottom: none; }
            td::before { content: attr(data-label); font-size: 10px; text-transform: uppercase; color: var(--text-secondary); margin-bottom: 4px; font-weight: 800; letter-spacing: 0.5px; }
            .action-buttons-cell { justify-content: flex-start; }
            .pill-group-grid { grid-template-columns: repeat(2, 1fr); }
            .search-container { max-width: 100% !important; margin-left: 0 !important; }
        }
    </style>
</head>
<body>

    <div class="container">
        <!-- HEADER -->
        <div class="title-block">
            <div class="header-title-group">
                <h2>База жильцов</h2>
                <div id="conn-status" class="status-badge status-online">● Онлайн</div>
                <div id="wifi-rssi" class="status-badge" style="background:var(--table-header); border:1px solid var(--border);">📶 -- dBm</div>
            </div>
            <div style="display:flex; gap:8px;">
                <button class="btn-header-action" onclick="triggerGateOpen()" title="Импульс открытия">🚪 Открыть ворота</button>
                <button class="btn-theme-toggle" id="theme-btn-text" onclick="toggleTheme()">🌙 Тёмная</button>
            </div>
        </div>

        <!-- DASHBOARD WIDGETS -->
        <div class="dashboard-grid">
            <div class="dash-card">
                <div class="dash-icon">🏢</div>
                <div class="dash-info">
                    <span class="dash-value" id="dash-total-flats">0</span>
                    <span class="dash-label">Всего квартир</span>
                </div>
            </div>
            <div class="dash-card">
                <div class="dash-icon">🚗</div>
                <div class="dash-info">
                    <span class="dash-value" id="dash-total-cars">0</span>
                    <span class="dash-label">Машин в базе</span>
                </div>
            </div>
            <div class="dash-card">
                <div class="dash-icon">📞</div>
                <div class="dash-info">
                    <span class="dash-value" id="dash-total-phones">0</span>
                    <span class="dash-label">Телефонов</span>
                </div>
            </div>
            <div class="dash-card">
                <div class="dash-icon">💾</div>
                <div class="dash-info">
                    <span class="dash-value" id="dash-free-mem">-- КБ</span>
                    <span class="dash-label">Свободно RAM</span>
                </div>
            </div>
        </div>

        <!-- CONTROLS & ACTIONS -->
        <div class="controls-wrapper">
            <div class="actions-buttons-group">
                <button class="btn btn-main-excel" onclick="exportToCSV()">📊 Экспорт CSV</button>
                <button class="btn btn-main-backup" onclick="openBackupModal()">💾 Бэкап / Импорт</button>
                <button class="btn btn-main-log" onclick="openLogModal()">📋 Журнал событий</button>
                <button class="btn btn-main-del" onclick="openDeleteModal()">✕ Удалить квартиры</button>
                <button class="btn btn-main-add" onclick="openModal()">+ Добавить квартиру</button>
            </div>

            <!-- MULTI-FILTER PANEL -->
            <div class="filter-tags">
                <span style="font-size:11px; font-weight:800; color:var(--text-secondary); text-transform:uppercase; letter-spacing:0.5px; margin-right:2px;">Фильтры:</span>
                <button class="filter-tag active" id="f-all" onclick="resetFilters()">Все</button>
                
                <select id="f-entrance" class="filter-select" onchange="applyMultiFilter()">
                    <option value="">🚪 Подъезд: Все</option>
                    <option value="pod1">Подъезд 1</option>
                    <option value="pod2">Подъезд 2</option>
                    <option value="pod3">Подъезд 3</option>
                    <option value="pod4">Подъезд 4</option>
                    <option value="pod5">Подъезд 5</option>
                    <option value="pod6">Подъезд 6</option>
                </select>

                <button class="filter-tag" id="f-nocars" onclick="toggleTagFilter('nocars')">Без машин</button>
                <button class="filter-tag" id="f-nophones" onclick="toggleTagFilter('nophones')">Без телефонов</button>
                <button class="filter-tag" id="f-multicars" onclick="toggleTagFilter('multicars')">🚘 2+ Машины</button>
                <button class="filter-tag" id="f-duplicates" onclick="toggleTagFilter('duplicates')">⚠️ Дубликаты</button>

                <!-- SEARCH BAR -->
                <div class="search-container" style="max-width: 340px; margin-left: auto;">
                    <span class="search-icon">🔍</span>
                    <input type="text" id="search" class="search-box" placeholder="Поиск (Ctrl+F)..." oninput="debouncedSearch()">
                    <span id="search-counter" class="search-counter">0</span>
                    <button id="search-clear" class="search-clear-btn" onclick="clearSearch()">✕</button>
                </div>
            </div>
        </div>

        <!-- MODERN TABLE -->
        <div class="table-relative-wrapper">
            <div id="loading-overlay" class="spinner-overlay"><div class="spinner"></div></div>
            
            <div class="table-container" id="table-container-box">
                <table>
                    <thead>
                        <tr>
                            <th onclick="sortTable(0, true)" style="width: 140px;">Квартира</th>
                            <th onclick="sortTable(1)">Жильцы</th>
                            <th onclick="sortTable(2)">Машины</th>
                            <th onclick="sortTable(3)">Телефоны</th>
                            <th style="width: 100px; text-align: center; cursor: default;">Действия</th>
                        </tr>
                    </thead>
                    <tbody id="db-table-body"></tbody>
                </table>
            </div>
        </div>

        <!-- PAGINATION & ROW LIMIT -->
        <div class="pagination-container">
            <div id="pagination-info" class="pagination-info">Показано 0-0 из 0</div>
            <div class="pagination-controls">
                <button class="page-btn" id="compact-toggle-btn" onclick="toggleCompactMode()" title="Переключить плотность таблицы">↔ Компактный вид</button>
                <label style="font-size:12px; color:var(--text-secondary); font-weight:700;">На странице:</label>
                <select id="rows-per-page" class="rows-per-page" onchange="changeRowsPerPage(this.value)">
                    <option value="10" selected>10</option>
                    <option value="25">25</option>
                    <option value="50">50</option>
                    <option value="100">100</option>
                </select>
                <div id="pagination-buttons" class="pagination-buttons"></div>
            </div>
        </div>
    </div>

    <!-- MODAL: SECURITY PIN CODE -->
    <div id="pinModal" class="modal">
        <div class="modal-content" style="max-width:380px;">
            <h3>🔒 Подтверждение PIN</h3>
            <p style="font-size:13px; color:var(--text-secondary);">Введите PIN-код администратора для завершения операции:</p>
            <div class="form-group">
                <input type="password" id="admin-pin-input" placeholder="****" maxlength="8" style="text-align:center; font-size:20px; letter-spacing:4px;">
            </div>
            <button class="btn btn-submit" onclick="confirmPinAction()">Подтвердить</button>
            <button class="btn btn-close" onclick="closeModal('pinModal')">Отмена</button>
        </div>
    </div>

    <!-- MODAL: ADD RECORD -->
    <div id="userModal" class="modal">
        <div class="modal-content">
            <div class="modal-header-modern">
                <h3>🏠 Добавить квартиру</h3>
                <button class="modal-close-icon" onclick="closeModal('userModal')">✕</button>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">Номер квартиры</div>
                <div class="flat-input-wrapper">
                    <input type="text" id="flat" placeholder="кв. 415" onblur="validateInputDuplicate(this, 'flat')">
                </div>
                <div class="dup-hint" id="flat-dup-hint"></div>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">Привязка к подъезду</div>
                <div class="pill-group-grid">
                    <label class="pill-checkbox-label"><input type="checkbox" id="add-flag-pod1"> 🚪 Подъезд 1</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="add-flag-pod2"> 🚪 Подъезд 2</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="add-flag-pod3"> 🚪 Подъезд 3</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="add-flag-pod4"> 🚪 Подъезд 4</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="add-flag-pod5"> 🚪 Подъезд 5</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="add-flag-pod6"> 🚪 Подъезд 6</label>
                </div>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">👤 Жильцы</div>
                <div id="names-container">
                    <div class="dynamic-item-modern">
                        <input type="text" class="name-input" placeholder="Иван Иванов">
                    </div>
                </div>
                <button class="btn-inline-add" onclick="addField('names-container', 'name-input', 'Имя Фамилия')">
                    <span>+</span> Добавить жильца
                </button>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">🚗 Номера машин</div>
                <div id="cars-container">
                    <div class="dynamic-item-modern">
                        <input type="text" class="car-input" placeholder="AA 1234 BB" oninput="formatCarInput(this)" onblur="validateInputDuplicate(this, 'car')">
                    </div>
                </div>
                <button class="btn-inline-add" onclick="addField('cars-container', 'car-input', 'Номер машины')">
                    <span>+</span> Добавить машину
                </button>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">📞 Номера телефонов</div>
                <div id="phones-container">
                    <div class="dynamic-item-modern">
                        <input type="text" class="phone-input" placeholder="0931234567" oninput="formatPhoneInput(this)" onblur="validateInputDuplicate(this, 'phone')">
                    </div>
                </div>
                <button class="btn-inline-add" onclick="addField('phones-container', 'phone-input', 'Номер телефона')">
                    <span>+</span> Добавить телефон
                </button>
            </div>

            <div class="modal-actions-group">
                <button class="btn btn-modal-cancel" onclick="closeModal('userModal')">Отмена</button>
                <button class="btn btn-cta-submit" onclick="saveRecord()">Сохранить запись</button>
            </div>
        </div>
    </div>

    <!-- MODAL: EDIT RECORD -->
    <div id="editModal" class="modal">
        <div class="modal-content">
            <div class="modal-header-modern">
                <h3>✏️ Редактирование квартиры</h3>
                <button class="modal-close-icon" onclick="closeModal('editModal')">✕</button>
            </div>
            <input type="hidden" id="edit-original-flat">

            <div class="form-section-card">
                <div class="form-section-title">Номер квартиры</div>
                <input type="text" id="edit-flat" placeholder="кв. 415">
            </div>

            <div class="form-section-card">
                <div class="form-section-title">Привязка к подъезду</div>
                <div class="pill-group-grid">
                    <label class="pill-checkbox-label"><input type="checkbox" id="edit-flag-pod1"> 🚪 Подъезд 1</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="edit-flag-pod2"> 🚪 Подъезд 2</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="edit-flag-pod3"> 🚪 Подъезд 3</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="edit-flag-pod4"> 🚪 Подъезд 4</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="edit-flag-pod5"> 🚪 Подъезд 5</label>
                    <label class="pill-checkbox-label"><input type="checkbox" id="edit-flag-pod6"> 🚪 Подъезд 6</label>
                </div>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">👤 Жильцы</div>
                <div id="edit-names-container"></div>
                <button class="btn-inline-add" onclick="addField('edit-names-container', 'edit-name-input', 'Имя Фамилия')">+ Добавить жильца</button>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">🚗 Номера машин</div>
                <div id="edit-cars-container"></div>
                <button class="btn-inline-add" onclick="addField('edit-cars-container', 'edit-car-input', 'Номер машины')">+ Добавить машину</button>
            </div>

            <div class="form-section-card">
                <div class="form-section-title">📞 Номера телефонов</div>
                <div id="edit-phones-container"></div>
                <button class="btn-inline-add" onclick="addField('edit-phones-container', 'edit-phone-input', 'Номер телефона')">+ Добавить телефон</button>
            </div>

            <div class="modal-actions-group">
                <button class="btn btn-modal-cancel" onclick="closeModal('editModal')">Отмена</button>
                <button class="btn btn-cta-submit" onclick="updateRecord()">Сохранить изменения</button>
            </div>
        </div>
    </div>

    <!-- MODAL: BACKUP / RESTORE -->
    <div id="backupModal" class="modal">
        <div class="modal-content">
            <h3>Резервное копирование и Импорт</h3>
            <div class="form-group">
                <label>Скачать полный бэкап базы данных:</label>
                <button class="btn btn-main-backup" style="width:100%;" onclick="downloadBackup()">📥 Скачать JSON бэкап</button>
            </div>
            <hr style="border:0; border-top:1px solid var(--border); margin:20px 0;">
            <div class="form-group">
                <label>Импортировать из файла (JSON / CSV):</label>
                <input type="file" id="backup-file" accept=".json,.csv" onchange="validateSelectedFile(event)">
                <div id="file-validation-info" style="font-size:12px; margin-top:5px; color:var(--text-secondary); font-weight:600;"></div>
                <div style="display:flex; gap:10px; margin-top:15px;">
                    <button class="btn btn-submit" style="margin-top:0;" onclick="uploadBackup(false)">📤 Перезаписать</button>
                    <button class="btn btn-main-log" style="margin-top:0; width:100%;" onclick="uploadBackup(true)">🔄 Объединить</button>
                </div>
            </div>
            <hr style="border:0; border-top:1px solid var(--border); margin:20px 0;">
            <div class="form-group">
                <label style="color:var(--danger)">Опасная зона:</label>
                <button class="btn" style="background-color:rgba(239,68,68,0.1); color:var(--danger); border:1px solid var(--danger); width:100%;" onclick="requestPinProtectedAction('wipe')">🗑️ Полностью очистить базу данных</button>
            </div>
            <button class="btn btn-close" onclick="closeModal('backupModal')">Закрыть</button>
        </div>
    </div>

    <!-- MODAL: LOGS -->
    <div id="logModal" class="modal">
        <div class="modal-content" style="max-width: 650px;">
            <h3>Журнал событий ESP32</h3>
            <div class="form-group">
                <div id="log-container" style="background:#090d16; color:#38bdf8; font-family:'SFMono-Regular', Consolas, monospace; padding:15px; border-radius:14px; max-height:350px; overflow-y:auto; font-size:12px; line-height:1.5; white-space:pre-wrap; border:1px solid var(--border);">Загрузка журналов...</div>
            </div>
            <div style="display:flex; gap:10px; margin-bottom:10px;">
                <button class="btn btn-main-log" style="flex:1;" onclick="downloadLogsText()">📥 Скачать логи (.txt)</button>
                <button class="btn btn-main-del" style="flex:1;" onclick="requestClearLogsAction()">🗑️ Очистить логи</button>
            </div>
            <button class="btn btn-close" onclick="closeModal('logModal')">Закрыть</button>
        </div>
    </div>

    <!-- MODAL: DELETE FLATS -->
    <div id="deleteModal" class="modal">
        <div class="modal-content">
            <h3>Удаление квартир</h3>
            <div class="form-group">
                <div class="search-container" style="margin-bottom:10px;">
                    <span class="search-icon">🔍</span>
                    <input type="text" id="delete-search" class="search-box" placeholder="Поиск квартиры..." oninput="filterDeleteList()">
                </div>
            </div>
            
            <div class="delete-list-wrapper">
                <div class="delete-list-toolbar">
                    <label class="checkbox-label">
                        <input type="checkbox" id="select-all-del" onchange="toggleSelectAllDelete(this)"> Выбрать все
                    </label>
                    <span id="del-selected-count" style="color:var(--text-secondary); font-weight:700;">Выбрано: 0</span>
                </div>
                <div id="delete-flats-list" class="delete-list-container"></div>
            </div>

            <button class="btn btn-danger-submit" onclick="deleteSelectedFlats()">Удалить выбранные</button>
            <button class="btn btn-close" onclick="closeModal('deleteModal')">Отмена</button>
        </div>
    </div>

    <div id="toast-container" class="toast-container"></div>

    <script>
        let globalData = [];
        let filteredData = [];
        let activeTags = new Set();
        let currentSortColumn = -1;
        let isAscending = true;
        let searchTimeout;
        let currentPage = 1;
        let rowsPerPage = 10;
        let pendingPinAction = null;
        let parsedBackupData = null;
        let lastEnteredPin = ""; 
        let isCompactView = false;

        /* --- THEME SYSTEM --- */
        function updateThemeButtonUI(isDark) {
            const btn = document.getElementById('theme-btn-text');
            if (btn) btn.innerText = isDark ? '☀️ Светлая' : '🌙 Тёмная';
        }

        function toggleTheme() {
            document.body.classList.toggle('dark-theme');
            const isDark = document.body.classList.contains('dark-theme');
            localStorage.setItem('theme', isDark ? 'dark' : 'light');
            updateThemeButtonUI(isDark);
            showToast(isDark ? 'Тёмная тема включена' : 'Светлая тема включена', 'success');
        }

        function initTheme() {
            const savedTheme = localStorage.getItem('theme');
            const isDark = savedTheme === 'dark';
            if (isDark) document.body.classList.add('dark-theme');
            else document.body.classList.remove('dark-theme');
            updateThemeButtonUI(isDark);

            if (localStorage.getItem('compactView') === 'true') {
                isCompactView = true;
                document.getElementById('table-container-box').classList.add('table-compact');
            }
        }

        function toggleCompactMode() {
            isCompactView = !isCompactView;
            document.getElementById('table-container-box').classList.toggle('table-compact', isCompactView);
            localStorage.setItem('compactView', isCompactView);
            showToast(isCompactView ? 'Компактный вид включен' : 'Обычный вид таблицы', 'success');
        }

        function toggleLoader(show) {
            document.getElementById('loading-overlay').style.display = show ? 'flex' : 'none';
        }

        function showToast(message, type = 'success') {
            const container = document.getElementById('toast-container');
            const toast = document.createElement('div');
            let icon = type === 'success' ? '✅' : (type === 'error' ? '❌' : '⚠️');
            toast.className = `toast toast-${type}`;
            toast.innerHTML = `<span>${icon}</span> <span>${message}</span>`;
            container.appendChild(toast);
            setTimeout(() => toast.classList.add('show'), 10);
            setTimeout(() => {
                toast.classList.remove('show');
                setTimeout(() => toast.remove(), 300);
            }, 3500);
        }

        function copyToClipboard(text, label) {
            navigator.clipboard.writeText(text).then(() => {
                showToast(`${label} скопирован!`, 'success');
            }).catch(() => {
                showToast('Ошибка копирования', 'error');
            });
        }

        /* --- NETWORK & GATE ACTION --- */
        setInterval(() => {
            fetch('/get_data', { method: 'HEAD' })
                .then(() => {
                    const badge = document.getElementById('conn-status');
                    badge.className = "status-badge status-online";
                    badge.innerText = "● Онлайн";
                })
                .catch(() => {
                    const badge = document.getElementById('conn-status');
                    badge.className = "status-badge status-offline";
                    badge.innerText = "● Офлайн";
                });
        }, 5000);

        function triggerGateOpen() {
            showToast("Отправка команды открытия...", "warning");
            fetch('/open_gate', { method: 'POST' })
                .then(res => res.ok ? showToast("🚪 Ворота открыты!", "success") : showToast("Ошибка реле", "error"))
                .catch(() => showToast("Ошибка связи с ESP32", "error"));
        }

        /* --- DASHBOARD STATS CALCULATOR --- */
        function updateDashboardStats() {
            let totalCars = 0;
            let totalPhones = 0;

            globalData.forEach(item => {
                if (item.cars) totalCars += item.cars.filter(c => c.trim()).length;
                if (item.phones) totalPhones += item.phones.filter(p => p.trim()).length;
            });

            document.getElementById('dash-total-flats').innerText = globalData.length;
            document.getElementById('dash-total-cars').innerText = totalCars;
            document.getElementById('dash-total-phones').innerText = totalPhones;
        }

        /* --- MULTI-FILTER & SEARCH LOGIC --- */
        function toggleTagFilter(tag) {
            let btn = document.getElementById(`f-${tag}`);
            if (activeTags.has(tag)) {
                activeTags.delete(tag);
                btn.classList.remove('active');
            } else {
                activeTags.add(tag);
                btn.classList.add('active');
            }
            document.getElementById('f-all').classList.remove('active');
            applyMultiFilter();
        }

        function resetFilters() {
            activeTags.clear();
            document.querySelectorAll('.filter-tag').forEach(b => b.classList.remove('active'));
            document.getElementById('f-all').classList.add('active');
            document.getElementById('f-entrance').value = "";
            document.getElementById('f-entrance').classList.remove('active');
            applyMultiFilter();
        }

        function cleanStr(str) {
            return (str || '').toString().toLowerCase().replace(/[\s\-\(\)\._]/g, '');
        }

        function applyMultiFilter() {
            let entranceVal = document.getElementById('f-entrance').value;
            let selectElem = document.getElementById('f-entrance');
            if (entranceVal) selectElem.classList.add('active'); else selectElem.classList.remove('active');

            let rawQuery = document.getElementById('search').value.trim();
            let normQuery = cleanStr(rawQuery);

            filteredData = globalData.filter(item => {
                let nameStr = Array.isArray(item.name) ? item.name.join(' ') : (item.name || '');
                let validCars = item.cars ? item.cars.filter(c => c && c.trim() !== "") : [];
                let validPhones = item.phones ? item.phones.filter(p => p && p.trim() !== "") : [];

                let searchableCombined = [item.flat, nameStr, ...validCars, ...validPhones].map(cleanStr).join(' ');
                let matchesSearch = !normQuery || searchableCombined.includes(normQuery);

                if (!matchesSearch) return false;

                let customFlags = item.flags || [];
                
                if (entranceVal && !customFlags.includes(entranceVal)) return false;
                if (activeTags.has('nocars') && validCars.length > 0) return false;
                if (activeTags.has('nophones') && validPhones.length > 0) return false;
                if (activeTags.has('multicars') && validCars.length < 2) return false;

                if (activeTags.has('duplicates')) {
                    let hasDupCar = validCars.some(car => {
                        let cleanCar = cleanStr(car);
                        return cleanCar && globalData.some(other => cleanStr(other.flat) !== cleanStr(item.flat) && other.cars && other.cars.some(oc => cleanStr(oc) === cleanCar));
                    });
                    let hasDupPhone = validPhones.some(phone => {
                        let cleanPhone = cleanStr(phone);
                        return cleanPhone && globalData.some(other => cleanStr(other.flat) !== cleanStr(item.flat) && other.phones && other.phones.some(op => cleanStr(op) === cleanPhone));
                    });
                    if (!hasDupCar && !hasDupPhone && !customFlags.includes('duplicates')) return false;
                }

                return true;
            });

            // Update search counter pill
            let counterPill = document.getElementById('search-counter');
            if (normQuery.length > 0) {
                counterPill.innerText = filteredData.length;
                counterPill.style.display = 'inline-block';
            } else {
                counterPill.style.display = 'none';
            }

            currentPage = 1;
            renderTableDirect(filteredData.length, true);
        }

        /* --- DATA LOADING --- */
        function loadData() {
            toggleLoader(true);
            fetch(`/get_data?_t=${Date.now()}`)
                .then(res => res.json())
                .then(response => {
                    globalData = Array.isArray(response) ? response : (response.data || []);
                    if (response.free_bytes !== undefined) {
                        let kb = (response.free_bytes / 1024).toFixed(1);
                        document.getElementById('dash-free-mem').innerText = `${kb} КБ`;
                    }
                    if (response.rssi !== undefined) {
                        document.getElementById('wifi-rssi').innerText = `📶 ${response.rssi} dBm`;
                    }
                    updateDashboardStats();
                    applyMultiFilter();
                })
                .catch(() => showToast('Ошибка загрузки данных', 'error'))
                .finally(() => toggleLoader(false));
        }

        /* --- SORT TABLE --- */
        function sortTable(colIndex, isNumeric = false) {
            let ths = document.querySelectorAll('th');
            ths.forEach((th, idx) => {
                if (idx !== colIndex) th.classList.remove('sort-asc', 'sort-desc');
            });

            if (currentSortColumn === colIndex) {
                isAscending = !isAscending;
            } else {
                currentSortColumn = colIndex;
                isAscending = true;
            }

            ths[colIndex].classList.toggle('sort-asc', isAscending);
            ths[colIndex].classList.toggle('sort-desc', !isAscending);

            filteredData.sort((a, b) => {
                let valA = "", valB = "";
                if (colIndex === 0) { valA = a.flat || ""; valB = b.flat || ""; }
                else if (colIndex === 1) { valA = Array.isArray(a.name) ? a.name.join(' ') : (a.name || ""); valB = Array.isArray(b.name) ? b.name.join(' ') : (b.name || ""); }
                else if (colIndex === 2) { valA = (a.cars || []).join(' '); valB = (b.cars || []).join(' '); }
                else if (colIndex === 3) { valA = (a.phones || []).join(' '); valB = (b.phones || []).join(' '); }

                if (isNumeric) {
                    let numA = parseInt(valA.replace(/\D/g, '')) || 0;
                    let numB = parseInt(valB.replace(/\D/g, '')) || 0;
                    return isAscending ? numA - numB : numB - numA;
                }
                return isAscending ? valA.localeCompare(valB, 'ru') : valB.localeCompare(valA, 'ru');
            });

            renderTableDirect(filteredData.length, true);
        }

        /* --- RENDER MODERN TABLE --- */
        function highlightText(text, query) {
            if (!query) return text;
            let escapedQuery = query.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
            return text.toString().replace(new RegExp(escapedQuery, 'gi'), match => `<mark>${match}</mark>`);
        }

        function renderTableDirect(totalCount, isClientSliced = true) {
            let tbody = document.getElementById('db-table-body');
            tbody.innerHTML = '';
            let query = document.getElementById('search').value.trim();

            let displayItems = filteredData;
            if (isClientSliced) {
                let start = (currentPage - 1) * rowsPerPage;
                displayItems = filteredData.slice(start, start + rowsPerPage);
            }

            displayItems.forEach(item => {
                let tr = document.createElement('tr');
                tr.setAttribute('ondblclick', `openEditModal('${item.flat}')`);
                
                let nameStr = Array.isArray(item.name) ? item.name.join(', ') : (item.name || '');
                let validCars = item.cars ? item.cars.filter(c => c && c.trim()) : [];
                let validPhones = item.phones ? item.phones.filter(p => p && p.trim()) : [];

                // License Plate Rendering (Click to Copy)
                let carsPills = validCars.length ? 
                    `<ul class="table-pills-list">${validCars.map(c => `<li class="car-pill" onclick="copyToClipboard('${c}', 'Номер авто')" title="Кликните чтобы скопировать">${highlightText(c, query)}</li>`).join('')}</ul>` 
                    : `<span class="empty-placeholder">🚗 Авто не додано</span>`;

                // Interactive Phone Badges
                let phonesPills = validPhones.length ? 
                    `<ul class="table-pills-list">${validPhones.map(p => {
                        let clean = p.replace(/\D/g, '');
                        return `<li class="phone-pill">
                                    <a href="tel:${clean}" class="phone-link" title="Позвонить">${highlightText(p, query)}</a>
                                    <a href="https://wa.me/${clean}" target="_blank" class="wa-link" title="WhatsApp">💬</a>
                                </li>`;
                    }).join('')}</ul>` 
                   : `<span class="empty-placeholder">📞 Телефон не додано</span>`;

                tr.innerHTML = `
                    <td data-label="Квартира"><span class="flat-badge">${highlightText(item.flat, query)}</span></td>
                    <td data-label="Жильцы"><span class="resident-name">${nameStr ? highlightText(nameStr, query) : '<span class="empty-placeholder">👤 Мешканця не додано</span>'}</span></td>
                    <td data-label="Машины">${carsPills}</td>
                    <td data-label="Телефоны">${phonesPills}</td>
                    <td data-label="Действия">
                        <div class="action-buttons-cell">
                            <button class="btn-action-icon btn-action-edit" onclick="openEditModal('${item.flat}')" title="Изменить">✏️</button>
                            <button class="btn-action-icon btn-action-delete" onclick="quickDeleteSingleFlat('${item.flat}')" title="Удалить">🗑️</button>
                        </div>
                    </td>
                `;
                tbody.appendChild(tr);
            });

            renderPaginationDirect(totalCount);
        }

        function renderPaginationDirect(totalCount) {
            let start = totalCount === 0 ? 0 : (currentPage - 1) * rowsPerPage + 1;
            let end = Math.min(currentPage * rowsPerPage, totalCount);
            document.getElementById('pagination-info').innerText = `Показано ${start}-${end} из ${totalCount}`;

            let container = document.getElementById('pagination-buttons');
            container.innerHTML = '';
            let totalPages = Math.ceil(totalCount / rowsPerPage);
            if (totalPages <= 1) return;

            for (let i = 1; i <= totalPages; i++) {
                if (i === 1 || i === totalPages || (i >= currentPage - 1 && i <= currentPage + 1)) {
                    let pageBtn = document.createElement('button');
                    pageBtn.className = `page-btn ${i === currentPage ? 'active' : ''}`;
                    pageBtn.innerText = i;
                    pageBtn.onclick = () => { currentPage = i; renderTableDirect(filteredData.length, true); };
                    container.appendChild(pageBtn);
                }
            }
        }

        function changeRowsPerPage(val) {
            rowsPerPage = parseInt(val);
            currentPage = 1;
            renderTableDirect(filteredData.length, true);
        }

        /* --- BACKUP & RESTORE MODAL --- */
        function openBackupModal() {
            parsedBackupData = null;
            document.getElementById('backup-file').value = '';
            document.getElementById('file-validation-info').innerText = '';
            let modal = document.getElementById('backupModal');
            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);
        }

        function downloadBackup() {
            let dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(globalData, null, 2));
            let dlAnchorElem = document.createElement('a');
            dlAnchorElem.setAttribute("href", dataStr);
            dlAnchorElem.setAttribute("download", `esp32_backup_${Date.now()}.json`);
            dlAnchorElem.click();
        }

        function validateSelectedFile(event) {
            let file = event.target.files[0];
            if (!file) return;

            let reader = new FileReader();
            reader.onload = function(e) {
                let content = e.target.result;
                if (file.name.toLowerCase().endsWith('.csv')) {
                    try {
                        let lines = content.split(/\r\n|\n/);
                        parsedBackupData = [];
                        for (let i = 1; i < lines.length; i++) {
                            let line = lines[i].trim();
                            if (!line) continue;
                            
                            let parts = line.split(';').map(p => p.replace(/^"|"$/g, '').trim());
                            if (parts.length >= 1 && parts[0]) {
                                parsedBackupData.push({
                                    flat: formatFlatNumber(parts[0]),
                                    name: parts[1] ? parts[1].split(',').map(s=>s.trim()).filter(Boolean) : [],
                                    cars: parts[2] ? parts[2].split('\n').map(s=>cleanStr(s).toUpperCase()).filter(Boolean) : [],
                                    phones: parts[3] ? parts[3].split('\n').map(s=>s.replace(/\D/g, '')).filter(Boolean) : []
                                });
                            }
                        }
                        document.getElementById('file-validation-info').innerText = `Из CSV файла распознано: ${parsedBackupData.length} квартир`;
                    } catch(err) {
                        showToast("Ошибка считывания CSV", "error");
                    }
                } else {
                    try {
                        parsedBackupData = JSON.parse(content);
                        if (Array.isArray(parsedBackupData)) {
                            document.getElementById('file-validation-info').innerText = `Из JSON распознано: ${parsedBackupData.length} записей`;
                        } else {
                            showToast("Ошибка структуры JSON", "error");
                        }
                    } catch(err) {
                        showToast("Ошибка чтения файла", "error");
                    }
                }
            };
            reader.readAsText(file);
        }

        function uploadBackup(merge = false) {
            if (!parsedBackupData || !Array.isArray(parsedBackupData)) {
                showToast("Выберите валидный файл", "warning");
                return;
            }

            toggleLoader(true);
            let payloadData = parsedBackupData;

            if (merge) {
                let combinedMap = new Map();
                globalData.forEach(item => combinedMap.set(item.flat, item));
                parsedBackupData.forEach(item => {
                    if (combinedMap.has(item.flat)) {
                        let existing = combinedMap.get(item.flat);
                        let mergedNames = Array.from(new Set([...(existing.name || []), ...(item.name || [])]));
                        let mergedCars = Array.from(new Set([...(existing.cars || []), ...(item.cars || [])]));
                        let mergedPhones = Array.from(new Set([...(existing.phones || []), ...(item.phones || [])]));
                        let mergedFlags = Array.from(new Set([...(existing.flags || []), ...(item.flags || [])]));
                        combinedMap.set(item.flat, {
                            flat: item.flat,
                            name: mergedNames,
                            cars: mergedCars,
                            phones: mergedPhones,
                            flags: mergedFlags
                        });
                    } else {
                        combinedMap.set(item.flat, item);
                    }
                });
                payloadData = Array.from(combinedMap.values());
            }

            let endpoint = merge ? '/merge_backup' : '/restore_backup';

            fetch(endpoint, {
                method: 'POST',
                headers: { 
                    'Content-Type': 'application/json',
                    'X-Merge-Mode': merge ? 'true' : 'false'
                },
                body: JSON.stringify(payloadData)
            })
            .then(res => {
                if (!res.ok && merge) {
                    return fetch('/restore_backup', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(payloadData)
                    });
                }
                return res;
            })
            .then(res => {
                if (res.ok) {
                    closeModal('backupModal');
                    showToast(merge ? 'База объединена!' : 'База данных восстановлена!', 'success');
                    loadData();
                } else {
                    showToast('Ошибка при отправке данных', 'error');
                }
            })
            .catch(() => showToast('Ошибка сети при восстановлении', 'error'))
            .finally(() => toggleLoader(false));
        }

        /* --- LOGS MODAL & EXPORT --- */
        function openLogModal() {
            let modal = document.getElementById('logModal');
            document.getElementById('log-container').innerText = "Загрузка журналов...";
            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);

            fetch('/get_logs')
                .then(res => res.text())
                .then(text => {
                    document.getElementById('log-container').innerText = text || "Журнал пуст.";
                })
                .catch(() => {
                    document.getElementById('log-container').innerText = "Ошибка загрузки логов.";
                });
        }

        function downloadLogsText() {
            let logText = document.getElementById('log-container').innerText;
            let blob = new Blob([logText], { type: "text/plain;charset=utf-8" });
            let link = document.createElement("a");
            link.href = URL.createObjectURL(blob);
            link.download = `esp32_system_logs_${Date.now()}.txt`;
            link.click();
        }

        function requestClearLogsAction() {
            if (!confirm("Очистить журнал логов?")) return;
            requestPinProtectedAction('clear_logs');
        }

        /* --- DELETE MODAL --- */
        function openDeleteModal() {
            let modal = document.getElementById('deleteModal');
            document.getElementById('delete-search').value = '';
            document.getElementById('select-all-del').checked = false;
            filterDeleteList();
            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);
        }

        function filterDeleteList() {
            let list = document.getElementById('delete-flats-list');
            let query = document.getElementById('delete-search').value.toLowerCase();
            list.innerHTML = '';

            let filtered = globalData.filter(item => item.flat.toLowerCase().includes(query));

            if (filtered.length === 0) {
                list.innerHTML = `<div style="text-align:center; padding: 20px; color:var(--text-secondary); font-size:12px;">Ничего не найдено</div>`;
                updateDeleteCount();
                return;
            }

            filtered.forEach(item => {
                let div = document.createElement('div');
                div.className = 'delete-item-row';
                div.innerHTML = `
                    <label class="checkbox-label" style="width:100%;">
                        <input type="checkbox" class="del-flat-checkbox" value="${item.flat}" onchange="updateDeleteCount()">
                        <span style="font-weight:700;">${item.flat}</span>
                    </label>
                    <span style="font-size:11px; color:var(--text-secondary);">${Array.isArray(item.name) ? item.name.join(', ') : (item.name || 'Без жильца')}</span>
                `;
                list.appendChild(div);
            });

            updateDeleteCount();
        }

        function toggleSelectAllDelete(master) {
            document.querySelectorAll('.del-flat-checkbox').forEach(cb => {
                cb.checked = master.checked;
            });
            updateDeleteCount();
        }

        function updateDeleteCount() {
            let count = document.querySelectorAll('.del-flat-checkbox:checked').length;
            document.getElementById('del-selected-count').innerText = `Выбрано: ${count}`;
        }

        function deleteSelectedFlats() {
            let selected = Array.from(document.querySelectorAll('.del-flat-checkbox:checked')).map(cb => cb.value);
            if (selected.length === 0) {
                showToast("Выделите элементы для удаления", "warning");
                return;
            }

            if (!confirm(`Удалить выбранные квартиры (${selected.length} шт.)?`)) return;

            toggleLoader(true);
            fetch('/delete_flats', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ flats: selected })
            })
            .then(res => {
                if (res.ok) {
                    closeModal('deleteModal');
                    showToast('Записи удалены!', 'success');
                    loadData();
                } else {
                    showToast('Ошибка при удалении', 'error');
                }
            })
            .catch(() => showToast('Ошибка сети', 'error'))
            .finally(() => toggleLoader(false));
        }

        /* --- SECURITY PIN SYSTEM --- */
        function requestPinProtectedAction(actionType) {
            pendingPinAction = actionType;
            let modal = document.getElementById('pinModal');
            document.getElementById('admin-pin-input').value = '';
            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);
            setTimeout(() => document.getElementById('admin-pin-input').focus(), 100);
        }

        function confirmPinAction() {
            const pinInput = document.getElementById('admin-pin-input');
            lastEnteredPin = pinInput.value.trim();
            if (!lastEnteredPin) { showToast("Введите PIN!", "warning"); return; }

            closeModal('pinModal');
            if (pendingPinAction === 'wipe') wipeDatabase();
            if (pendingPinAction === 'clear_logs') clearLogsOnESP();
            
            pinInput.value = '';
        }

        function wipeDatabase() {
            toggleLoader(true);
            fetch('/restore_backup', { 
                method: 'POST', 
                headers: { 
                    'Content-Type': 'application/json',
                    'X-Admin-PIN': lastEnteredPin 
                }, 
                body: JSON.stringify([]) 
            })
            .then(res => { 
                if (res.status === 403) { showToast('Неверный PIN на устройстве!', 'error'); return; }
                if (res.ok) { closeModal('backupModal'); showToast('База полностью очищена!', 'danger'); loadData(); } 
            })
            .catch(() => showToast('Ошибка при очистке', 'error'))
            .finally(() => {
                toggleLoader(false);
                lastEnteredPin = "";
            });
        }

        function clearLogsOnESP() {
            fetch('/clear_logs', { 
                method: 'POST',
                headers: { 'X-Admin-PIN': lastEnteredPin } 
            })
            .then(res => { 
                if (res.status === 403) { showToast('Неверный PIN!', 'error'); return; }
                if (res.ok) { document.getElementById('log-container').innerText = "Логи очищены."; showToast("Логи стерты"); } 
            })
            .catch(() => showToast("Ошибка очистки", "error"))
            .finally(() => {
                lastEnteredPin = "";
            });
        }

        /* --- FORMATTERS & DUP CHECKERS --- */
        function formatFlatNumber(input) {
            let cleaned = (input || '').trim();
            if (!cleaned) return '';
            if (!/^кв\./i.test(cleaned)) {
                return 'кв. ' + cleaned;
            }
            return cleaned;
        }

        function formatCarInput(elem) {
            elem.value = elem.value.toUpperCase().replace(/[^A-Z0-9А-Я\s\-]/gi, '');
        }

        function formatPhoneInput(elem) {
            elem.value = elem.value.replace(/[^\d\+]/g, '');
        }

        function validateInputDuplicate(elem, type) {
            let val = elem.value.trim();
            let parent = elem.parentElement;
            let existingHint = parent.querySelector('.dup-hint');
            if (existingHint) existingHint.style.display = 'none';
            elem.classList.remove('input-duplicate-warning');

            if (!val) return;

            let cleanVal = cleanStr(val);
            let originalFlat = document.getElementById('edit-original-flat') ? document.getElementById('edit-original-flat').value : '';

            let duplicateFound = null;

            if (type === 'flat') {
                let formatted = formatFlatNumber(val);
                duplicateFound = globalData.find(item => cleanStr(item.flat) === cleanStr(formatted) && item.flat !== originalFlat);
                if (duplicateFound) {
                    elem.classList.add('input-duplicate-warning');
                    let hint = document.getElementById('flat-dup-hint');
                    hint.innerText = `⚠️ Квартира ${duplicateFound.flat} уже существует!`;
                    hint.style.display = 'block';
                } else {
                    document.getElementById('flat-dup-hint').style.display = 'none';
                }
            } else if (type === 'car') {
                duplicateFound = globalData.find(item => cleanStr(item.flat) !== cleanStr(originalFlat) && item.cars && item.cars.some(c => cleanStr(c) === cleanVal));
                if (duplicateFound) showInputWarning(elem, `Уже привязано к ${duplicateFound.flat}`);
            } else if (type === 'phone') {
                duplicateFound = globalData.find(item => cleanStr(item.flat) !== cleanStr(originalFlat) && item.phones && item.phones.some(p => cleanStr(p) === cleanVal));
                if (duplicateFound) showInputWarning(elem, `Уже принадлежит ${duplicateFound.flat}`);
            }
        }

        function showInputWarning(elem, text) {
            elem.classList.add('input-duplicate-warning');
            let parent = elem.parentElement;
            let hint = parent.querySelector('.dup-hint');
            if (!hint) {
                hint = document.createElement('div');
                hint.className = 'dup-hint';
                parent.appendChild(hint);
            }
            hint.innerText = `⚠️ ${text}`;
            hint.style.display = 'block';
        }

        /* --- ADD & EDIT ACTIONS --- */
        function openModal() {
            let modal = document.getElementById('userModal');
            document.getElementById('flat').value = 'кв. ';
            document.getElementById('flat-dup-hint').style.display = 'none';
            document.getElementById('flat').classList.remove('input-duplicate-warning');

            for (let i = 1; i <= 6; i++) {
                document.getElementById(`add-flag-pod${i}`).checked = false;
            }

            document.getElementById('names-container').innerHTML = `<div class="dynamic-item-modern"><input type="text" class="name-input" placeholder="Иван Иванов"></div>`;
            document.getElementById('cars-container').innerHTML = `<div class="dynamic-item-modern"><input type="text" class="car-input" placeholder="AA 1234 BB" oninput="formatCarInput(this)" onblur="validateInputDuplicate(this, 'car')"></div>`;
            document.getElementById('phones-container').innerHTML = `<div class="dynamic-item-modern"><input type="text" class="phone-input" placeholder="0931234567" oninput="formatPhoneInput(this)" onblur="validateInputDuplicate(this, 'phone')"></div>`;

            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);
            setTimeout(() => document.getElementById('flat').focus(), 150);
        }

        function openEditModal(flatNumber) {
            let record = globalData.find(item => item.flat === flatNumber);
            if (!record) return;

            document.getElementById('edit-original-flat').value = record.flat;
            document.getElementById('edit-flat').value = record.flat;

            let flags = record.flags || [];
            for (let i = 1; i <= 6; i++) {
                document.getElementById(`edit-flag-pod${i}`).checked = flags.includes(`pod${i}`);
            }

            let buildInputs = (containerId, classInput, arr, ph, type) => {
                let cont = document.getElementById(containerId); cont.innerHTML = '';
                (arr.length ? arr : ['']).forEach((v, idx) => {
                    let div = document.createElement('div'); div.className = 'dynamic-item-modern';
                    let eventAttr = type === 'car' ? 'oninput="formatCarInput(this)" onblur="validateInputDuplicate(this, \'car\')"' : (type === 'phone' ? 'oninput="formatPhoneInput(this)" onblur="validateInputDuplicate(this, \'phone\')"' : '');
                    div.innerHTML = `<input type="text" class="${classInput}" value="${v}" placeholder="${ph}" ${eventAttr}>${idx > 0 ? '<button class="btn-icon-del" onclick="this.parentElement.remove()">✕</button>' : ''}`;
                    cont.appendChild(div);
                });
            };

            buildInputs('edit-names-container', 'edit-name-input', Array.isArray(record.name) ? record.name : [record.name], 'Имя Фамилия', 'name');
            buildInputs('edit-cars-container', 'edit-car-input', record.cars || [], 'AA 1234 BB', 'car');
            buildInputs('edit-phones-container', 'edit-phone-input', record.phones || [], '0931234567', 'phone');

            let modal = document.getElementById('editModal');
            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);
        }

        function saveRecord() {
            let rawFlat = document.getElementById('flat').value;
            let flat = formatFlatNumber(rawFlat);
            let names = Array.from(document.querySelectorAll('.name-input')).map(i => i.value.trim()).filter(Boolean);
            let cars = Array.from(document.querySelectorAll('.car-input')).map(i => i.value.toUpperCase().trim()).filter(Boolean);
            let phones = Array.from(document.querySelectorAll('.phone-input')).map(i => i.value.trim()).filter(Boolean);

            let selectedFlags = [];
            for (let i = 1; i <= 6; i++) {
                if (document.getElementById(`add-flag-pod${i}`).checked) selectedFlags.push(`pod${i}`);
            }

            if (!flat || flat === 'кв.') { showToast('Укажите корректный номер квартиры', 'warning'); return; }

            toggleLoader(true);
            fetch('/add_data', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ flat, name: names, cars, phones, flags: selectedFlags })
            }).then(res => {
                if (res.ok) { closeModal('userModal'); showToast('Квартира успешно добавлена!'); loadData(); }
            }).finally(() => toggleLoader(false));
        }

        function updateRecord() {
            let originalFlat = document.getElementById('edit-original-flat').value;
            let flat = formatFlatNumber(document.getElementById('edit-flat').value);
            let names = Array.from(document.querySelectorAll('.edit-name-input')).map(i => i.value.trim()).filter(Boolean);
            let cars = Array.from(document.querySelectorAll('.edit-car-input')).map(i => i.value.toUpperCase().trim()).filter(Boolean);
            let phones = Array.from(document.querySelectorAll('.edit-phone-input')).map(i => i.value.trim()).filter(Boolean);

            let selectedFlags = [];
            for (let i = 1; i <= 6; i++) {
                if (document.getElementById(`edit-flag-pod${i}`).checked) selectedFlags.push(`pod${i}`);
            }

            if (!flat || flat === 'кв.') { showToast('Укажите номер квартиры', 'warning'); return; }

            toggleLoader(true);
            fetch('/edit_data', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ originalFlat, flat, name: names, cars, phones, flags: selectedFlags })
            }).then(res => {
                if (res.ok) { closeModal('editModal'); showToast('Изменения сохранены!'); loadData(); }
            }).finally(() => toggleLoader(false));
        }

        function quickDeleteSingleFlat(flatNumber) {
            if (!confirm(`Удалить ${flatNumber}?`)) return;
            toggleLoader(true);
            fetch('/delete_flats', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ flats: [flatNumber] })
            }).then(res => { if (res.ok) { showToast('Удалено!'); loadData(); } })
            .finally(() => toggleLoader(false));
        }

        /* --- UTILS & MODALS CLOSE --- */
        function addField(containerId, className, placeholderText) {
            let container = document.getElementById(containerId);
            let div = document.createElement('div'); div.className = 'dynamic-item-modern';
            
            let isCar = className.includes('car');
            let isPhone = className.includes('phone');
            let eventAttr = isCar ? 'oninput="formatCarInput(this)" onblur="validateInputDuplicate(this, \'car\')"' : (isPhone ? 'oninput="formatPhoneInput(this)" onblur="validateInputDuplicate(this, \'phone\')"' : '');

            div.innerHTML = `<input type="text" class="${className}" placeholder="${placeholderText}" ${eventAttr}><button class="btn-icon-del" onclick="this.parentElement.remove()">✕</button>`;
            container.appendChild(div);
            div.querySelector('input').focus();
        }

        function closeModal(id) {
            let m = document.getElementById(id);
            m.classList.remove('show');
            setTimeout(() => m.style.display = 'none', 250);
        }

        function debouncedSearch() {
            let query = document.getElementById('search').value;
            document.getElementById('search-clear').style.display = query.length > 0 ? 'block' : 'none';
            clearTimeout(searchTimeout);
            searchTimeout = setTimeout(applyMultiFilter, 250);
        }

        function clearSearch() {
            document.getElementById('search').value = '';
            document.getElementById('search-clear').style.display = 'none';
            document.getElementById('search-counter').style.display = 'none';
            applyMultiFilter();
        }

        function exportToCSV() {
            let rows = ["Квартира;Жильцы;Машины;Телефоны"];
            globalData.forEach(i => {
                rows.push(`"${i.flat}";"${Array.isArray(i.name)?i.name.join(', '):i.name}";"${i.cars?i.cars.join('\n'):''}";"${i.phones?i.phones.join('\n'):''}"`);
            });
            let blob = new Blob([new Uint8Array([0xEF, 0xBB, 0xBF]), rows.join('\r\n')], { type: "text/csv;charset=utf-8;" });
            let link = document.createElement("a");
            link.href = URL.createObjectURL(blob);
            link.download = `esp32_db_export_${Date.now()}.csv`;
            link.click();
        }

        /* --- SHORTCUTS & HOTKEYS --- */
        document.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'f') {
                e.preventDefault();
                document.getElementById('search').focus();
            }
            if (e.key === 'Escape') {
                let m = document.querySelector('.modal.show');
                if (m) closeModal(m.id);
            }
            // Quick Add (Key N when not typing)
            if (e.key.toLowerCase() === 'n' && !['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement.tagName)) {
                e.preventDefault();
                openModal();
            }
            // Quick Backup (Key B when not typing)
            if (e.key.toLowerCase() === 'b' && !['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement.tagName)) {
                e.preventDefault();
                openBackupModal();
            }
        });

        window.onload = function() {
            initTheme();
            loadData();
        };
    </script>
</body>
</html>
)rawliteral";


  
  server.send(200, "text/html", html);
}

// Отдача данных БД в формате JSON
void handleGetData() {
  if (!checkAuth()) return;
  
  File file = LittleFS.open(DB_FILE, "r");
  if (!file) {
    server.send(200, "application/json", "[]");
    return;
  }
  
  server.streamFile(file, "application/json");
  file.close();
}

// Добавление новой записи
void handleAddData() {
  if (!checkAuth()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "No data");
    return;
  }

  String postBody = server.arg("plain");
  JsonDocument newRecord;
  DeserializationError error = deserializeJson(newRecord, postBody);
  
  if (error) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  // Читаем старую базу
  JsonDocument db;
  if (LittleFS.exists(DB_FILE)) {
    File file = LittleFS.open(DB_FILE, "r");
    deserializeJson(db, file);
    file.close();
  } else {
    db.to<JsonArray>();
  }

  // Добавляем новую запись в массив
  db.add(newRecord);

  // Сохраняем обновленную базу обратно в LittleFS
  File file = LittleFS.open(DB_FILE, "w");
  if (!file) {
    server.send(500, "text/plain", "FS Error");
    return;
  }
  serializeJson(db, file);
  file.close();

  server.send(200, "text/plain", "OK");
}

// Обработчик массового удаления квартир по их номерам
void handleDeleteData() {
  if (!checkAuth()) return;
  
  // Проверяем, пришли ли данные в теле запроса
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }

  // Парсим входящий JSON от браузера (формат: {"flats":["кв. 1","кв. 2"]})
  JsonDocument requestDoc;
  DeserializationError error = deserializeJson(requestDoc, server.arg("plain"));
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonArray flatsToDelete = requestDoc["flats"].as<JsonArray>();
  if (flatsToDelete.isNull() || flatsToDelete.size() == 0) {
    server.send(400, "application/json", "{\"error\":\"Missing flats array\"}");
    return;
  }

  // Открываем существующую базу данных для чтения
  JsonDocument dbDoc;
  if (LittleFS.exists(DB_FILE)) {
    File file = LittleFS.open(DB_FILE, "r");
    deserializeJson(dbDoc, file);
    file.close();
  } else {
    server.send(404, "application/json", "{\"error\":\"Database file not found\"}");
    return;
  }

  JsonArray currentDB = dbDoc.as<JsonArray>();
  
  // Создаем временный документ для сохранения отфильтрованных квартир
  JsonDocument newDBDoc;
  JsonArray newDBArray = newDBDoc.to<JsonArray>();

  // Фильтруем: переносим в новую БД только те квартиры, которых нет в списке удаления
  for (JsonObject item : currentDB) {
    const char* itemFlat = item["flat"];
    bool shouldDelete = false;

    for (JsonVariant v : flatsToDelete) {
      if (itemFlat != nullptr && strcmp(itemFlat, v.as<const char*>()) == 0) {
        shouldDelete = true;
        break;
      }
    }

    if (!shouldDelete) {
      newDBArray.add(item);
    }
  }

  // Перезаписываем базу данных обновленным массивом
  File file = LittleFS.open(DB_FILE, "w");
  if (!file) {
    server.send(500, "application/json", "{\"error\":\"Failed to open DB for writing\"}");
    return;
  }
  serializeJson(newDBDoc, file);
  file.close();

  // Отправляем успешный статус, чтобы JS в браузере закрыл окно и обновил таблицу
  server.send(200, "application/json", "{\"status\":\"success\"}");
}

// Восстановление базы данных из бэкапа
void handleRestoreBackup() {
  if (!checkAuth()) return;
  
  // Проверяем, пришли ли данные в теле запроса
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing backup data");
    return;
  }

  String backupData = server.arg("plain");
  
  // Валидируем присланные данные, чтобы убедиться, что это правильный JSON
  JsonDocument testDoc;
  DeserializationError error = deserializeJson(testDoc, backupData);
  
  if (error) {
    server.send(400, "text/plain", "Invalid JSON format");
    return;
  }

  // Проверяем, что нам прислали именно массив (база данных должна быть массивом объектов)
  if (!testDoc.is<JsonArray>()) {
    server.send(400, "text/plain", "Backup must be a JSON Array");
    return;
  }

  // Полностью перезаписываем базу данных полученным файлом
  File file = LittleFS.open(DB_FILE, "w");
  if (!file) {
    server.send(500, "text/plain", "Failed to open DB file for writing");
    return;
  }
  
  file.print(backupData);
  file.close();

  Serial.println("База данных успешно восстановлена из бэкапа!");
  server.send(200, "text/plain", "OK");
}

// Редактирование существующей записи
void handleEditData() {
  if (!checkAuth()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "No data");
    return;
  }

  String postBody = server.arg("plain");
  JsonDocument editRecord;
  DeserializationError error = deserializeJson(editRecord, postBody);
  
  if (error) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  const char* originalFlat = editRecord["originalFlat"];
  const char* newFlat = editRecord["flat"];
  
  if (!originalFlat || !newFlat) {
    server.send(400, "text/plain", "Missing flat identifiers");
    return;
  }

  // Читаем базу данных
  JsonDocument db;
  if (LittleFS.exists(DB_FILE)) {
    File file = LittleFS.open(DB_FILE, "r");
    deserializeJson(db, file);
    file.close();
  } else {
    server.send(404, "text/plain", "Database not found");
    return;
  }

  JsonArray dbArray = db.as<JsonArray>();
  bool found = false;

  // Ищем запись по оригинальному названию квартиры и обновляем её поля
  for (JsonObject item : dbArray) {
    const char* currentFlat = item["flat"];
   if (currentFlat != nullptr && strcmp(currentFlat, originalFlat) == 0) {
    item["flat"] = newFlat;
    item["name"] = editRecord["name"];
    item["cars"] = editRecord["cars"];
    item["phones"] = editRecord["phones"];
    item["flags"] = editRecord["flags"];
    found = true;
    break;
}
  }

  if (!found) {
    server.send(404, "text/plain", "Record not found");
    return;
  }

  // Сохраняем обновленную базу обратно в LittleFS
  File file = LittleFS.open(DB_FILE, "w");
  if (!file) {
    server.send(500, "text/plain", "FS Error");
    return;
  }
  serializeJson(db, file);
  file.close();

  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);

  // Инициализация Файловой системы LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Ошибка монтирования LittleFS");
    return;
  }

  // Настройка Точки Доступа (SoftAP)
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Точка доступа запущена.");
  Serial.print("IP адрес ESP32 в сети AP: ");
  Serial.println(WiFi.softAPIP()); // Обычно 192.168.4.1

// Для POST запросов (добавляем явный сборщик контента collectHeaders)
  const char* headerkeys[] = {"Content-Type"} ;
  server.collectHeaders(headerkeys, 1 );
  
  // Маршруты веб-сервера
  server.on("/", handleRoot);
  server.on("/get_data", handleGetData);
  server.on("/add_data", HTTP_POST, handleAddData);
server.on("/delete_flats", HTTP_POST, handleDeleteData); // <-- Добавить/заменить на эту строку
server.on("/restore_backup", HTTP_POST, handleRestoreBackup); // <-- ДОБАВИТЬ ЭТУ СТРОКУ
server.on("/edit_data", HTTP_POST, handleEditData); // <-- ДОБАВИТЬ ЭТУ СТРОКУ
server.on("/get_data", HTTP_HEAD, []() {         // <-- ДОБАВИТЬ ЭТУ СТРОКУ
    if (!checkAuth()) return;
    server.send(200, "application/json", ""); 
  });

  server.begin();
  Serial.println("Веб-сервер успешно запущен!");
}

void loop() {
  server.handleClient();
}
