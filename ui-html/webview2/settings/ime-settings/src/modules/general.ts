import { setupToggleButton } from './shared';

export function setupGeneral(): void {
  // 悬浮工具栏
  setupToggleButton('ftbToggleBtn');

  // 设置翻页方式复选框
  setupPageOptions();

  // 中英混输开关
  setupToggleButton('zhEnToggleBtn');
}

function setupPageOptions(): void {
  const checkboxes = document.querySelectorAll('input[name="page-method"]');
  checkboxes.forEach((checkbox: Element) => {
    checkbox.addEventListener('change', (e: Event) => {
      const target = e.target as HTMLInputElement;
      if (target.checked) {
      }
    });
  });
}