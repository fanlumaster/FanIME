import { applyDropdownValue, populateDropdownMenu, setupDropdownMenu } from './shared';
import { loadHTML } from '../utils/common-utils';
import { applyThemeConfig, type ResolvedTheme, type ThemeConfig } from './theme';

function postConfigUpdate(path: string, value: string | number): void {
  window.chrome?.webview?.postMessage(
    JSON.stringify({
      type: 'configUpdate',
      data: { path, value }
    })
  );
}

export type CandidateAppearanceConfig = {
  font?: string;
  english_font?: string;
  default_font?: string;
  font_size?: number;
  cand_text_color?: string;
  page_size?: number;
  system_fonts?: string[];
};

export type CandidatePreviewHelpcodeConfig = {
  input_schema?: string;
  shuangpin_helpcode?: boolean;
  quanpin_helpcode?: boolean;
  show_sp_helpcode_in_candidate_window?: boolean;
  show_qp_helpcode_in_candidate_window?: boolean;
};

let previewFont = 'Noto Sans SC';
let previewEnglishFont = 'Segoe UI';
let previewDefaultFont = 'Microsoft YaHei';
let previewFontSize = 16;
let previewTextColor = 'auto';
let previewPageSize = 8;
let previewCandTheme: ResolvedTheme = 'dark';
let previewInputSchema = 'shuangpin';
let previewShuangpinHelpcode = true;
let previewQuanpinHelpcode = true;
let previewShowSpHelpcode = true;
let previewShowQpHelpcode = true;

const FALLBACK_CJK_FONTS = [
  'Noto Sans SC',
  'Microsoft YaHei',
  'Microsoft YaHei UI',
  'SimHei',
  'SimSun',
  'DengXian',
  'KaiTi',
  'FangSong'
];

const FALLBACK_ENGLISH_FONTS = [
  'Segoe UI',
  'Arial',
  'Calibri',
  'Consolas',
  'Cascadia Mono',
  'Times New Roman',
  'Courier New',
  'Tahoma'
];

function quoteFont(name: string): string {
  return /\s/.test(name) ? `"${name.replace(/"/g, '\\"')}"` : name;
}

function appearancePreviewRoots(): HTMLElement[] {
  return Array.from(document.querySelectorAll<HTMLElement>('.cand-preview .candidate'));
}

function themeFallbackColor(): string {
  return previewCandTheme === 'light' ? '#1a1a1a' : '#e9e8e8';
}

function shouldShowHelpcodeInPreview(): boolean {
  if (previewInputSchema === 'quanpin') {
    return previewQuanpinHelpcode && previewShowQpHelpcode;
  }
  if (previewInputSchema === 'shuangpin') {
    return previewShuangpinHelpcode && previewShowSpHelpcode;
  }
  return false;
}

export function applyCandidatePreviewHelpcode(): void {
  const show = shouldShowHelpcodeInPreview();
  document.querySelectorAll<HTMLElement>('.cand-preview .cand-helpcode').forEach((el) => {
    el.hidden = !show;
  });
}

export function updateCandidatePreviewHelpcode(config: CandidatePreviewHelpcodeConfig): void {
  if (typeof config.input_schema === 'string') {
    previewInputSchema = config.input_schema;
  }
  if (typeof config.shuangpin_helpcode === 'boolean') {
    previewShuangpinHelpcode = config.shuangpin_helpcode;
  }
  if (typeof config.quanpin_helpcode === 'boolean') {
    previewQuanpinHelpcode = config.quanpin_helpcode;
  }
  if (typeof config.show_sp_helpcode_in_candidate_window === 'boolean') {
    previewShowSpHelpcode = config.show_sp_helpcode_in_candidate_window;
  }
  if (typeof config.show_qp_helpcode_in_candidate_window === 'boolean') {
    previewShowQpHelpcode = config.show_qp_helpcode_in_candidate_window;
  }
  applyCandidatePreviewHelpcode();
}

