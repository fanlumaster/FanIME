// HTML 加载工具
export async function loadHTML(url: string): Promise<string> {
  const response = await fetch(url);
  return await response.text();
}

// 只显示当前的模块，隐藏其他模块
export function showOnlyCurrentModule(moduleName: string): void {
  const modules = ['general', 'appearance', 'input', 'helpcode'];
  modules.forEach((module: string) => {
    if (module === moduleName) {
      document.getElementById(module)!.style.display = 'block';
    } else {
      document.getElementById(module)!.style.display = 'none';
    }
  });
}