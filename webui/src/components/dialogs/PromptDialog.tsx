import { useEffect, useRef, useState } from "react";
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { useDialogs } from "@/store/dialogs";

// 通用输入弹窗（替代 window.prompt）：Enter 确认、Esc 取消、点击遮罩取消、
// 打开时自动聚焦并全选。返回 Promise<string|null>。
export default function PromptDialog() {
  const prompt = useDialogs((s) => s.prompt);
  const closePrompt = useDialogs((s) => s.closePrompt);
  const [value, setValue] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);
  const open = !!prompt;

  useEffect(() => {
    if (!prompt) return;
    setValue(prompt.opts.value || "");
    const id = requestAnimationFrame(() => {
      const el = inputRef.current;
      if (el) {
        el.focus();
        el.select();
      }
    });
    return () => cancelAnimationFrame(id);
  }, [prompt]);

  const confirm = () => closePrompt(value);
  const cancel = () => closePrompt(null);

  return (
    <Dialog open={open} onOpenChange={(o) => !o && cancel()}>
      <DialogContent
        className="w-[400px] max-w-[92vw]"
        onOpenAutoFocus={(e) => e.preventDefault()}
      >
        <DialogHeader>
          <DialogTitle className="whitespace-pre-line leading-relaxed">
            {prompt?.title ?? ""}
          </DialogTitle>
        </DialogHeader>
        <Input
          ref={inputRef}
          type={prompt?.opts.password ? "password" : "text"}
          placeholder={prompt?.opts.placeholder || "输入内容"}
          value={value}
          onChange={(e) => setValue(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") confirm();
          }}
        />
        <DialogFooter>
          <Button onClick={cancel}>取消</Button>
          <Button variant="primary" onClick={confirm}>
            确定
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
