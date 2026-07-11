import { setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

export function setupHelpcode(): void {
  // 双拼辅助码方案
  setupDropdownMenu('shuangpinHelpcodeSchemeBtn', 'shuangpinHelpcodeSchemeMenu', 'changeShuangpinScheme', true);

  // 全拼辅助码方案
  setupDropdownMenu('quanpinHelpcodeSchemeBtn', 'quanpinHelpcodeSchemeMenu', 'changeQuanpinScheme', true);

  // 双拼辅助码开关
  setupToggleButton('shuangpinHelpcodeToggleBtn');

  // 全拼辅助码开关
  setupToggleButton('quanpinHelpcodeToggleBtn');

  // 是否在候选窗口中显示双拼辅助码
  setupToggleButton('showShuangpinHelpcodeToggleBtn', (active) => {
    updateConfig('helpcode.show_sp_helpcode_in_candidate_window', active);
  });

  // 是否在候选窗口中显示全拼辅助码
  setupToggleButton('showQuanpinHelpcodeToggleBtn', (active) => {
    updateConfig('helpcode.show_qp_helpcode_in_candidate_window', active);
  });
}
