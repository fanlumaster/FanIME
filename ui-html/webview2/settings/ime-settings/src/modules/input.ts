import { setupDropdownMenu } from './shared';

export function setupInput(): void {
  // 双拼方案
  setupDropdownMenu('shuangpinSchemeBtn', 'shuangpinSchemeMenu', 'changeShuangpinScheme', true);
}