import { setupDropdownMenu, setupToggleButton } from './shared';

export function setupHelpcode(): void {
  // 双拼辅助码方案
  setupDropdownMenu('shuangpinHelpcodeSchemeBtn', 'shuangpinHelpcodeSchemeMenu', 'changeShuangpinScheme', true);

  // 全拼辅助码方案
  setupDropdownMenu('quanpinHelpcodeSchemeBtn', 'quanpinHelpcodeSchemeMenu', 'changeQuanpinScheme', true);

  // 双拼辅助码开关
  setupToggleButton('shuangpinHelpcodeToggleBtn');

  // 全拼辅助码开关
  setupToggleButton('quanpinHelpcodeToggleBtn');
}