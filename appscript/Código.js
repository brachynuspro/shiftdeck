/*
 * ShiftDeck OAuth Bridge - Google Apps Script
 * ============================================
 *
 * Returns a styled dark-mode page with a "Copy code" button after
 * Spotify redirects here with the authorization code. The user taps
 * the button, the code is copied to the clipboard, and the browser
 * auto-redirects to the ESP's /code?code=XXX endpoint which already
 * accepts and processes the code (see serveCode() in SpotifyOAuth.cpp).
 *
 * Apps Script project ID and deployment are managed by `clasp`
 * (see .clasp.json in appscript/). After editing this file, run:
 *   cd appscript && npx clasp push
 *
 * Spotify redirect_uri MUST be set to this Apps Script URL in the
 * Spotify Developer Dashboard. The ESP includes the URL in the
 * `state` parameter so we can redirect back to the right device.
 */

function doGet(e) {
  var props = PropertiesService.getScriptProperties();

  if (e.parameter.code) {
    props.setProperty('spotify_code', e.parameter.code);
    var ip = e.parameter.state || '192.168.0.19';
    var code = e.parameter.code;
    return HtmlService.createHtmlOutput(buildCodePage(code, ip))
      .setTitle('ShiftDeck - Copiar codigo')
      .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL);
  }

  if (e.parameter.action == 'get') {
    var code = props.getProperty('spotify_code') || '';
    if (code) props.deleteProperty('spotify_code');
    return ContentService.createTextOutput(code);
  }

  return ContentService.createTextOutput('ShiftDeck OAuth Bridge');
}

