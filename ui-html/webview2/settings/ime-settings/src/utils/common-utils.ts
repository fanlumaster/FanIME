// HTML 加载工具
export async function loadHTML(url: string): Promise<string> {
  const response = await fetch(url);
  return await response.text();
}