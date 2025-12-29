import './styles/index.css';

import { loadHTML, showOnlyCurrentModule } from './utils/common-utils';
import { loadContent, setupSidebar } from './modules/sidebar';

async function initializeApp() {
  // 加载侧边栏
  const sidebarHTML = await loadHTML('/src/partials/sidebar.html');
  document.getElementById('sidebar-container')!.innerHTML = sidebarHTML;

  const container = document.getElementById('content-container')!;
  container.style.visibility = 'hidden';

  // 先将所有的 sidebar 中的分区的 html 加载进来
  await loadContent('general');
  await loadContent('appearance');
  await loadContent('input');
  await loadContent('helpcode');

  // 加载默认内容(通用设置)，初始化功能
  // 隐藏其他的分区
  showOnlyCurrentModule('appearance');
  setupSidebar();

  container.style.visibility = 'visible';
}


document.addEventListener('DOMContentLoaded', initializeApp);