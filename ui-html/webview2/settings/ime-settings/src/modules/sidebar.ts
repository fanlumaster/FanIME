import { loadHTML, showOnlyCurrentModule } from '../utils/common-utils';
import { setupGeneral } from './general';
import { setupAppearance } from './appearance';
import { setupInput } from './input';
import { setupHelpcode } from './helpcode';

// 动态加载内容
export async function loadContent(moduleName: string) {
  const contentHTML = await loadHTML(`/src/partials/${moduleName}.html`);
  const container = document.getElementById(moduleName)!;

  container.innerHTML = contentHTML;

  // 根据加载的模块初始化对应的功能
  switch (moduleName) {
    case 'general':
      setupGeneral();
      break;
    case 'appearance':
      await setupAppearance();
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
  const moveIndicator = setupSidebarIndicator();
  const sidebarItems = document.querySelectorAll('.sidebar .item');

  sidebarItems.forEach((item: Element) => {
    item.addEventListener('click', () => {
      if (item.classList.contains('active')) {
        return;
      }
      const currentActive = document.querySelector('.sidebar .item.active');
      currentActive?.classList.remove('active');
      item.classList.add('active');
      moveIndicator(item as HTMLElement);

      const htmlItem = item as HTMLElement;
      const targetId = htmlItem.dataset.target;
      console.log(targetId);
      if (targetId) {
        showOnlyCurrentModule(targetId);
      }
    });
  });
}

function setupSidebarIndicator(): (item: HTMLElement) => void {
  const sidebar = document.querySelector('.sidebar') as HTMLElement | null;
  if (!sidebar) {
    return () => { };
  }

  const currentActive = sidebar.querySelector('.item.active') as HTMLElement | null;
  const indicator = document.createElement('div');
  indicator.className = 'active-indicator';
  indicator.style.transition = 'none';

  if (currentActive) {
    const initBarHeight = 18;
    const initBarLeft = currentActive.offsetLeft - 2;
    const initBarTop = currentActive.offsetTop + (currentActive.offsetHeight - initBarHeight) / 2;
    indicator.style.transform = `translate(${initBarLeft}px, ${initBarTop}px)`;
    indicator.style.opacity = '1';
    indicator.style.visibility = 'visible';
  } else {
    indicator.style.visibility = 'hidden';
  }

  sidebar.appendChild(indicator);
  let hasPositioned = Boolean(currentActive);

  if (currentActive) {
    requestAnimationFrame(() => {
      indicator.classList.add('ready');
      indicator.style.transition = '';
    });
  }

  const moveIndicator = (item: HTMLElement) => {
    const barHeight = 18;
    const barLeft = item.offsetLeft - 2;
    const barTop = item.offsetTop + (item.offsetHeight - barHeight) / 2;

    if (!hasPositioned) {
      hasPositioned = true;
      indicator.style.transform = `translate(${barLeft}px, ${barTop}px)`;
      indicator.style.opacity = '1';
      indicator.style.visibility = 'visible';
      indicator.classList.add('ready');
      indicator.style.transition = '';
      return;
    }

    indicator.style.transform = `translate(${barLeft}px, ${barTop}px)`;
    indicator.style.opacity = '1';
  };

  window.addEventListener('resize', () => {
    const activeItem = sidebar.querySelector('.item.active') as HTMLElement | null;
    if (activeItem) {
      moveIndicator(activeItem);
    }
  });

  return moveIndicator;
}