function applyCandidatePreviewStyle(): void {
  // English font first so Latin glyphs prefer it; CJK falls through to Chinese font.
  const family = [previewEnglishFont, previewFont, previewDefaultFont, 'sans-serif']
    .filter(Boolean)
    .map(quoteFont)
    .join(', ');
  appearancePreviewRoots().forEach((el) => {
    el.style.setProperty('--cand-font-family', family);
    el.style.setProperty('--cand-font-size', `${previewFontSize}px`);
    el.style.fontFamily = family;
    el.style.fontSize = `${previewFontSize}px`;
    if (previewTextColor && previewTextColor !== 'auto') {
      el.style.setProperty('--cand-text', previewTextColor);
      el.style.setProperty('--cand-num', previewTextColor.length === 7 ? `${previewTextColor}9d` : previewTextColor);
    } else {
      el.style.removeProperty('--cand-text');
      el.style.removeProperty('--cand-num');
    }

    el.querySelectorAll<HTMLElement>('.container').forEach((container) => {
      container.style.fontFamily = family;
      container.style.fontSize = `${previewFontSize}px`;
    });

    el.querySelectorAll<HTMLElement>('.row-wrapper').forEach((wrapper, index) => {
      wrapper.style.display = index < previewPageSize ? '' : 'none';
    });
  });
}

function syncColorControls(color: string | undefined): void {
  const input = document.getElementById('candTextColorInput') as HTMLInputElement | null;
  const resetBtn = document.getElementById('candTextColorResetBtn');
  const normalized = !color || color === 'auto' ? 'auto' : color;
  previewTextColor = normalized;
  if (input) {
    input.value = normalized === 'auto' ? themeFallbackColor() : normalized;
  }
  resetBtn?.classList.toggle('is-active', normalized === 'auto');
}

/** Called when candidate-surface theme (or global follow) resolves to a new dark/light. */
export function onCandidateSurfaceThemeChanged(theme: ResolvedTheme): void {
  previewCandTheme = theme;
  if (previewTextColor === 'auto') {
    syncColorControls('auto');
    applyCandidatePreviewStyle();
  }
}

function populateFontMenus(systemFonts: string[] | undefined): void {
  const fonts =
    systemFonts && systemFonts.length > 0
      ? systemFonts
      : [...FALLBACK_CJK_FONTS, ...FALLBACK_ENGLISH_FONTS];
  populateDropdownMenu('candFontMenu', fonts, { selected: previewFont, previewFont: true });
  populateDropdownMenu('candEnglishFontMenu', fonts, {
    selected: previewEnglishFont,
    previewFont: true
  });
  applyDropdownValue('candFontBtn', 'candFontMenu', previewFont);
  applyDropdownValue('candEnglishFontBtn', 'candEnglishFontMenu', previewEnglishFont);
}

export function applyAppearanceConfig(
  candidateWindowPreeditStyle: string | undefined,
  tsfPreeditStyle: string | undefined,
  themeConfig?: ThemeConfig,
  candidateAppearance?: CandidateAppearanceConfig
): void {
  applyDropdownValue('candPreeditStyleBtn', 'candPreeditStyleMenu', candidateWindowPreeditStyle);
  applyDropdownValue('tsfPreeditStyleBtn', 'tsfPreeditStyleMenu', tsfPreeditStyle);
  if (themeConfig) {
    applyThemeConfig(themeConfig);
  }

  if (candidateAppearance?.font) {
    previewFont = candidateAppearance.font;
  }
  if (candidateAppearance?.english_font) {
    previewEnglishFont = candidateAppearance.english_font;
  }
  if (candidateAppearance?.default_font) {
    previewDefaultFont = candidateAppearance.default_font;
  }
  if (typeof candidateAppearance?.font_size === 'number') {
    previewFontSize = candidateAppearance.font_size;
    applyDropdownValue('candFontSizeBtn', 'candFontSizeMenu', String(candidateAppearance.font_size));
  }
  if (typeof candidateAppearance?.page_size === 'number') {
    previewPageSize = candidateAppearance.page_size;
    applyDropdownValue('candPageSizeBtn', 'candPageSizeMenu', String(candidateAppearance.page_size));
  }
  populateFontMenus(candidateAppearance?.system_fonts);
  syncColorControls(candidateAppearance?.cand_text_color);
  applyCandidatePreviewStyle();
  applyCandidatePreviewHelpcode();
}

