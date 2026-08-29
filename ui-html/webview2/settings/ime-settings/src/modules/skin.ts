import { loadHTML } from '../utils/common-utils';
import ftbHTML from '../../../../ftb/default.html?raw';

export type SkinPreviewTheme = 'dark' | 'light';
export type CandidateSkin = string;

type ExternalSkin = {
  id: string; name: string; version: string; author?: string; description?: string;
  base: string; preview?: string; layouts: string[]; themes: string[]; compatible: boolean;
};
type SkinScanIssue = { folder: string; reason: string };

const BUILTIN_SKINS = ['fluent', 'wechat', 'graphite', 'willow_green'] as const;
type BuiltinSkin = typeof BUILTIN_SKINS[number];
const previewOverrides: Record<BuiltinSkin, SkinPreviewTheme | null> = {
  fluent: null, wechat: null, graphite: null, willow_green: null
};
let activeTheme: SkinPreviewTheme = 'dark';
let activeSkin: CandidateSkin = 'fluent';
let externalSkins: ExternalSkin[] = [];
let scanIssues: SkinScanIssue[] = [];
let skinDirectory = '';
let catalogScanned = false;
let catalogRevision = 0;
const SKIN_PREVIEW_PAGE_SIZE = 6;

function normalizeCandidateSkin(value: unknown): CandidateSkin {
  return typeof value === 'string' && /^[a-z0-9][a-z0-9._-]{0,63}$/.test(value) ? value : 'fluent';
}

function asBuiltinSkin(value: unknown): BuiltinSkin {
  return BUILTIN_SKINS.includes(value as BuiltinSkin) ? value as BuiltinSkin : 'fluent';
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

function applySkinPreviewTheme(skin: BuiltinSkin, theme: SkinPreviewTheme): void {
  const card = document.querySelector<HTMLElement>(`[data-candidate-skin="${skin}"]`);
  if (!card) return;
  card.querySelectorAll<HTMLElement>('[data-skin-horizontal], [data-skin-vertical], [data-skin-toolbar]')
    .forEach((element) => {
      element.classList.toggle('theme-light', theme === 'light');
      element.classList.toggle('theme-dark', theme === 'dark');
    });
  const title = card.querySelector<HTMLElement>('.section-title');
  if (title) {
    const name = skin === 'wechat' ? '微信绿主题' : skin === 'graphite' ? '石墨 Graphite'
      : skin === 'willow_green' ? '杨柳青 Willow green' : 'Fluent 主题';
    title.textContent = `${name}(${theme === 'light' ? 'Light' : 'Dark'})`;
  }
  card.querySelectorAll<HTMLButtonElement>('[data-skin-preview-switch]').forEach((button) => {
    button.textContent = theme === 'light' ? '预览深色' : '预览浅色';
  });
}

function resolvedPreviewTheme(skin: BuiltinSkin): SkinPreviewTheme {
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
      type: 'configUpdate', data: { path: 'appearance.candidate_skin', value: activeSkin }
    }));
  }
}

function resourceUrl(id: string, relativePath: string): string {
  return `https://candidate-skins/${encodeURIComponent(id)}/${relativePath.split('/').map(encodeURIComponent).join('/')}?v=${catalogRevision}`;
}

