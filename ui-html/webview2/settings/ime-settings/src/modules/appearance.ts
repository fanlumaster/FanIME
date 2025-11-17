import { setupDropdownMenu, setupToggleButton } from './shared';
import { loadHTML } from '../utils/common-utils';

export async function setupAppearance() {
  // 候选窗口预览
  const contentHTML = await loadHTML(`/src/partials/candidate/candidate-wnd.html`);
  document.getElementById('candidate-wnd')!.innerHTML = contentHTML;

  // 主题模式
  setupDropdownMenu('themeBtn', 'themeMenu', 'changeTheme');

  // 候选项排列方式
  setupDropdownMenu('arrangeBtn', 'arrangeMenu', 'changeCandidateArrange');

  // 是否显示双拼键位提示图片
  setupToggleButton('auxSpPicToggleBtn');
}