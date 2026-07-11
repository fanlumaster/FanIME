import { setupDropdownMenu } from './shared';
import { loadHTML } from '../utils/common-utils';

export async function setupAppearance() {
  // 候选窗口预览
  const wnd_v = document.getElementById('candidate-wnd-v')!;
  wnd_v.innerHTML = await loadHTML(`/src/partials/candidate/candidate-wnd-v.html`);
  const wnd_h = document.getElementById('candidate-wnd-h')!;
  wnd_h.innerHTML = await loadHTML(`/src/partials/candidate/candidate-wnd-h.html`);
  wnd_h.style.display = 'none';

  // 主题模式
  setupDropdownMenu('themeBtn', 'themeMenu', 'changeTheme');

  // 候选项排列方式
  setupDropdownMenu('arrangeBtn', 'arrangeMenu', 'changeCandidateArrange');
}
