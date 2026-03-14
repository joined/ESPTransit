# Web Flasher

Flash ESPTransit firmware directly from your browser using WebSerial.

!!! warning "Browser Compatibility"
    WebSerial is only supported in **Chromium-based browsers** (Chrome, Edge, Opera, Brave).
    Firefox and Safari are not supported.

<div id="flash-tool">

<div class="flash-controls">
  <div class="flash-row">
    <label for="release-select">Release</label>
    <select id="release-select" disabled>
      <option>Loading releases...</option>
    </select>
  </div>

  <div class="flash-row">
    <label for="board-select">Board</label>
    <select id="board-select" disabled>
      <option value="jc8012p4a1c">JC8012P4A1C (10.1" 800x1280)</option>
      <option value="jc4880p443c">JC4880P443C (4.3" 480x800)</option>
      <option value="jc1060p470c">JC1060P470C (7.0" 1024x600)</option>
    </select>
  </div>

  <div class="flash-buttons">
    <button id="connect-btn" class="md-button" disabled>Connect</button>
    <button id="flash-btn" class="md-button md-button--primary" disabled>Flash</button>
  </div>
</div>

<div class="flash-progress" id="flash-progress" hidden>
  <div class="progress-item">
    <span class="progress-label" id="progress-label">Ready</span>
    <div class="progress-bar-outer">
      <div class="progress-bar-inner" id="progress-bar" style="width: 0%"></div>
    </div>
    <span class="progress-percent" id="progress-percent">0%</span>
  </div>
</div>

<div class="flash-log-container" id="log-container" hidden>
  <div class="flash-log-header">
    <span>Console</span>
    <button id="log-clear-btn" class="flash-log-clear" title="Clear log">Clear</button>
  </div>
  <pre class="flash-log" id="flash-log"></pre>
</div>

</div>
