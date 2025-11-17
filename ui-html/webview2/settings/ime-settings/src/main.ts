import './styles/index.css';

import { loadHTML } from './utils/common-utils';
import { loadContent, setupSidebar } from './modules/sidebar';

async function initializeApp() {
  // 加载侧边栏
  const sidebarHTML = await loadHTML('/src/partials/sidebar.html');
  document.getElementById('sidebar-container')!.innerHTML = sidebarHTML;

  // 加载默认内容(通用设置)，初始化功能
  await loadContent('general');
  setupSidebar();
}


document.addEventListener('DOMContentLoaded', initializeApp);