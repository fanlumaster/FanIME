import { setupDropdownMenu } from './shared';

export function setupAppearance(): void {
  // 主题模式
  setupDropdownMenu('themeBtn', 'themeMenu', 'changeTheme');

  // 候选项排列方式
  setupDropdownMenu('arrangeBtn', 'arrangeMenu', 'changeCandidateArrange');
}