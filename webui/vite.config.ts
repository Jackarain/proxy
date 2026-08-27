import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import path from "node:path";

// 构建产物直接输出到 apps/launcher/webui（launcher 编译期内嵌目录）。
// base 用相对路径，保证在 launcher 任意子路径下都能正确加载资源。
export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
  base: "./",
  server: {
    port: 5173,
    // 开发模式代理：请求转发到本地 launcher（默认 0.0.0.0:18080）。
    proxy: {
      "/api": {
        target: "http://127.0.0.1:18080",
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: "../apps/launcher/webui",
    emptyOutDir: true,
    sourcemap: false,
  },
});
