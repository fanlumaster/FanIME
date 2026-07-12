import { setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

export function setupGeneral(): void {
  // 悬浮工具栏
  setupToggleButton('ftbToggleBtn', (active) => {
    updateConfig('general.floating_toolbar', active);
  });

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
      if (target.value === 'minus') {
        updateConfig('general.paging_minus_equal', target.checked);
      } else if (target.value === 'comma') {
        updateConfig('general.paging_comma_period', target.checked);
      } else if (target.value === 'tab') {
        updateConfig('general.paging_tab', target.checked);
      } else if (target.value === 'page') {
        updateConfig('general.paging_page_up_down', target.checked);
      } else if (target.value === 'arrow') {
        updateConfig('general.candidate_arrow_navigation', target.checked);
      }
    });
  });
}