export async function setupAppearance() {
  // 候选窗口预览
  const wnd_v = document.getElementById('candidate-wnd-v')!;
  wnd_v.innerHTML = await loadHTML(`/src/partials/candidate/candidate-wnd-v.html`);
  const wnd_h = document.getElementById('candidate-wnd-h')!;
  wnd_h.innerHTML = await loadHTML(`/src/partials/candidate/candidate-wnd-h.html`);
  wnd_h.style.display = 'none';
  populateFontMenus(undefined);
  applyCandidatePreviewStyle();
  applyCandidatePreviewHelpcode();

  // 主题模式（全局）
  setupDropdownMenu('themeBtn', 'themeMenu', 'changeTheme', false, 'appearance.theme_mode');

  // 分表面主题
  setupDropdownMenu(
    'settingsThemeBtn',
    'settingsThemeMenu',
    'changeSettingsTheme',
    false,
    'appearance.theme_settings'
  );
  setupDropdownMenu('candThemeBtn', 'candThemeMenu', 'changeCandTheme', false, 'appearance.theme_cand');
  setupDropdownMenu('ftbThemeBtn', 'ftbThemeMenu', 'changeFtbTheme', false, 'appearance.theme_ftb');
  setupDropdownMenu('menuThemeBtn', 'menuThemeMenu', 'changeMenuTheme', false, 'appearance.theme_menu');

  // 候选项排列方式
  setupDropdownMenu('arrangeBtn', 'arrangeMenu', 'changeCandidateArrange');

  // 候选窗样式
  setupDropdownMenu('candFontBtn', 'candFontMenu', '', true, 'appearance.font', (value) => {
    previewFont = value;
    applyCandidatePreviewStyle();
    return value;
  });
  setupDropdownMenu('candEnglishFontBtn', 'candEnglishFontMenu', '', true, 'appearance.english_font', (value) => {
    previewEnglishFont = value;
    applyCandidatePreviewStyle();
    return value;
  });
  setupDropdownMenu('candFontSizeBtn', 'candFontSizeMenu', '', true, 'appearance.font_size', (value) => {
    previewFontSize = Number(value) || 16;
    applyCandidatePreviewStyle();
    return previewFontSize;
  });
  setupDropdownMenu('candPageSizeBtn', 'candPageSizeMenu', '', true, 'appearance.page_size', (value) => {
    previewPageSize = Number(value) || 8;
    applyCandidatePreviewStyle();
    return previewPageSize;
  });

  const colorInput = document.getElementById('candTextColorInput') as HTMLInputElement | null;
  colorInput?.addEventListener('input', () => {
    const value = colorInput.value;
    previewTextColor = value;
    syncColorControls(value);
    applyCandidatePreviewStyle();
    postConfigUpdate('appearance.cand_text_color', value);
  });
  document.getElementById('candTextColorResetBtn')?.addEventListener('click', () => {
    previewTextColor = 'auto';
    syncColorControls('auto');
    applyCandidatePreviewStyle();
    postConfigUpdate('appearance.cand_text_color', 'auto');
  });

  // 候选窗预编辑
  setupDropdownMenu(
    'candPreeditStyleBtn',
    'candPreeditStyleMenu',
    'changeCandPreeditStyle',
    true,
    'appearance.candidate_window_preedit_style'
  );

  // 行内预编辑
  setupDropdownMenu(
    'tsfPreeditStyleBtn',
    'tsfPreeditStyleMenu',
    'changeTsfPreeditStyle',
    true,
    'appearance.tsf_preedit_style'
  );
}
