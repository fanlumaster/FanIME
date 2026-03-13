import { defineConfig } from "vite";

export default defineConfig({
  // Use relative asset paths so WebView2 can load from file://
  base: "./",
  // plugins: [
  //   {
  //     name: "strip-crossorigin",
  //     transformIndexHtml(html) {
  //       return html.replace(/\s+crossorigin(=(["']).*?\2)?/g, "");
  //     },
  //   },
  // ],
});