function buildCodePage(code, ip) {
  return [
    '<!DOCTYPE html>',
    '<html lang="pt-BR">',
    '<head>',
    '<meta charset="UTF-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">',
    '<title>ShiftDeck - Copiar codigo</title>',
    '<style>',
    '*{margin:0;padding:0;box-sizing:border-box}',
    'body{background:#121212;color:#fff;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;',
    'min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}',
    '.card{background:#1B1B1B;border-radius:14px;padding:28px 24px;width:100%;max-width:440px;',
    'box-shadow:0 8px 24px rgba(0,0,0,0.4);text-align:center}',
    '.logo{color:#1ED760;font-size:26px;font-weight:700;margin-bottom:4px;letter-spacing:0.5px}',
    'h1{color:#fff;font-size:17px;margin-bottom:6px;font-weight:500}',
    'p{color:#B3B3B3;font-size:13px;margin-bottom:18px;line-height:1.5}',
    '.ip{color:#1ED760;font-weight:600;font-family:"SF Mono",Menlo,Consolas,monospace;font-size:14px}',
    '.codebox{background:#0a0a0a;border:1px solid #1ED760;border-radius:8px;padding:14px;',
    'margin-bottom:18px;word-break:break-all;user-select:all;-webkit-user-select:all;',
    'font-family:"SF Mono",Menlo,Consolas,monospace;font-size:13px;color:#1ED760;',
    'max-height:140px;overflow-y:auto;line-height:1.4;text-align:left}',
    '.btn{display:block;width:100%;padding:14px;background:#1ED760;color:#000;border:none;',
    'border-radius:8px;font-size:16px;font-weight:700;cursor:pointer;margin-bottom:10px;',
    'transition:background 0.15s ease;font-family:inherit}',
    '.btn:active{background:#1ae660}',
    '.btn:disabled{background:#1ED760;color:#000;cursor:default}',
    '.btn.copied{background:#0a0a0a !important;color:#1ED760 !important;',
    'border:1px solid #1ED760 !important}',
    '.btn-secondary{display:block;width:100%;padding:11px;background:transparent;color:#B3B3B3;',
    'border:1px solid #333;border-radius:8px;font-size:13px;cursor:pointer;margin-bottom:0;',
    'text-decoration:none;font-family:inherit}',
    '.btn-secondary:active{background:#222}',
    '.spinner{display:inline-block;width:14px;height:14px;border:2px solid #1ED760;',
    'border-top-color:transparent;border-radius:50%;animation:spin 0.8s linear infinite;',
    'vertical-align:middle;margin-right:8px}',
    '@keyframes spin{to{transform:rotate(360deg)}}',
    '.hidden{display:none !important}',
    '.footer{color:#444;font-size:10px;margin-top:18px;line-height:1.4}',
    '</style>',
    '</head>',
    '<body>',
    '<div class="card">',
    '  <div class="logo">ShiftDeck</div>',
    '  <h1>Codigo de autorizacao</h1>',
    '  <p>Toque no botao abaixo para copiar o codigo.<br>',
    '     Voce sera redirecionado para o dispositivo em<br>',
    '     <span class="ip">' + escapeHtml(ip) + '</span></p>',
    '  <div class="codebox" id="code">' + escapeHtml(code) + '</div>',
    '  <button class="btn" id="copyBtn" type="button" onclick="copyCode()">',
    '    <span id="btnSpinner" class="spinner hidden"></span>',
    '    <span id="btnLabel">COPIAR CODIGO</span>',
    '  </button>',
    '  <a class="btn-secondary" id="manualLink" ',
    '     href="http://' + escapeHtml(ip) + '/code?code=' + encodeURIComponent(code) + '">',
    '    Ir para ' + escapeHtml(ip) + ' manualmente',
    '  </a>',
    '  <div class="footer">ShiftDeck v1.0.0 - ESP32-2432S028</div>',
    '</div>',
    '<script>',
    'var code = ' + JSON.stringify(code) + ';',
    'var ip = ' + JSON.stringify(ip) + ';',
    'var target = "http://" + ip + "/code?code=" + encodeURIComponent(code);',
    'var btn = document.getElementById("copyBtn");',
    'var label = document.getElementById("btnLabel");',
    'var spinner = document.getElementById("btnSpinner");',
    'var manualLink = document.getElementById("manualLink");',
    '',
    'function showCopied() {',
    '  spinner.classList.add("hidden");',
    '  label.textContent = "COPIADO  -  ABRINDO DISPOSITIVO...";',
    '  btn.classList.add("copied");',
    '  btn.disabled = true;',
    '  manualLink.style.display = "none";',
    '  setTimeout(function() { window.location.href = target; }, 1200);',
    '}',
    '',
    'function showError(msg) {',
    '  spinner.classList.add("hidden");',
    '  label.textContent = msg;',
    '  btn.classList.add("copied");',
    '  btn.disabled = true;',
    '  manualLink.textContent = "Toque aqui para abrir " + ip;',
    '  manualLink.href = target;',
    '}',
    '',
    'function fallbackCopy() {',
    '  var box = document.getElementById("code");',
    '  var range = document.createRange();',
    '  range.selectNode(box);',
    '  var sel = window.getSelection();',
    '  sel.removeAllRanges();',
    '  sel.addRange(range);',
    '  try {',
    '    var ok = document.execCommand("copy");',
    '    sel.removeAllRanges();',
    '    if (ok) showCopied(); else showError("NAO FOI POSSIVEL COPIAR");',
    '  } catch (e) {',
    '    sel.removeAllRanges();',
    '    showError("NAO FOI POSSIVEL COPIAR");',
    '  }',
    '}',
    '',
    'function copyCode() {',
    '  if (btn.disabled) return;',
    '  spinner.classList.remove("hidden");',
    '  label.textContent = "COPIANDO...";',
    '  if (navigator.clipboard && navigator.clipboard.writeText) {',
    '    navigator.clipboard.writeText(code).then(showCopied).catch(function() {',
    '      fallbackCopy();',
    '    });',
    '  } else {',
    '    fallbackCopy();',
    '  }',
    '}',
    '',
    '// Auto-trigger the copy after 1.2s so most users do not have to',
    '// tap the button. If auto-copy fails (e.g. browser blocks it),',
    '// the user can still tap the button to retry.',
    'setTimeout(copyCode, 1200);',
    '</script>',
    '</body>',
    '</html>'
  ].join('\n');
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}
