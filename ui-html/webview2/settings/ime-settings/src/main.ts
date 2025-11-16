import './styles/index.css';
// import './style.css';

import { loadHTML } from './utils/common-utils';
import { loadContent, setupSidebar } from './modules/sidebar';
import { setupGeneral } from './modules/general';

async function initializeApp() {
  // 加载侧边栏
  const sidebarHTML = await loadHTML('/src/partials/sidebar.html');
  document.getElementById('sidebar-container')!.innerHTML = sidebarHTML;

  // 加载默认内容（通用设置）
  await loadContent('general');

  // 初始化功能
  setupSidebar();
  setupGeneral();
}


document.addEventListener('DOMContentLoaded', initializeApp);