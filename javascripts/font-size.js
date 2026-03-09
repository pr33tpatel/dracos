const STORAGE_KEY = 'dracos-font-size';
const MIN = 12;
const MAX = 32;
const DEFAULT = 20;

function applyFontSize(px) {
  document.documentElement.style.fontSize = px + 'px';
}

function buildSlider() {
  // remove any existing slider (re-injection on navigation)
  const existing = document.getElementById('font-size-control');
  if (existing) existing.remove();

  const saved = parseInt(localStorage.getItem(STORAGE_KEY)) || DEFAULT;
  applyFontSize(saved);

  const wrapper = document.createElement('div');
  wrapper.id = 'font-size-control';
  wrapper.innerHTML = `
    <span class="font-size-label">A</span>
    <input type="range" min="${MIN}" max="${MAX}" value="${saved}"
           id="font-size-slider" title="Adjust font size">
    <span class="font-size-label large">A</span>
    <span id="font-size-preview">${saved}px</span>
  `;

  const slider = wrapper.querySelector('#font-size-slider');
  const preview = wrapper.querySelector('#font-size-preview');

  // show pending value while dragging, don't resize yet
  slider.addEventListener('input', (e) => {
    preview.textContent = e.target.value + 'px';
  });

  // resize only on release
  slider.addEventListener('change', (e) => {
    const val = parseInt(e.target.value);
    applyFontSize(val);
    localStorage.setItem(STORAGE_KEY, val);
    preview.textContent = val + 'px';
  });

  // inject BEFORE the first child of the header inner (dark/light switch area)
  const target = document.querySelector('.md-header__inner');
  if (target) {
    // find the color palette / theme toggle element
    const palette = target.querySelector('.md-header__option');
    if (palette) {
      target.insertBefore(wrapper, palette);  // insert left of dark/light switch
    } else {
      target.appendChild(wrapper);            // fallback
    }
  }
}

document.addEventListener('DOMContentLoaded', buildSlider);
