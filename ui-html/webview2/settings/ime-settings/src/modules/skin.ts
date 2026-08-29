import { loadHTML } from '../utils/common-utils';
import ftbHTML from '../../../../ftb/default.html?raw';

export type SkinPreviewTheme = 'dark' | 'light';
export type CandidateSkin = string;

type ExternalSkin = {
  id: string; name: string; version: string; author?: string; description?: string;
  base: string; stylesheet?: string; toolbarStylesheet?: string; preview?: string; layouts: string[]; themes: string[];
  minWidthDip?: number; decorationTopDip?: number; decorationWidthDip?: number; compatible: boolean;
};
type SkinScanIssue = { folder: string; reason: string };

const BUILTIN_SKINS = ['fluent', 'wechat', 'graphite', 'willow_green'] as const;
type BuiltinSkin = typeof BUILTIN_SKINS[number];
const previewOverrides: Record<string, SkinPreviewTheme | null> = {
  fluent: null, wechat: null, graphite: null, willow_green: null
};
const loadedExternalStyleIds = new Set<string>();
let activeTheme: SkinPreviewTheme = 'dark';
let activeSkin: CandidateSkin = 'fluent';
let externalSkins: ExternalSkin[] = [];
let scanIssues: SkinScanIssue[] = [];
let skinDirectory = '';
let catalogScanned = false;
let catalogRevision = 0;
let previewHorizontalHtml = '';
let previewVerticalHtml = '';
const SKIN_PREVIEW_PAGE_SIZE = 6;

function normalizeCandidateSkin(value: unknown): CandidateSkin {
  return typeof value === 'string' && /^[a-z0-9][a-z0-9._-]{0,63}$/.test(value) ? value : 'fluent';
}

function asBuiltinSkin(value: unknown): BuiltinSkin {
  return BUILTIN_SKINS.includes(value as BuiltinSkin) ? value as BuiltinSkin : 'fluent';
}

function findExternalSkin(id: string): ExternalSkin | undefined {
  return externalSkins.find((skin) => skin.id === id);
}

function builtinPreviewClass(skinId: string): string {
  if (skinId === 'wechat') return 'skin-wechat';
  if (skinId === 'graphite') return 'skin-graphite';
  if (skinId === 'willow_green') return 'skin-willow-green';
  const external = findExternalSkin(skinId);
  return external ? builtinPreviewClass(external.base) : '';
}

function limitCandidatePreview(host: HTMLElement): void {
  host.querySelectorAll<HTMLElement>('.row-wrapper').forEach((wrapper, index) => {
    wrapper.style.display = index < SKIN_PREVIEW_PAGE_SIZE ? '' : 'none';
  });
}

function wrapCandidateHtml(html: string): string {
  return `<div class="containerParent">${html}</div>`;
}

function fillPreviewHost(host: HTMLElement, html: string): void {
  host.innerHTML = wrapCandidateHtml(html);
  host.querySelectorAll('.container').forEach((container) => {
    container.classList.add('hover-active');
  });
  limitCandidatePreview(host);
}

function fillToolbar(host: HTMLElement): void {
  const source = new DOMParser().parseFromString(ftbHTML, 'text/html');
  const statusBar = source.querySelector<HTMLElement>('.status-bar');
  if (!statusBar) return;
  statusBar.querySelectorAll('#en, #fullwidth, #puncEn').forEach((element) => element.remove());
  statusBar.querySelectorAll<HTMLElement>('[id]').forEach((element) => element.removeAttribute('id'));
  host.replaceChildren(statusBar);
}

function resourceUrl(id: string, relativePath: string): string {
  return `https://candidate-skins/${encodeURIComponent(id)}/${relativePath.split('/').map(encodeURIComponent).join('/')}?v=${catalogRevision}`;
}

