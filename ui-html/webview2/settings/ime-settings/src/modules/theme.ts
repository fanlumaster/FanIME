import { syncSkinPreviewTheme } from './skin';

export type ThemeMode = 'dark' | 'light' | 'system';
export type SurfaceTheme = 'follow' | 'dark' | 'light';
export type ResolvedTheme = 'dark' | 'light';

export type ThemeConfig = {
  theme_mode?: string;
  theme_settings?: string;
  theme_cand?: string;
  theme_ftb?: string;
  theme_menu?: string;
};

const THEME_MODE_LABELS: Record<ThemeMode, string> = {
  dark: '深色',
  light: '浅色',
  system: '跟随系统'
};

const SURFACE_THEME_LABELS: Record<SurfaceTheme, string> = {
  follow: '跟随全局',
  dark: '深色',
  light: '浅色'
};

let mediaQuery: MediaQueryList | null = null;

function normalizeThemeMode(value: string | undefined): ThemeMode {
  if (value === 'light' || value === 'dark' || value === 'system') {
    return value;
  }
  if (value === 'auto') {
    return 'system';
  }
  return 'dark';
}

function normalizeSurfaceTheme(value: string | undefined): SurfaceTheme {
  if (value === 'light' || value === 'dark' || value === 'follow') {
    return value;
  }
  return 'follow';
}

export function isSystemLightTheme(): boolean {
  return window.matchMedia?.('(prefers-color-scheme: light)').matches ?? false;
}

export function resolveTheme(mode: ThemeMode, surface: SurfaceTheme = 'follow'): ResolvedTheme {
  if (surface === 'dark' || surface === 'light') {
    return surface;
  }
  if (mode === 'system') {
    return isSystemLightTheme() ? 'light' : 'dark';
  }
  return mode;
}

export function applyDocumentTheme(theme: ResolvedTheme): void {
  const root = document.documentElement;
  root.setAttribute('data-theme', theme);
  root.style.colorScheme = theme;

  const meta = document.querySelector('meta[name="color-scheme"]');
  if (meta) {
    meta.setAttribute('content', theme);
  }

  document.body?.setAttribute('data-theme', theme);
  document.body?.classList.toggle('theme-light', theme === 'light');
  document.body?.classList.toggle('theme-dark', theme === 'dark');
}

export function applyCandidatePreviewTheme(theme: ResolvedTheme): void {
  document.querySelectorAll('.candidate:not(#skinCandidateHorizontal):not(#skinCandidateVertical)').forEach((element) => {
    element.classList.toggle('theme-light', theme === 'light');
    element.classList.toggle('theme-dark', theme === 'dark');
  });
}

function ensureSystemThemeListener(onChange: () => void): void {
  if (!window.matchMedia) {
    return;
  }
  if (!mediaQuery) {
    mediaQuery = window.matchMedia('(prefers-color-scheme: light)');
  }
  mediaQuery.onchange = () => onChange();
}

export function applyThemeConfig(config: ThemeConfig | undefined): void {
  const mode = normalizeThemeMode(config?.theme_mode);
  const settingsTheme = normalizeSurfaceTheme(config?.theme_settings);
  const candTheme = normalizeSurfaceTheme(config?.theme_cand);

  const settingsResolved = resolveTheme(mode, settingsTheme);
  const candResolved = resolveTheme(mode, candTheme);

  applyDocumentTheme(settingsResolved);
  applyCandidatePreviewTheme(candResolved);
  applyFtbPreviewTheme(resolveTheme(mode, normalizeSurfaceTheme(config?.theme_ftb)));

  applyDropdownLabel('themeBtn', THEME_MODE_LABELS[mode]);
  applyDropdownLabel('settingsThemeBtn', SURFACE_THEME_LABELS[settingsTheme]);
  applyDropdownLabel('candThemeBtn', SURFACE_THEME_LABELS[candTheme]);
  applyDropdownLabel('ftbThemeBtn', SURFACE_THEME_LABELS[normalizeSurfaceTheme(config?.theme_ftb)]);
  applyDropdownLabel('menuThemeBtn', SURFACE_THEME_LABELS[normalizeSurfaceTheme(config?.theme_menu)]);

  ensureSystemThemeListener(() => {
    const modeNow = normalizeThemeMode(config?.theme_mode);
    const settingsNow = normalizeSurfaceTheme(config?.theme_settings);
    const candNow = normalizeSurfaceTheme(config?.theme_cand);
    const ftbNow = normalizeSurfaceTheme(config?.theme_ftb);
    const menuNow = normalizeSurfaceTheme(config?.theme_menu);
    if (modeNow !== 'system' &&
        settingsNow !== 'follow' &&
        candNow !== 'follow' &&
        ftbNow !== 'follow' &&
        menuNow !== 'follow') {
      return;
    }
    applyThemeConfig(config);
  });

  updateSkinThemeCard(mode, settingsResolved, candResolved);
}

