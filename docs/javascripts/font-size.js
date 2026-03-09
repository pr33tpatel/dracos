const STORAGE_KEY = 'dracos-font-size';
const MIN = 10;
const MAX = 32;
const MOBILE_SIZE = 12;
const DESKTOP_SIZE = 20;

const isMobile = window.matchMedia('(max-width: 768px)').matches;
const DEFAULT = parseInt(localStorage.getItem(STORAGE_KEY)) || (isMobile ? MOBILE_SIZE : DESKTOP_SIZE)
function applyFontSize(px) {
  document.documentElement.style.fontSize = px + 'px';
}

function buildSlider() {
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

  slider.addEventListener('input', (e) => {
    preview.textContent = e.target.value + 'px';
  });

  slider.addEventListener('change', (e) => {
    const val = parseInt(e.target.value);
    applyFontSize(val);
    localStorage.setItem(STORAGE_KEY, val);
    preview.textContent = val + 'px';
  });

  // inject BEFORE the first child of the header inner (dark/light switch area)
  const target = document.querySelector('.md-header__inner');
  if (target) {
    const palette = target.querySelector('.md-header__option');
    if (palette) {
      target.insertBefore(wrapper, palette);  // insert left of dark/light switch
    } else {
      target.appendChild(wrapper);            // fallback
    }
  }
}

document.addEventListener('DOMContentLoaded', buildSlider);