function renderExternalSkins(): void {
  const list = document.getElementById('externalSkinList');
  const empty = document.getElementById('externalSkinEmpty');
  const directory = document.getElementById('externalSkinDirectory');
  const diagnostics = document.getElementById('externalSkinDiagnostics');
  if (!list || !empty || !directory || !diagnostics) return;

  directory.textContent = skinDirectory || '%LOCALAPPDATA%\\metasequoiaime\\skins';
  list.replaceChildren(...externalSkins.map((skin) => {
    const card = document.createElement('div');
    card.className = `section external-skin-card${skin.compatible ? '' : ' is-incompatible'}`;
    card.dataset.candidateSkin = skin.id;
    const preview = document.createElement('div');
    preview.className = 'external-skin-preview';
    if (skin.preview) {
      const image = document.createElement('img');
      image.src = resourceUrl(skin.id, skin.preview);
      image.alt = '';
      image.loading = 'lazy';
      preview.append(image);
    } else preview.textContent = '无预览图';

    const info = document.createElement('div');
    info.className = 'external-skin-info';
    const title = document.createElement('div');
    title.className = 'section-title';
    title.textContent = skin.name;
    const meta = document.createElement('div');
    meta.className = 'external-skin-meta';
    meta.textContent = [skin.id, skin.version && `v${skin.version}`, skin.author].filter(Boolean).join(' · ');
    const description = document.createElement('div');
    description.className = 'skin-theme-description';
    description.textContent = skin.compatible ? (skin.description || `基于 ${skin.base}`)
      : `当前布局或明暗模式不受支持（${skin.layouts.join('/')}，${skin.themes.join('/')}）`;
    info.append(title, meta, description);

    const toggle = document.createElement('div');
    toggle.className = 'ftb-toggle-btn skin-theme-toggle';
    toggle.dataset.skinSwitch = skin.id;
    toggle.setAttribute('role', 'switch');
    toggle.setAttribute('aria-checked', 'false');
    toggle.tabIndex = skin.compatible ? 0 : -1;
    toggle.setAttribute('aria-disabled', String(!skin.compatible));
    const knob = document.createElement('div');
    knob.className = 'ftb-toggle-knob';
    toggle.append(knob);
    if (skin.compatible) {
      const activate = () => selectSkin(skin.id, true);
      toggle.addEventListener('click', activate);
      toggle.addEventListener('keydown', (event) => {
        if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); activate(); }
      });
    }
    card.append(preview, info, toggle);
    return card;
  }));
  empty.textContent = catalogScanned ? '没有发现外部皮肤。' : '尚未扫描。点击“刷新皮肤”读取皮肤目录。';
  empty.hidden = externalSkins.length !== 0;
  diagnostics.replaceChildren();
  if (scanIssues.length) {
    const details = document.createElement('details');
    const label = document.createElement('summary');
    label.textContent = `已忽略 ${scanIssues.length} 个无效皮肤目录`;
    const entries = document.createElement('ul');
    scanIssues.forEach((issue) => {
      const item = document.createElement('li');
      item.textContent = `${issue.folder}：${issue.reason}`;
      entries.append(item);
    });
    details.append(label, entries);
    diagnostics.append(details);
  }
  syncSkinSwitches();
}

export function applyCandidateSkinCatalog(
  skins: unknown, issues: unknown, directory: unknown, scanned: unknown, revision: unknown
): void {
  externalSkins = Array.isArray(skins) ? skins.filter((skin): skin is ExternalSkin =>
    !!skin && typeof skin === 'object' && typeof skin.id === 'string' && typeof skin.name === 'string') : [];
  scanIssues = Array.isArray(issues) ? issues.filter((issue): issue is SkinScanIssue =>
    !!issue && typeof issue === 'object' && typeof issue.folder === 'string' && typeof issue.reason === 'string') : [];
  skinDirectory = typeof directory === 'string' ? directory : '';
  catalogScanned = scanned === true;
  catalogRevision = typeof revision === 'number' && Number.isFinite(revision) ? revision : 0;
  renderExternalSkins();
}

export function applyCandidateSkin(value: unknown): void { selectSkin(value, false); }

export function syncSkinPreviewTheme(theme: SkinPreviewTheme): void {
  activeTheme = theme;
  BUILTIN_SKINS.forEach((skin) => {
    previewOverrides[skin] = null;
    applySkinPreviewTheme(skin, resolvedPreviewTheme(skin));
  });
}

export async function setupSkin(): Promise<void> {
  const horizontalHtml = await loadHTML('/src/partials/candidate/candidate-wnd-h.html');
  const verticalHtml = await loadHTML('/src/partials/candidate/candidate-wnd-v.html');
  document.querySelectorAll<HTMLElement>('[data-skin-horizontal]').forEach((host) => {
    host.innerHTML = horizontalHtml; limitCandidatePreview(host);
  });
  document.querySelectorAll<HTMLElement>('[data-skin-vertical]').forEach((host) => {
    host.innerHTML = verticalHtml; limitCandidatePreview(host);
  });
  document.querySelectorAll<HTMLElement>('[data-skin-toolbar]').forEach(fillToolbar);
  BUILTIN_SKINS.forEach((skin) => applySkinPreviewTheme(skin, resolvedPreviewTheme(skin)));
  syncSkinSwitches();
  syncAppearancePreviews();
  renderExternalSkins();

  document.querySelectorAll<HTMLElement>('[data-skin-switch]').forEach((element) => {
    if (!BUILTIN_SKINS.includes(element.dataset.skinSwitch as BuiltinSkin)) return;
    const activate = () => selectSkin(element.dataset.skinSwitch, true);
    element.addEventListener('click', activate);
    element.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); activate(); }
    });
  });
  document.querySelectorAll<HTMLButtonElement>('[data-skin-preview-switch]').forEach((button) => {
    button.addEventListener('click', () => {
      const skin = asBuiltinSkin(button.closest<HTMLElement>('[data-candidate-skin]')?.dataset.candidateSkin);
      previewOverrides[skin] = resolvedPreviewTheme(skin) === 'light' ? 'dark' : 'light';
      applySkinPreviewTheme(skin, previewOverrides[skin]!);
    });
  });
  document.getElementById('refreshExternalSkins')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'skinCatalogRequest' }));
  });
  document.getElementById('openExternalSkinDirectory')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'openSkinDirectory' }));
  });
}
