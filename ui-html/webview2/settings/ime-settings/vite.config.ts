import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

const rootDir = dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  base: "./",
  plugins: [
    {
      name: "inline-first-paint-partials",
      transformIndexHtml(html) {
        const partials = resolve(rootDir, "src/partials");
        const sidebar = readFileSync(resolve(partials, "sidebar.html"), "utf8");
        const appearance = readFileSync(resolve(partials, "appearance.html"), "utf8");
        return html
          .replace("<!--INLINE_SIDEBAR-->", sidebar)
          .replace("<!--INLINE_APPEARANCE-->", appearance);
      },
    },
  ],
});
