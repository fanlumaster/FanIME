import { applyDropdownValue, setupDropdownMenu } from './shared';
import { loadHTML } from '../utils/common-utils';
import { applyThemeConfig, type ThemeConfig } from './theme';

export function applyAppearanceConfig(
  candidateWindowPreeditStyle: string | undefined,
  tsfPreeditStyle: string | undefined,
  themeConfig?: ThemeConfig
): void {
  applyDropdownValue('candPreeditStyleBtn', 'candPreeditStyleMenu', candidateWindowPreeditStyle);
  applyDropdownValue('tsfPreeditStyleBtn', 'tsfPreeditStyleMenu', tsfPreeditStyle);
  if (themeConfig) {
    applyThemeConfig(themeConfig);
  }
}

export async function setupAppearance() {
  // 候选窗口预览
  const wnd_v = document.getElementById('candidate-wnd-v')!;
  wnd_v.innerHTML = await loadHTML(`/src/partials/candidate/candidate-wnd-v.html`);
  const wnd_h = document.getElementById('candidate-wnd-h')!;
  wnd_h.innerHTML = await loadHTML(`/src/partials/candidate/candidate-wnd-h.html`);
  wnd_h.style.display = 'none';

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