function rewriteSkinCssUrls(css: string, skinId: string): string {
  return css.replace(/url\(\s*(['"]?)([^'")]+)\1\s*\)/gi, (full, _quote: string, url: string) => {
    const trimmed = url.trim();
    if (!trimmed || /^(data:|https?:|\/\/|\/)/i.test(trimmed)) return full;
    const relative = trimmed.replace(/^\.\//, '');
    if (relative.includes('..')) return full;
    return `url("${resourceUrl(skinId, relative)}")`;
  });
}

function applyDecorationVars(host: HTMLElement, skin: ExternalSkin): void {
  host.style.setProperty('--msime-skin-min-width', `${skin.minWidthDip || 0}px`);
  host.style.setProperty('--msime-skin-decoration-top', `${skin.decorationTopDip || 0}px`);
  host.style.setProperty('--msime-skin-decoration-width', `${skin.decorationWidthDip || 0}px`);
}

function clearDecorationVars(host: HTMLElement): void {
  host.style.removeProperty('--msime-skin-min-width');
  host.style.removeProperty('--msime-skin-decoration-top');
  host.style.removeProperty('--msime-skin-decoration-width');
}

async function injectScopedSkinCss(skin: ExternalSkin, stylesheet: string | undefined, styleId: string, force = false): Promise<void> {
  if (!stylesheet) return;
  if (!force && loadedExternalStyleIds.has(styleId) && document.getElementById(styleId)) return;
  try {
    const response = await fetch(resourceUrl(skin.id, stylesheet), { cache: 'no-store' });
    if (!response.ok) return;
    const css = rewriteSkinCssUrls(await response.text(), skin.id).replace(/:root\b/g, ':scope');
    const scoped = `@scope ([data-external-skin-preview="${skin.id}"]) {\n${css}\n}`;
    let style = document.getElementById(styleId) as HTMLStyleElement | null;
    if (!style) {
      style = document.createElement('style');
      style.id = styleId;
      style.dataset.externalSkinStyle = skin.id;
      document.head.appendChild(style);
    }
    style.textContent = scoped;
    loadedExternalStyleIds.add(styleId);
  } catch {
    // Preview stays on the inherited built-in base if the stylesheet cannot load.
  }
}

async function ensureExternalSkinStyle(skin: ExternalSkin, force = false): Promise<void> {
  await Promise.all([
    injectScopedSkinCss(skin, skin.stylesheet, `external-skin-style-${skin.id}`, force),
    injectScopedSkinCss(skin, skin.toolbarStylesheet, `external-toolbar-style-${skin.id}`, force)
  ]);
}

function resetExternalSkinStyles(): void {
  document.querySelectorAll('style[data-external-skin-style]').forEach((node) => node.remove());
  loadedExternalStyleIds.clear();
}

function resolvedPreviewTheme(skinId: string, supported?: string[]): SkinPreviewTheme {
  const override = previewOverrides[skinId];
  if (override) return override;
  if (!supported || supported.length === 0 || supported.includes(activeTheme)) return activeTheme;
  return supported[0] === 'light' ? 'light' : 'dark';
}

function applyBuiltinCardTheme(skin: BuiltinSkin, theme: SkinPreviewTheme): void {
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

function applyExternalCardTheme(skin: ExternalSkin): void {
  const card = document.querySelector<HTMLElement>(`[data-candidate-skin="${skin.id}"]`);
  if (!card) return;
  const theme = resolvedPreviewTheme(skin.id, skin.themes);
  card.querySelectorAll<HTMLElement>('[data-skin-horizontal], [data-skin-vertical], [data-skin-toolbar]')
    .forEach((element) => {
      element.classList.toggle('theme-light', theme === 'light');
      element.classList.toggle('theme-dark', theme === 'dark');
    });
  card.querySelectorAll<HTMLButtonElement>('[data-skin-preview-switch]').forEach((button) => {
    button.textContent = theme === 'light' ? '预览深色' : '预览浅色';
  });
}

function syncSkinSwitches(): void {
  document.querySelectorAll<HTMLElement>('[data-skin-switch]').forEach((element) => {
    const selected = element.dataset.skinSwitch === activeSkin;
    element.classList.toggle('active', selected);
    element.setAttribute('aria-checked', String(selected));
  });
}

function ensureContainerParent(root: HTMLElement): void {
  const box = root.querySelector<HTMLElement>(':scope > .container, .container');
  if (!box || box.parentElement?.classList.contains('containerParent')) return;
  if (box.parentElement !== root && !root.contains(box)) return;
  const parent = document.createElement('div');
  parent.className = 'containerParent';
  box.replaceWith(parent);
  parent.append(box);
}

function syncAppearancePreviews(): void {
  const previewClass = builtinPreviewClass(activeSkin);
  const external = findExternalSkin(activeSkin);
  document.querySelectorAll<HTMLElement>('.cand-preview .candidate').forEach((element) => {
    element.classList.toggle('skin-wechat', previewClass === 'skin-wechat');
    element.classList.toggle('skin-graphite', previewClass === 'skin-graphite');
    element.classList.toggle('skin-willow-green', previewClass === 'skin-willow-green');
    ensureContainerParent(element);
    if (external) {
      element.dataset.externalSkinPreview = external.id;
      applyDecorationVars(element, external);
    } else {
      delete element.dataset.externalSkinPreview;
      clearDecorationVars(element);
    }
  });
  document.querySelectorAll<HTMLElement>('.ftb-preview-host:not([data-skin-toolbar])').forEach((element) => {
    element.classList.toggle('skin-wechat', previewClass === 'skin-wechat');
    element.classList.toggle('skin-graphite', previewClass === 'skin-graphite');
    element.classList.toggle('skin-willow-green', previewClass === 'skin-willow-green');
    if (external) {
      element.dataset.externalSkinPreview = external.id;
    } else {
      delete element.dataset.externalSkinPreview;
    }
  });
  document.querySelectorAll<HTMLElement>('.cand-preview').forEach((element) => {
    element.classList.toggle('has-skin-decoration', !!(external && (external.decorationTopDip || 0) > 0));
  });
  if (external) void ensureExternalSkinStyle(external);
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

function bindSkinSwitch(element: HTMLElement, skinId: string, enabled: boolean): void {
  element.dataset.skinSwitch = skinId;
  element.setAttribute('role', 'switch');
  element.tabIndex = enabled ? 0 : -1;
  element.setAttribute('aria-disabled', String(!enabled));
  if (!enabled) return;
  const activate = () => selectSkin(skinId, true);
  element.addEventListener('click', activate);
  element.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); activate(); }
  });
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
    const previewClass = builtinPreviewClass(skin.base);
    card.className = `section skin-theme-card external-skin-live-card${skin.compatible ? '' : ' is-incompatible'}${(skin.decorationTopDip || 0) > 0 ? ' has-skin-decoration' : ''}`;
    card.dataset.candidateSkin = skin.id;
    card.dataset.externalSkinPreview = skin.id;
    applyDecorationVars(card, skin);

    const header = document.createElement('div');
    header.className = 'skin-theme-header';
    const titles = document.createElement('div');
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
    titles.append(title, meta, description);

    const actions = document.createElement('div');
    actions.className = 'skin-theme-actions';
    const toggle = document.createElement('div');
    toggle.className = 'ftb-toggle-btn skin-theme-toggle';
    const knob = document.createElement('div');
    knob.className = 'ftb-toggle-knob';
    toggle.append(knob);
    bindSkinSwitch(toggle, skin.id, skin.compatible);
    actions.append(toggle);
    const switcher = document.createElement('button');
    switcher.type = 'button';
    switcher.className = 'skin-preview-switch';
    switcher.dataset.skinPreviewSwitch = '';
    switcher.title = '切换明暗预览';
    switcher.textContent = '预览浅色';
    switcher.addEventListener('click', () => {
      previewOverrides[skin.id] = resolvedPreviewTheme(skin.id) === 'light' ? 'dark' : 'light';
      applyExternalCardTheme(skin);
    });
    actions.append(switcher);
    header.append(titles, actions);

    const previews = document.createElement('div');
    previews.className = 'skin-preview-list';
    const horizontal = document.createElement('div');
    horizontal.className = 'skin-preview-row skin-preview-horizontal';
    const horizontalStage = document.createElement('div');
    horizontalStage.className = 'skin-preview-stage';
    const horizontalHost = document.createElement('div');
    horizontalHost.className = `candidate wnd-h${previewClass ? ` ${previewClass}` : ''}`;
    horizontalHost.dataset.skinHorizontal = '';
    if (previewHorizontalHtml) fillPreviewHost(horizontalHost, previewHorizontalHtml);
    horizontalStage.append(horizontalHost);
    horizontal.append(horizontalStage);
    const vertical = document.createElement('div');
    vertical.className = 'skin-preview-row skin-preview-vertical';
    const verticalStage = document.createElement('div');
    verticalStage.className = 'skin-preview-stage';
    const verticalHost = document.createElement('div');
    verticalHost.className = `candidate wnd-v${previewClass ? ` ${previewClass}` : ''}`;
    verticalHost.dataset.skinVertical = '';
    if (previewVerticalHtml) fillPreviewHost(verticalHost, previewVerticalHtml);
    verticalStage.append(verticalHost);
    vertical.append(verticalStage);
    const toolbar = document.createElement('div');
    toolbar.className = 'skin-preview-row skin-preview-toolbar';
    const toolbarStage = document.createElement('div');
    toolbarStage.className = 'skin-preview-stage';
    const toolbarHost = document.createElement('div');
    toolbarHost.className = `ftb-preview-host${previewClass ? ` ${previewClass}` : ''}`;
    toolbarHost.dataset.skinToolbar = '';
    fillToolbar(toolbarHost);
    toolbarStage.append(toolbarHost);
    toolbar.append(toolbarStage);
    previews.append(horizontal, vertical, toolbar);

    card.append(header, previews);
    void ensureExternalSkinStyle(skin, true).then(() => applyExternalCardTheme(skin));
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
  externalSkins.forEach(applyExternalCardTheme);
  syncSkinSwitches();
  syncAppearancePreviews();
}

export function applyCandidateSkinCatalog(
  skins: unknown, issues: unknown, directory: unknown, scanned: unknown, revision: unknown
): void {
  const nextRevision = typeof revision === 'number' && Number.isFinite(revision) ? revision : 0;
  if (nextRevision !== catalogRevision) resetExternalSkinStyles();
  externalSkins = Array.isArray(skins) ? skins.filter((skin): skin is ExternalSkin =>
    !!skin && typeof skin === 'object' && typeof skin.id === 'string' && typeof skin.name === 'string').map((skin) => ({
    ...skin,
    base: typeof skin.base === 'string' ? skin.base : 'fluent',
    layouts: Array.isArray(skin.layouts) ? skin.layouts : [],
    themes: Array.isArray(skin.themes) ? skin.themes : [],
    compatible: skin.compatible !== false
  })) : [];
  scanIssues = Array.isArray(issues) ? issues.filter((issue): issue is SkinScanIssue =>
    !!issue && typeof issue === 'object' && typeof issue.folder === 'string' && typeof issue.reason === 'string') : [];
  skinDirectory = typeof directory === 'string' ? directory : '';
  catalogScanned = scanned === true;
  catalogRevision = nextRevision;
  renderExternalSkins();
}

export function applyCandidateSkin(value: unknown): void { selectSkin(value, false); }

export function syncSkinPreviewTheme(theme: SkinPreviewTheme): void {
  activeTheme = theme;
  BUILTIN_SKINS.forEach((skin) => {
    previewOverrides[skin] = null;
    applyBuiltinCardTheme(skin, resolvedPreviewTheme(skin));
  });
  externalSkins.forEach((skin) => {
    previewOverrides[skin.id] = null;
    applyExternalCardTheme(skin);
  });
}

export async function setupSkin(): Promise<void> {
  resetExternalSkinStyles();
  previewHorizontalHtml = await loadHTML('/src/partials/candidate/candidate-wnd-h.html');
  previewVerticalHtml = await loadHTML('/src/partials/candidate/candidate-wnd-v.html');
  document.querySelectorAll<HTMLElement>('[data-skin-horizontal]').forEach((host) => {
    host.innerHTML = previewHorizontalHtml; limitCandidatePreview(host);
  });
  document.querySelectorAll<HTMLElement>('[data-skin-vertical]').forEach((host) => {
    host.innerHTML = previewVerticalHtml; limitCandidatePreview(host);
  });
  document.querySelectorAll<HTMLElement>('[data-skin-toolbar]').forEach(fillToolbar);
  BUILTIN_SKINS.forEach((skin) => applyBuiltinCardTheme(skin, resolvedPreviewTheme(skin)));
  syncSkinSwitches();
  syncAppearancePreviews();
  renderExternalSkins();

  document.querySelectorAll<HTMLElement>('[data-skin-switch]').forEach((element) => {
    if (!BUILTIN_SKINS.includes(element.dataset.skinSwitch as BuiltinSkin)) return;
    bindSkinSwitch(element, element.dataset.skinSwitch || 'fluent', true);
  });
  document.querySelectorAll<HTMLButtonElement>('[data-skin-preview-switch]').forEach((button) => {
    button.addEventListener('click', () => {
      const skin = asBuiltinSkin(button.closest<HTMLElement>('[data-candidate-skin]')?.dataset.candidateSkin);
      previewOverrides[skin] = resolvedPreviewTheme(skin) === 'light' ? 'dark' : 'light';
      applyBuiltinCardTheme(skin, previewOverrides[skin]!);
    });
  });
  document.getElementById('refreshExternalSkins')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'skinCatalogRequest' }));
  });
  document.getElementById('openExternalSkinDirectory')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'openSkinDirectory' }));
  });
}
