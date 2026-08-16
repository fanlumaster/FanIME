import { loadHTML } from '../utils/common-utils';
import ftbHTML from '../../../../ftb/default.html?raw';

export type SkinPreviewTheme = 'dark' | 'light';
export type CandidateSkin = 'fluent' | 'wechat' | 'graphite' | 'willow_green';

const SKINS: CandidateSkin[] = ['fluent', 'wechat', 'graphite', 'willow_green'];
const previewOverrides: Record<CandidateSkin, SkinPreviewTheme | null> = {
  fluent: null,
  wechat: null,
  graphite: null,
  willow_green: null
};
let activeTheme: SkinPreviewTheme = 'dark';
let activeSkin: CandidateSkin = 'fluent';
const SKIN_PREVIEW_PAGE_SIZE = 6;

function normalizeCandidateSkin(value: unknown): CandidateSkin {
  return value === 'wechat' || value === 'graphite' || value === 'willow_green' ? value : 'fluent';
}

function limitCandidatePreview(host: HTMLElement): void {
  host.querySelectorAll<HTMLElement>('.row-wrapper').forEach((wrapper, index) => {
    wrapper.style.display = index < SKIN_PREVIEW_PAGE_SIZE ? '' : 'none';
  });
}

function fillToolbar(host: HTMLElement): void {
  const source = new DOMParser().parseFromString(ftbHTML, 'text/html');
  const statusBar = source.querySelector<HTMLElement>('.status-bar');
  if (!statusBar) return;
  statusBar.querySelectorAll('#en, #fullwidth, #puncEn').forEach((element) => element.remove());
  statusBar.querySelectorAll<HTMLElement>('[id]').forEach((element) => element.removeAttribute('id'));
  host.replaceChildren(statusBar);
}

function applySkinPreviewTheme(skin: CandidateSkin, theme: SkinPreviewTheme): void {
  const card = document.querySelector<HTMLElement>(`[data-candidate-skin="${skin}"]`);
  if (!card) return;

  card.querySelectorAll<HTMLElement>('[data-skin-horizontal], [data-skin-vertical], [data-skin-toolbar]')
    .forEach((element) => {
      element.classList.toggle('theme-light', theme === 'light');
      element.classList.toggle('theme-dark', theme === 'dark');
    });

  const title = card.querySelector<HTMLElement>('.section-title');
  if (title) {
    const name = skin === 'wechat'
      ? '微信绿主题'
      : skin === 'graphite'
        ? '石墨 Graphite'
        : skin === 'willow_green'
          ? '杨柳青 Willow green'
          : 'Fluent 主题';
    title.textContent = `${name}(${theme === 'light' ? 'Light' : 'Dark'})`;
  }
  card.querySelectorAll<HTMLButtonElement>('[data-skin-preview-switch]').forEach((button) => {
    button.textContent = theme === 'light' ? '预览深色' : '预览浅色';
  });
}

function resolvedPreviewTheme(skin: CandidateSkin): SkinPreviewTheme {
  return previewOverrides[skin] ?? activeTheme;
}

function syncSkinSwitches(): void {
  document.querySelectorAll<HTMLElement>('[data-skin-switch]').forEach((element) => {
    const selected = element.dataset.skinSwitch === activeSkin;
    element.classList.toggle('active', selected);
    element.setAttribute('aria-checked', String(selected));
  });
}

function syncAppearancePreviews(): void {
  const useWechat = activeSkin === 'wechat';
  const useGraphite = activeSkin === 'graphite';
  const useWillowGreen = activeSkin === 'willow_green';
  document.querySelectorAll<HTMLElement>('.cand-preview .candidate').forEach((element) => {
    element.classList.toggle('skin-wechat', useWechat);
    element.classList.toggle('skin-graphite', useGraphite);
    element.classList.toggle('skin-willow-green', useWillowGreen);
  });
  document.querySelectorAll<HTMLElement>('.ftb-preview-host:not([data-skin-toolbar])').forEach((element) => {
    element.classList.toggle('skin-wechat', useWechat);
    element.classList.toggle('skin-graphite', useGraphite);
    element.classList.toggle('skin-willow-green', useWillowGreen);
  });
}

function selectSkin(value: unknown, persist: boolean): void {
  activeSkin = normalizeCandidateSkin(value);
  syncSkinSwitches();
  syncAppearancePreviews();
  if (persist) {
    window.chrome?.webview?.postMessage(JSON.stringify({
      type: 'configUpdate',
      data: { path: 'appearance.candidate_skin', value: activeSkin }
    }));
  }
}

export function applyCandidateSkin(value: unknown): void {
  selectSkin(value, false);
}

export function syncSkinPreviewTheme(theme: SkinPreviewTheme): void {
  activeTheme = theme;
  SKINS.forEach((skin) => {
    previewOverrides[skin] = null;
    applySkinPreviewTheme(skin, resolvedPreviewTheme(skin));
  });
}

export async function setupSkin(): Promise<void> {
  const horizontalHtml = await loadHTML('/src/partials/candidate/candidate-wnd-h.html');
  const verticalHtml = await loadHTML('/src/partials/candidate/candidate-wnd-v.html');

  document.querySelectorAll<HTMLElement>('[data-skin-horizontal]').forEach((host) => {
    host.innerHTML = horizontalHtml;
    limitCandidatePreview(host);
  });
  document.querySelectorAll<HTMLElement>('[data-skin-vertical]').forEach((host) => {
    host.innerHTML = verticalHtml;
    limitCandidatePreview(host);
  });
  document.querySelectorAll<HTMLElement>('[data-skin-toolbar]').forEach(fillToolbar);

  SKINS.forEach((skin) => applySkinPreviewTheme(skin, resolvedPreviewTheme(skin)));
  syncSkinSwitches();
  syncAppearancePreviews();

  document.querySelectorAll<HTMLElement>('[data-skin-switch]').forEach((element) => {
    const activate = () => selectSkin(element.dataset.skinSwitch, true);
    element.addEventListener('click', activate);
    element.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        activate();
      }
    });
  });

  document.querySelectorAll<HTMLButtonElement>('[data-skin-preview-switch]').forEach((button) => {
    button.addEventListener('click', () => {
      const card = button.closest<HTMLElement>('[data-candidate-skin]');
      const skin = normalizeCandidateSkin(card?.dataset.candidateSkin);
      const current = resolvedPreviewTheme(skin);
      previewOverrides[skin] = current === 'light' ? 'dark' : 'light';
      applySkinPreviewTheme(skin, previewOverrides[skin]!);
    });
  });
}
