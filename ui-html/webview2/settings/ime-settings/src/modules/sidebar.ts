import { loadHTML } from '../utils/common-utils';
import { setupGeneral } from './general';
import { setupAppearance } from './appearance';
import { setupInput } from './input';
import { setupHelpcode } from './helpcode';

// 动态加载内容
export async function loadContent(moduleName: string) {
  const contentHTML = await loadHTML(`/src/partials/${moduleName}.html`);
  document.getElementById('content-container')!.innerHTML = contentHTML;

  // 根据加载的模块初始化对应的功能
  switch (moduleName) {
    case 'general':
      setupGeneral();
      break;
    case 'appearance':
      setupAppearance();
      break;
    case 'input':
      setupInput();
      break;
    case 'helpcode':
      setupHelpcode();
      break;
  }
}

export function setupSidebar(): void {
  const sidebarItems = document.querySelectorAll('.sidebar .item');

  sidebarItems.forEach((item: Element) => {
    item.addEventListener('click', () => {
      const currentActive = document.querySelector('.sidebar .item.active');
      currentActive?.classList.remove('active');
      item.classList.add('active');

      const htmlItem = item as HTMLElement;
      const targetId = htmlItem.dataset.target;
      console.log(targetId);
      if (targetId) {
        loadContent(targetId);
      }
    });
  });
}