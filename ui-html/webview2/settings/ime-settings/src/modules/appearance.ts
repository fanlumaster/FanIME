import { serializeHostMessage } from '../../../../shared/messages';
import {
  applyDropdownValue,
  applyToggleState,
  populateDropdownMenu,
  registerDropdownPreparer,
  setupDropdownMenu,
  setupToggleButton
} from './shared';
import { loadHTML } from '../utils/common-utils';
import { applyThemeConfig, setCandidateSurfaceThemeListener, type ResolvedTheme, type ThemeConfig } from './theme';

function postConfigUpdate(path: string, value: string | number | boolean): void {
  window.chrome?.webview?.postMessage(
    serializeHostMessage({
      type: 'configUpdate',
      data: { path, value }
    })
  );
}

export type CandidateAppearanceConfig = {
  font?: string;
  font_css_family?: string;
  english_font?: string;
  english_font_css_family?: string;
  default_font?: string;
  default_font_css_family?: string;
  font_size?: number;
  candidate_window_preedit_font_size?: number;
  cand_text_color?: string;
  page_size?: number;
  candidate_window_follow_cursor?: boolean;
  ui_backend?: string;
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
// Keep persisted/menu labels separate from the family Chromium actually uses.
// Some fonts expose a GDI face name with a weight suffix but a different
// OpenType typographic family name.
let previewFontCssFamily = previewFont;
let previewEnglishFontCssFamily = previewEnglishFont;
let previewDefaultFontCssFamily = previewDefaultFont;
let previewFontSize = 16;
let previewPreeditFontSize = 16;
let previewTextColor = 'auto';
let previewPageSize = 6;
let previewPreeditStyle = 'pinyin';
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

function applyCandidatePreviewPreedit(): void {
  const hide = previewPreeditStyle === 'empty';
  appearancePreviewRoots().forEach((el) => {
    el.querySelectorAll<HTMLElement>('.container').forEach((container) => {
      container.classList.toggle('preedit-hidden', hide);
    });
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
  const family = [previewEnglishFontCssFamily, previewFontCssFamily, previewDefaultFontCssFamily, 'sans-serif']
    .filter(Boolean)
    .map(quoteFont)
    .join(', ');
  appearancePreviewRoots().forEach((el) => {
    el.style.setProperty('--cand-font-family', family);
    el.style.setProperty('--cand-font-size', `${previewFontSize}px`);
    el.style.setProperty('--preedit-font-size', `${previewPreeditFontSize}px`);
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

setCandidateSurfaceThemeListener(onCandidateSurfaceThemeChanged);

type FontMenu = {
  btnId: string;
  menuId: string;
  getSelected: () => string;
  getSelectedCssFamily: () => string;
};

const FONT_MENUS: FontMenu[] = [
  {
    btnId: 'candFontBtn',
    menuId: 'candFontMenu',
    getSelected: () => previewFont,
    getSelectedCssFamily: () => previewFontCssFamily
  },
  {
    btnId: 'candEnglishFontBtn',
    menuId: 'candEnglishFontMenu',
    getSelected: () => previewEnglishFont,
    getSelectedCssFamily: () => previewEnglishFontCssFamily
  }
];

let fontList: string[] = [...FALLBACK_CJK_FONTS, ...FALLBACK_ENGLISH_FONTS];
let fontListVersion = 0;

/** Version of `fontList` each menu was built from, plus the extra row it got for an uninstalled font. */
const builtFontMenus = new Map<string, { version: number; extra: string | null }>();

function fontMenuNeedsBuild(menu: FontMenu): boolean {
  const built = builtFontMenus.get(menu.menuId);
  if (!built || built.version !== fontListVersion) {
    return true;
  }
  const selected = menu.getSelected();
  return Boolean(selected) && built.extra !== selected && !fontList.includes(selected);
}

function buildFontMenu(menu: FontMenu): void {
  const selected = menu.getSelected();
  populateDropdownMenu(menu.menuId, fontList, {
    selected,
    previewFont: true,
    previewFamily: (name) => (name === selected ? menu.getSelectedCssFamily() : name)
  });
  builtFontMenus.set(menu.menuId, {
    version: fontListVersion,
    extra: selected && !fontList.includes(selected) ? selected : null
  });
  applyDropdownValue(menu.btnId, menu.menuId, selected);
}

function filterFontMenu(menu: FontMenu, query: string): void {
  const menuElement = document.getElementById(menu.menuId);
  if (!menuElement) {
    return;
  }

  const normalizedQuery = query.trim().toLocaleLowerCase();
  let visibleCount = 0;
  menuElement.querySelectorAll<HTMLElement>('.dropdown-item').forEach((item) => {
    const matches = !normalizedQuery || (item.dataset.value || '').toLocaleLowerCase().includes(normalizedQuery);
    item.hidden = !matches;
    if (matches) {
      visibleCount += 1;
    }
  });

  menuElement.querySelector('.font-dropdown-empty')?.remove();
  if (visibleCount === 0) {
    const empty = document.createElement('div');
    empty.className = 'font-dropdown-empty';
    empty.textContent = '没有匹配的字体';
    menuElement.appendChild(empty);
  }
  menuElement.scrollTop = 0;
}

function setupFontSearch(menu: FontMenu): void {
  const control = document.getElementById(menu.btnId);
  const menuElement = document.getElementById(menu.menuId);
  const input = control?.querySelector<HTMLInputElement>('.font-search-input');
  if (!control || !menuElement || !input) {
    return;
  }

  const reset = () => {
    input.value = menu.getSelected();
    filterFontMenu(menu, '');
  };

  // Keep the input focused until the delegated click handler has committed the
  // selected row. Otherwise blur may close/reset the menu between mousedown
  // and click, making a filtered result appear unresponsive.
  menuElement.addEventListener('mousedown', (event) => {
    if ((event.target as HTMLElement | null)?.closest('.dropdown-item')) {
      event.preventDefault();
    }
  });
  input.addEventListener('focus', () => input.select());
  input.addEventListener('input', () => filterFontMenu(menu, input.value));
  input.addEventListener('blur', () => {
    // Let a dropdown-item click finish first, then restore the persisted value
    // when keyboard focus leaves the whole combobox.
    setTimeout(() => {
      const active = document.activeElement;
      if (!active || (!control.contains(active) && !menuElement.contains(active))) {
        menuElement.classList.remove('open');
        reset();
      }
    }, 0);
  });
  input.addEventListener('keydown', (event) => {
    if (event.key !== 'Escape') {
      return;
    }
    event.stopPropagation();
    if (input.value !== menu.getSelected()) {
      input.value = '';
      filterFontMenu(menu, '');
    } else {
      menuElement.classList.remove('open');
      input.blur();
    }
  });
  document.addEventListener('click', (event) => {
    if (!control.contains(event.target as Node) && !menuElement.contains(event.target as Node)) {
      reset();
    }
  });
}

function populateFontMenus(systemFonts: string[] | undefined): void {
  const fonts =
    systemFonts && systemFonts.length > 0
      ? systemFonts
      : [...FALLBACK_CJK_FONTS, ...FALLBACK_ENGLISH_FONTS];
  if (fonts.length !== fontList.length || fonts.some((name, index) => name !== fontList[index])) {
    fontList = fonts;
    fontListVersion += 1;
  }
  // Rows are built on first open, so the label falls back to the raw font name.
  FONT_MENUS.forEach((menu) => applyDropdownValue(menu.btnId, menu.menuId, menu.getSelected()));
}

export function applyAppearanceConfig(
  candidateWindowPreeditStyle: string | undefined,
  tsfPreeditStyle: string | undefined,
  themeConfig?: ThemeConfig,
  candidateAppearance?: CandidateAppearanceConfig
): void {
  applyDropdownValue('candPreeditStyleBtn', 'candPreeditStyleMenu', candidateWindowPreeditStyle);
  applyDropdownValue('tsfPreeditStyleBtn', 'tsfPreeditStyleMenu', tsfPreeditStyle);
  if (candidateWindowPreeditStyle) {
    previewPreeditStyle = candidateWindowPreeditStyle === 'empty' ? 'empty' : 'pinyin';
  }
  if (themeConfig) {
    applyThemeConfig(themeConfig);
  }

  if (candidateAppearance?.font) {
    previewFont = candidateAppearance.font;
    previewFontCssFamily = candidateAppearance.font_css_family || previewFont;
  }
  if (candidateAppearance?.english_font) {
    previewEnglishFont = candidateAppearance.english_font;
    previewEnglishFontCssFamily = candidateAppearance.english_font_css_family || previewEnglishFont;
  }
  if (candidateAppearance?.default_font) {
    previewDefaultFont = candidateAppearance.default_font;
    previewDefaultFontCssFamily = candidateAppearance.default_font_css_family || previewDefaultFont;
  }
  if (typeof candidateAppearance?.font_size === 'number') {
    previewFontSize = candidateAppearance.font_size;
    applyDropdownValue('candFontSizeBtn', 'candFontSizeMenu', String(candidateAppearance.font_size));
  }
  if (typeof candidateAppearance?.candidate_window_preedit_font_size === 'number') {
    previewPreeditFontSize = candidateAppearance.candidate_window_preedit_font_size;
    applyDropdownValue('candPreeditFontSizeBtn', 'candPreeditFontSizeMenu', String(previewPreeditFontSize));
  } else {
    previewPreeditFontSize = previewFontSize;
  }
  if (typeof candidateAppearance?.page_size === 'number') {
    previewPageSize = candidateAppearance.page_size;
    applyDropdownValue('candPageSizeBtn', 'candPageSizeMenu', String(candidateAppearance.page_size));
  }
  if (typeof candidateAppearance?.candidate_window_follow_cursor === 'boolean') {
    applyToggleState('candidateFollowCursorToggleBtn', candidateAppearance.candidate_window_follow_cursor);
  }
  applyDropdownValue('uiBackendBtn', 'uiBackendMenu', candidateAppearance?.ui_backend);
  populateFontMenus(candidateAppearance?.system_fonts);
  syncColorControls(candidateAppearance?.cand_text_color);
  applyCandidatePreviewStyle();
  applyCandidatePreviewHelpcode();
  applyCandidatePreviewPreedit();
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
  applyCandidatePreviewPreedit();

  // 主题模式（全局）
  setupDropdownMenu('themeBtn', 'themeMenu', 'changeTheme', false, 'appearance.theme_mode');
  const themeSurfaceExpand = document.getElementById('themeSurfaceExpand');
  const themeSurfaceDetails = document.getElementById('themeSurfaceDetails');
  themeSurfaceExpand?.addEventListener('click', () => {
    const expanded = themeSurfaceExpand.getAttribute('aria-expanded') !== 'true';
    themeSurfaceExpand.setAttribute('aria-expanded', String(expanded));
    themeSurfaceDetails?.classList.toggle('open', expanded);
  });

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
  setupDropdownMenu('emojiThemeBtn', 'emojiThemeMenu', 'changeEmojiTheme', false, 'appearance.theme_emoji');
  setupDropdownMenu('screenKeyboardThemeBtn', 'screenKeyboardThemeMenu', 'changeScreenKeyboardTheme', false, 'appearance.theme_screen_keyboard');
  setupDropdownMenu('handwritingThemeBtn', 'handwritingThemeMenu', 'changeHandwritingTheme', false, 'appearance.theme_handwriting');
  setupDropdownMenu('voiceThemeBtn', 'voiceThemeMenu', 'changeVoiceTheme', false, 'appearance.theme_voice');

  setupDropdownMenu('uiBackendBtn', 'uiBackendMenu', '', true, 'appearance.ui_backend');

  // 候选项排列方式
  setupDropdownMenu('arrangeBtn', 'arrangeMenu', 'changeCandidateArrange');
  setupToggleButton('candidateFollowCursorToggleBtn', (active) => {
    postConfigUpdate('appearance.candidate_window_follow_cursor', active);
  });

  // 候选窗样式
  FONT_MENUS.forEach((menu) => {
    registerDropdownPreparer(menu.menuId, {
      isPending: () => fontMenuNeedsBuild(menu),
      prepare: () => buildFontMenu(menu)
    });
    setupFontSearch(menu);
  });
  setupDropdownMenu('candFontBtn', 'candFontMenu', '', true, 'appearance.font', (value) => {
    const cssFamily = value === previewFont ? previewFontCssFamily : value;
    previewFont = value;
    previewFontCssFamily = cssFamily;
    applyCandidatePreviewStyle();
    filterFontMenu(FONT_MENUS[0], '');
    return value;
  });
  setupDropdownMenu('candEnglishFontBtn', 'candEnglishFontMenu', '', true, 'appearance.english_font', (value) => {
    const cssFamily = value === previewEnglishFont ? previewEnglishFontCssFamily : value;
    previewEnglishFont = value;
    previewEnglishFontCssFamily = cssFamily;
    applyCandidatePreviewStyle();
    filterFontMenu(FONT_MENUS[1], '');
    return value;
  });
  setupDropdownMenu('candFontSizeBtn', 'candFontSizeMenu', '', true, 'appearance.font_size', (value) => {
    previewFontSize = Number(value) || 16;
    applyCandidatePreviewStyle();
    return previewFontSize;
  });
  setupDropdownMenu(
    'candPreeditFontSizeBtn',
    'candPreeditFontSizeMenu',
    '',
    true,
    'appearance.candidate_window_preedit_font_size',
    (value) => {
      previewPreeditFontSize = Number(value) || 16;
      applyCandidatePreviewStyle();
      return previewPreeditFontSize;
    }
  );
  setupDropdownMenu('candPageSizeBtn', 'candPageSizeMenu', '', true, 'appearance.page_size', (value) => {
    previewPageSize = Number(value) || 6;
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
    'appearance.candidate_window_preedit_style',
    (value) => {
      previewPreeditStyle = value === 'empty' ? 'empty' : 'pinyin';
      applyCandidatePreviewPreedit();
      return previewPreeditStyle;
    }
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
