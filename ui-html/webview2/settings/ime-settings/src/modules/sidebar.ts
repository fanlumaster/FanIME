import { loadHTML, showOnlyCurrentModule } from '../utils/common-utils';
import { setupAppearance } from './appearance';
import { setupInput } from './input';
import { setupHelpcode } from './helpcode';
import { setupFloatingToolbar } from './floating-toolbar';
import { setupVoiceInput } from './voice';
import { setupScreenKeyboardSettings } from './screenkb-settings';
import { setupHandwritingSettings } from './handwriting-settings';
import { setupDictionary } from './dict';
import { setupSkin } from './skin';
import { setupToolsSettings } from './tools-settings';
import { setupAiSettings } from './ai-settings';
import { setupFeedbackSettings } from './feedback-settings';
import { setupShortcut } from './shortcut';
import { setupAboutSettings } from './about-settings';

// 动态加载内容
export async function loadContent(moduleName: string) {
  const contentHTML = await loadHTML(`/src/partials/${moduleName}.html`);
  const container = document.getElementById(moduleName)!;

  container.innerHTML = contentHTML;

  // 根据加载的模块初始化对应的功能
  switch (moduleName) {
    case 'floating-toolbar':
      setupFloatingToolbar();
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
    case 'dict':
      setupDictionary();
      break;
    case 'skin':
      await setupSkin();
      break;
    case 'voice':
      setupVoiceInput();
      break;
    case 'screenkb-settings':
      setupScreenKeyboardSettings();
      break;
    case 'handwriting-settings':
      setupHandwritingSettings();
      break;
    case 'tools-settings':
      setupToolsSettings();
      break;
    case 'ai-settings':
      setupAiSettings();
      break;
    case 'shortcut':
      setupShortcut();
      break;
    case 'about-settings':
      setupAboutSettings();
      break;
    case 'feedback-settings':
      setupFeedbackSettings();
      break;
  }
}

export function setupSidebar(): void {
  const moveIndicator = setupSidebarIndicator();
  const sidebarItems = document.querySelectorAll('.sidebar .item');
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  sidebarItems.forEach((item: Element) => {
    item.addEventListener('click', () => {
      if (item.classList.contains('active')) {
        return;
      }
      const currentActive = document.querySelector('.sidebar .item.active');
      currentActive?.classList.remove('active');
      item.classList.add('active');

      const htmlItem = item as HTMLElement;
      // 先把指示条交给合成层动画，再做右侧换页，避免重排打断滑动。
      moveIndicator(htmlItem, { animate: !reduceMotion });

      const targetId = htmlItem.dataset.target;
      if (targetId) {
        showOnlyCurrentModule(targetId);
      }
    });
  });
}

type MoveIndicatorOptions = {
  animate?: boolean;
};

function setupSidebarIndicator(): (item: HTMLElement, options?: MoveIndicatorOptions) => void {
  const sidebar = document.querySelector('.sidebar') as HTMLElement | null;
  if (!sidebar) {
    return () => { };
  }

  const currentActive = sidebar.querySelector('.item.active') as HTMLElement | null;
  const indicator = document.createElement('div');
  indicator.className = 'active-indicator';

  let currentX = 0;
  let currentY = 0;
  let hasPositioned = false;
  let activeAnimation: Animation | null = null;

  const readTarget = (item: HTMLElement) => {
    const barHeight = 22;
    return {
      x: item.offsetLeft - 2,
      y: item.offsetTop + (item.offsetHeight - barHeight) / 2,
    };
  };

  const commitTransform = (x: number, y: number) => {
    currentX = x;
    currentY = y;
    indicator.style.transform = `translate3d(${x}px, ${y}px, 0)`;
  };

  const cancelActiveAnimation = () => {
    if (!activeAnimation) {
      return;
    }
    try {
      activeAnimation.commitStyles();
    } catch {
      // ignore: element may already match the target style
    }
    activeAnimation.cancel();
    activeAnimation = null;

    const matrix = new DOMMatrix(getComputedStyle(indicator).transform);
    currentX = matrix.m41;
    currentY = matrix.m42;
  };

  const durationForDistance = (dx: number, dy: number) => {
    const distance = Math.hypot(dx, dy);
    // 相邻项距离短，用更短时长，避免强 ease-out 在末段「粘滞」。
    return Math.round(Math.min(280, Math.max(140, 110 + distance * 0.9)));
  };

  if (currentActive) {
    const { x, y } = readTarget(currentActive);
    commitTransform(x, y);
    indicator.style.opacity = '1';
    indicator.style.visibility = 'visible';
    hasPositioned = true;
  } else {
    indicator.style.visibility = 'hidden';
  }

  sidebar.appendChild(indicator);

  const moveIndicator = (item: HTMLElement, options: MoveIndicatorOptions = {}) => {
    const { x, y } = readTarget(item);
    const shouldAnimate = options.animate !== false;

    if (!hasPositioned) {
      hasPositioned = true;
      commitTransform(x, y);
      indicator.style.opacity = '1';
      indicator.style.visibility = 'visible';
      return;
    }

    if (!shouldAnimate) {
      cancelActiveAnimation();
      commitTransform(x, y);
      indicator.style.opacity = '1';
      return;
    }

    cancelActiveAnimation();
    const duration = durationForDistance(x - currentX, y - currentY);
    const targetTransform = `translate3d(${x}px, ${y}px, 0)`;

    activeAnimation = indicator.animate(
      [
        { transform: `translate3d(${currentX}px, ${currentY}px, 0)` },
        { transform: targetTransform },
      ],
      {
        duration,
        easing: 'cubic-bezier(0.22, 1, 0.36, 1)',
        fill: 'forwards',
      },
    );

    const animation = activeAnimation;
    animation.finished.then(() => {
      if (activeAnimation !== animation) {
        return;
      }
      try {
        animation.commitStyles();
      } catch {
        indicator.style.transform = targetTransform;
      }
      animation.cancel();
      activeAnimation = null;
      currentX = x;
      currentY = y;
    }).catch(() => {
      // cancelled
    });

    indicator.style.opacity = '1';
  };

  window.addEventListener('resize', () => {
    const activeItem = sidebar.querySelector('.item.active') as HTMLElement | null;
    if (activeItem) {
      moveIndicator(activeItem, { animate: false });
    }
  });

  return moveIndicator;
}
