import { loadHTML } from '../utils/common-utils';
import ftbHTML from '../../../../ftb/default.html?raw';

export type SkinPreviewTheme = 'dark' | 'light';

let previewOverride: SkinPreviewTheme | null = null;
let activeTheme: SkinPreviewTheme = 'dark';

function fillToolbar(host: HTMLElement): void {
  const source = new DOMParser().parseFromString(ftbHTML, 'text/html');
  const statusBar = source.querySelector<HTMLElement>('.status-bar');
  if (!statusBar) return;
  statusBar.querySelectorAll('#en, #fullwidth, #puncEn').forEach((element) => element.remove());
  statusBar.querySelectorAll<HTMLElement>('[id]').forEach((element) => element.removeAttribute('id'));
  host.replaceChildren(statusBar);
}

function applySkinPreviewTheme(theme: SkinPreviewTheme): void {
  document.querySelectorAll('#skinCandidateHorizontal, #skinCandidateVertical').forEach((element) => {
    element.classList.toggle('theme-light', theme === 'light');
    element.classList.toggle('theme-dark', theme === 'dark');
  });
  document.querySelectorAll('#skinToolbarPreview').forEach((element) => {
    element.classList.toggle('theme-light', theme === 'light');
    element.classList.toggle('theme-dark', theme === 'dark');
  });

  const title = document.getElementById('skinThemeTitle');
  const button = document.getElementById('skinPreviewSwitch');
  if (title) {
    title.textContent = theme === 'light' ? 'Fluent 主题(Light)' : 'Fluent 主题(Dark)';
  }
  if (button) {
    button.textContent = theme === 'light' ? '预览深色' : '预览浅色';
  }
}

function resolvedPreviewTheme(): SkinPreviewTheme {
  return previewOverride ?? activeTheme;
}

export function syncSkinPreviewTheme(theme: SkinPreviewTheme): void {
  activeTheme = theme;
  // When the real theme changes, drop manual override so preview follows config again.
  previewOverride = null;
  applySkinPreviewTheme(resolvedPreviewTheme());
}

export async function setupSkin(): Promise<void> {
  const vertical = document.getElementById('skinCandidateVertical');
  const horizontal = document.getElementById('skinCandidateHorizontal');
  if (vertical) vertical.innerHTML = await loadHTML('/src/partials/candidate/candidate-wnd-v.html');
  if (horizontal) horizontal.innerHTML = await loadHTML('/src/partials/candidate/candidate-wnd-h.html');

  const toolbar = document.getElementById('skinToolbarPreview');
  if (toolbar) fillToolbar(toolbar);

  applySkinPreviewTheme(resolvedPreviewTheme());

  const switchBtn = document.getElementById('skinPreviewSwitch');
  switchBtn?.addEventListener('click', () => {
    const current = resolvedPreviewTheme();
    previewOverride = current === 'light' ? 'dark' : 'light';
    applySkinPreviewTheme(previewOverride);
  });
}