export function applyFtbPreviewTheme(theme: ResolvedTheme): void {
  document.querySelectorAll('.ftb-preview-host:not(#skinToolbarPreview)').forEach((element) => {
    element.classList.toggle('theme-light', theme === 'light');
    element.classList.toggle('theme-dark', theme === 'dark');
  });
}

function applyDropdownLabel(btnId: string, label: string): void {
  const span = document.querySelector<HTMLElement>(`#${btnId} span`);
  if (span) {
    span.textContent = label;
  }
}

function updateSkinThemeCard(
  mode: ThemeMode,
  settingsResolved: ResolvedTheme,
  candResolved: ResolvedTheme
): void {
  const description = document.getElementById('skinThemeStatus');
  if (description) {
    const modeLabel =
      mode === 'system' ? '跟随系统' : mode === 'light' ? '浅色' : '深色';
    description.textContent =
      `全局 ${modeLabel} · 设置 ${settingsResolved === 'light' ? '浅色' : '深色'} · 候选 ${candResolved === 'light' ? '浅色' : '深色'}`;
  }

  // Skin gallery follows the global theme; the switch button can preview the other one.
  syncSkinPreviewTheme(resolveTheme(mode, 'follow'));
}

export function setThemeMode(value: string | undefined): void {
  const mode = normalizeThemeMode(value);
  const current: ThemeConfig = {
    theme_mode: mode,
    theme_settings: readSurfaceFromUi('settingsThemeMenu', 'settingsThemeBtn'),
    theme_cand: readSurfaceFromUi('candThemeMenu', 'candThemeBtn'),
    theme_ftb: readSurfaceFromUi('ftbThemeMenu', 'ftbThemeBtn'),
    theme_menu: readSurfaceFromUi('menuThemeMenu', 'menuThemeBtn')
  };
  applyThemeConfig(current);
}

export function setSurfaceTheme(
  surface: 'settings' | 'cand' | 'ftb' | 'menu',
  value: string | undefined
): void {
  const current: ThemeConfig = {
    theme_mode: readModeFromUi(),
    theme_settings: readSurfaceFromUi('settingsThemeMenu', 'settingsThemeBtn'),
    theme_cand: readSurfaceFromUi('candThemeMenu', 'candThemeBtn'),
    theme_ftb: readSurfaceFromUi('ftbThemeMenu', 'ftbThemeBtn'),
    theme_menu: readSurfaceFromUi('menuThemeMenu', 'menuThemeBtn')
  };

  if (surface === 'settings') current.theme_settings = normalizeSurfaceTheme(value);
  if (surface === 'cand') current.theme_cand = normalizeSurfaceTheme(value);
  if (surface === 'ftb') current.theme_ftb = normalizeSurfaceTheme(value);
  if (surface === 'menu') current.theme_menu = normalizeSurfaceTheme(value);

  applyThemeConfig(current);
}

function readModeFromUi(): ThemeMode {
  const label = document.querySelector('#themeBtn span')?.textContent?.trim();
  if (label === '浅色') return 'light';
  if (label === '跟随系统') return 'system';
  return 'dark';
}

function readSurfaceFromUi(menuId: string, btnId: string): SurfaceTheme {
  const label = document.querySelector(`#${btnId} span`)?.textContent?.trim();
  if (label === '浅色') return 'light';
  if (label === '深色') return 'dark';
  const selected = document.querySelector<HTMLElement>(`#${menuId} .dropdown-item.active`);
  return normalizeSurfaceTheme(selected?.dataset.value);
}
